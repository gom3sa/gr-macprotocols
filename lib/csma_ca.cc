#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/block_detail.h>
#include <boost/circular_buffer.hpp>
#include "csma_ca.h"
#include <boost/thread.hpp>
#include <unistd.h>
#include <string>
#include <cstdlib>

#define BUFF_SIZE 64
#define MAX_TRIES 5
#define RxPHYDelay 1 // (us) for max distance of 300m between nodes
#define THRESHOLD -80 // Empirical

using namespace gr::macprotocols;

class csma_ca_impl : public csma_ca {

	boost::shared_ptr<gr::thread::thread> thread;

	boost::circular_buffer<buffer> cb{BUFF_SIZE}; // Circular Buffer: frames to be sent are stored here.
	boost::mutex mu1;

	public:
		csma_ca_impl(int slot_time, int sifs, int difs) : gr::block(
							"csma_ca",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							slot_time(slot_time), sifs(sifs), difs(difs), frame_id(0) {
			// Inputs
			message_port_register_in(pmt::mp("frame from app"));
			set_msg_handler(pmt::mp("frame from app"), boost::bind(&csma_ca_impl::frame_from_app, this, _1));

			message_port_register_in(pmt::mp("frame from phy"));
			set_msg_handler(pmt::mp("frame from phy"), boost::bind(&csma_ca_impl::frame_from_phy, this, _1));

			//message_port_register_in(pmt::mp("cs in"));
			//set_msg_handler(pmt::mp("cs in"), boost::bind(&csma_ca_impl::cs_in, this, _1));

			// Outputs
			message_port_register_out(pmt::mp("frame to phy"));

			//message_port_register_out(pmt::mp("mac out"));

			//message_port_register_out(pmt::mp("cs out"));

			// Threads
			//thread = boost::shared_ptr<gr::thread::thread>(new gr::thread::thread(boost::bind(&csma_ca_impl::eval_buffer, this)));

		}

		void frame_from_app(pmt::pmt_t frame) {
			buffer b; 
			b.frame = frame;
			b.attempts = 0;

			b.tod = clock::now();
			cb.push_back(b); send_frame(cb[0]);
		}

		void eval_buffer() {
			int ack_timeout = slot_time + sifs + RxPHYDelay;
			float duration; 
			clock_type tod;

			while(true) {
				if(cb.empty()) usleep(100);

				else {
					try {
						while(cb[0].attempts < MAX_TRIES) {
							send_frame(cb[0]);
							tod = cb[0].tod;
							do {
								duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - tod).count();
							} while(duration < ack_timeout); // Waiting for ack.
						}

						if(cb[0].attempts >= MAX_TRIES) {
							std::cout << std::endl << "ACK timeout! Frame is dropped." << std::endl;
							boost::unique_lock<boost::mutex> lock(mu1); // In order to remove cb[0]
							cb.pop_front();
							lock.unlock();
						}
					} catch(...) { std::cout << std::endl << "Exception has happened!" << std::endl;}
				}
			}
		}

		void send_frame(buffer &b_frame) {
			b_frame.attempts++;
			// Check if channel is busy. If not, send; otherwise, backoff.
			b_frame.tod = clock::now();
			message_port_pub(pmt::mp("frame to phy"), b_frame.frame);
		}

		void frame_from_phy(pmt::pmt_t frame) {
			// Check if it is an ack. If so, remove frame from buffer
			float duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - cb[0].tod).count();
			std::cout << std::endl << "Frame acked! ACK time = " << duration << " (us)" << std::endl;
			boost::unique_lock<boost::mutex> lock(mu1); // In order to remove cb[0]
			cb.pop_front();
			lock.unlock();
		}

	private:
		int slot_time, sifs, difs, frame_id;
};

csma_ca::sptr
csma_ca::make(int slot_time, int sifs, int difs) {
	return gnuradio::get_initial_sptr(new csma_ca_impl(slot_time, sifs, difs));
}

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/block_detail.h>
#include "csma_ca.h"
#include <boost/thread.hpp>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <stdlib.h>
#include <time.h>

#define MAX_TRIES 5
#define RxPHYDelay 1 // (us) for max distance of 300m between nodes
#define THRESHOLD -80 // Empirical

using namespace gr::macprotocols;

class csma_ca_impl : public csma_ca {

	boost::shared_ptr<gr::thread::thread> thread_check_buff, thread_send_frame;
	boost::mutex mu1;

	bool status = false; // FALSE: iddle, no frame to send; TRUE: busy, trying to send a frame. 
	bool frame_acked = false;

	public:
		csma_ca_impl(int slot_time, int sifs, int difs, int alpha) : gr::block(
							"csma_ca",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							slot_time(slot_time*alpha), sifs(sifs*alpha), difs(difs*alpha), alpha(alpha), frame_id(0) {
			// Inputs
			message_port_register_in(pmt::mp("frame from app"));
			set_msg_handler(pmt::mp("frame from app"), boost::bind(&csma_ca_impl::frame_from_app, this, _1));

			message_port_register_in(pmt::mp("frame from phy"));
			set_msg_handler(pmt::mp("frame from phy"), boost::bind(&csma_ca_impl::frame_from_phy, this, _1));

			// Outputs
			message_port_register_out(pmt::mp("frame to phy"));
			message_port_register_out(pmt::mp("frame request"));

			thread_check_buff = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&csma_ca_impl::check_buff, this)));
		}

		void check_buff() {
			while(true) {
				if(!status) { // This means no frame has arrived to be sent. So, it will request one.
					message_port_pub(pmt::mp("frame resquest"), pmt::string_to_symbol("get frame"));
				}
				usleep((rand() % 5)*(slot_time + sifs + difs)); srand(time(NULL));
			}
		}

		void frame_from_app(pmt::pmt_t frame) {
			std::cout << std::endl << "New frame from app" << std::endl;

			if(!status) {
				status = true; // This means that there is already one frame to be sent. No more frames will be requested meanwhile.

				thread_send_frame = boost::shared_ptr<gr::thread::thread> (new gr::thread::thread(boost::bind(&csma_ca_impl::send_frame, this, frame)));
			}
			
			/*buffer b; 
			b.frame = frame;
			b.attempts = 0;

			b.tod = clock::now();
			cb.push_back(b); send_frame(cb[0]);*/
		}

		void send_frame(pmt::pmt_t frame) {
			std::cout << std::endl << "Sending frame..." << std::endl;
			frame_acked = false;
			// TODO: Implementation of: 1) Carrier sense feature, 2) Backoff
			message_port_pub(pmt::mp("frame to phy"), frame);
			frame_acked = true;
			/*b_frame.attempts++;
			// Check if channel is busy. If not, send; otherwise, backoff.
			b_frame.tod = clock::now();
			message_port_pub(pmt::mp("frame to phy"), b_frame.frame);*/
		}

		void frame_from_phy(pmt::pmt_t frame) {
			// Check if it is: 1) an ack, 2) a frame
			// If an ack
				frame_acked = true; status = false;

			/*// Check if it is an ack. If so, remove frame from buffer
			float duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - cb[0].tod).count();
			std::cout << std::endl << "Frame acked! ACK time = " << duration << " (us)" << std::endl;
			boost::unique_lock<boost::mutex> lock(mu1); // In order to remove cb[0]
			cb.pop_front();
			lock.unlock();*/
		}

	private:
		int slot_time, sifs, difs, frame_id, alpha;
};

csma_ca::sptr
csma_ca::make(int slot_time, int sifs, int difs, int alpha) {
	return gnuradio::get_initial_sptr(new csma_ca_impl(slot_time, sifs, difs, alpha));
}

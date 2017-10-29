#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/block_detail.h>
#include <pmt/pmt.h>
#include <boost/circular_buffer.hpp>
#include <chrono>
#include <macprotocols/csma_ca.h>
#include <boost/thread.hpp>
#include <unistd.h>

#define BUFF_SIZE 64
#define MAX_TRIES 5
#define RxPHYDelay 1 // (us) for max distance of 300m between nodes

using namespace gr::macprotocols;

class buffer {

	typedef std::chrono::high_resolution_clock::time_point clock_type;

	public:
		pmt::pmt_t frame;
		clock_type toa; // Time Of Arrival
		clock_type tod; // Time Of Departure
		uint attempts = 0;
		bool acked = false;
};

class csma_ca_impl : public csma_ca {

	typedef std::chrono::high_resolution_clock clock;
	typedef std::chrono::high_resolution_clock::time_point clock_type;

	boost::circular_buffer<buffer> cb{BUFF_SIZE};

	boost::shared_ptr<gr::thread::thread> t_eval_buffer;
	boost::mutex mu;

	public:
		csma_ca_impl(int slot_time, int sifs, int difs) : gr::block(
							"csma_ca",
							gr::io_signature::make(0, 0, 0),
							gr::io_signature::make(0, 0, 0)),
							slot_time(slot_time), sifs(sifs), difs(difs), frame_id(0) {

			// Inputs
			message_port_register_in(pmt::mp("mac in"));
			set_msg_handler(pmt::mp("mac in"), boost::bind(&csma_ca_impl::mac_in, this, _1));

			message_port_register_in(pmt::mp("phy in"));
			set_msg_handler(pmt::mp("phy in"), boost::bind(&csma_ca_impl::phy_in, this, _1));

			// Outputs
			message_port_register_out(pmt::mp("phy out"));

			message_port_register_out(pmt::mp("mac out"));

			// Threads
			std::cout << "Thread started" << std::endl;
			t_eval_buffer = boost::shared_ptr<gr::thread::thread>
				(new gr::thread::thread(boost::bind(&csma_ca_impl::eval_buffer, this)));
		}

		void mac_in(pmt::pmt_t frame) {
			message_port_pub(pmt::mp("phy out"), frame);
			buffer b;
			b.frame = frame;
			b.toa = clock::now();
			b.attempts = 0;
			b.acked = false;
			boost::unique_lock<boost::mutex> lock(mu);
			cb.push_back(b);
			lock.unlock();

			std::cout << "Buffer size = " << cb.size() << "/ " << cb.capacity() << std::endl;
			if(cb.size() == cb.capacity())
				std::cout << "Buffer is full!!" << std::endl;
		}

		void phy_in(pmt::pmt_t frame) {
			std::cout << "hello phy_in";
			message_port_pub(pmt::mp("mac out"), frame);
		}

		void eval_buffer() {
			clock_type now;
			float duration;
			int ack_timeout = slot_time + sifs + RxPHYDelay;
			while(true) {
				//now = clock::now();
				//usleep(1);
				//duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - now).count();
				//std::cout << "sleep time (us) = " << duration << std::endl;
				//std::cout << cb.size() << std::endl;
				for(int i = 0; i < cb.size(); i++) { 
					boost::unique_lock<boost::mutex> lock(mu);
					if(cb[i].attempts == 0) {
						//std::cout << "Fresh frame " << i << std::endl;
						cb[i].tod = clock::now();
						cb[i].attempts++;
					}
					else if(cb[i].attempts < MAX_TRIES and cb[i].acked == false) {
						now = clock::now();
						duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(now - cb[i].tod).count();
						if(duration >= ack_timeout) {
							//std::cout << "Frame " << i << " will be retransmitted" << std::endl;
							cb[i].tod = clock::now();
							cb[i].attempts++;
						}
					}
					else if(cb[i].attempts == MAX_TRIES and cb[i].acked == false) {
						now = clock::now();
						duration = (float) std::chrono::duration_cast<std::chrono::microseconds>(now - cb[i].tod).count();
						if(duration >= ack_timeout) {
							//std::cout << "Max attempts over. Frame will be dropped" << std::endl;
							cb[i].attempts++;
						}
						
					}
					lock.unlock();
				}
				
				while(!cb.empty() and cb[0].attempts > MAX_TRIES) {
					//std::cout << "Drop frame" << std::endl;
					cb.pop_front();
				}
				std::cout << std::flush;
			}
		}

	private:
		int slot_time, sifs, difs;
		uint frame_id; 
};

csma_ca::sptr
csma_ca::make(int slot_time, int sifs, int difs) {
	return gnuradio::get_initial_sptr(new csma_ca_impl(slot_time, sifs, difs));
}
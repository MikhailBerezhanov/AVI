#include <csignal>

extern "C"{
#include "unistd.h"
}

#include "utils/fs.hpp"
#include "tests.hpp"

#ifndef _SHARED_LOG
Logging BaseTest::logger{MSG_DEBUG, ""};
#endif

std::string BaseTest::cwd_;
std::atomic<int> BaseTest::sig_num{0};
std::function<void(void)> BaseTest::interrupt_hook = nullptr;

BaseTest::BaseTest(const std::string &type): test_type_(type)
{
	cwd_ = utils::get_cwd();
	signal_handlers_init({SIGINT, SIGQUIT, SIGTERM});
}

void BaseTest::signal_handlers_init(std::initializer_list<int> signals)
{
	sig_num.store(0);

	auto signal_handler = [](int num){ sig_num.store(num); };

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;        
	sigemptyset(&act.sa_mask);     

	for(const auto sig : signals){
		sigaddset(&act.sa_mask, sig);
		sigaction(sig, &act, nullptr);
	}                                        
}

void BaseTest::wait(int sec)
{
	if(sec > 0){
		for(int i = 0; i < sec; ++i){
			sleep(1);

			// waiting interrupted
			if(sig_num.load()){

				if(interrupt_hook){
					interrupt_hook();
				}

				exit(0);
			} 
		}

		// waiting finished
		return;
	}
	
	// wait for a signal forever if seconds <= 0
	for(;;){
		sleep(10);

		if(sig_num.load()){

			if(interrupt_hook){
				interrupt_hook();
			}

			break;
		}
	}
}
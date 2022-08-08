#include <chrono>

#define LOG_MODULE_NAME		"[ BGT ]"
#include "logger.hpp"

#include "bg_task.hpp"


// Сон с ожиданием сигналов 
Background_task::signal Background_task::sleep(std::chrono::milliseconds period)
{
	using namespace std::chrono;

	milliseconds elapsed_ms(0);

	while(elapsed_ms < period)
	{
		std::this_thread::sleep_for(this->tick_ms);
		elapsed_ms += this->tick_ms;

		std::lock_guard<std::mutex> lck(sig_mtx);
		if(sig != signal::SLEEP) {
			signal tmp = sig;
			sig = signal::SLEEP;
			return tmp;
		}
	}

	return Background_task::signal::SLEEP;
}

void Background_task::start(bool exec)
{
	// Если уже запущен - повторно не запускаем
	if( this->t.joinable() ){
		// printf("task %s is already running\n", this->get_name().c_str());
		return;
	} 
	// printf("starting task %s\n", this->get_name().c_str());
	// Возвращаем состояние сигнала в исходное
	this->send_signal(signal::SLEEP);

	this->exec_at_start = exec;
	if(period_ms <= std::chrono::milliseconds(0)) return;

	// Обертка для запуска пользовательской функции внутри цикла фоновой задачи
	auto wrapper = [this]()
	{
		for(;;){
			if( !exec_at_start && (this->sleep(this->period_ms) == signal::STOP) ){
				return;
			} 

			try{
				if(this->main_func() == signal::STOP){
					return;
				}
			}
			catch(const std::exception &e){
				log_excp("%s: %s\n", this->task_name, e.what());
			}

			if( exec_at_start && (this->sleep(this->period_ms) == signal::STOP) ){
				return;
			} 
		}
	};

	this->t = std::thread(wrapper);
} 

#ifdef _BG_TASK_TEST

#include <cinttypes>
#include <iostream>

#define LOG_MODULE_NAME		"[ BG ]"
#include "logger.hpp"

using namespace std;


class Custom_bg_task final : public Background_task
{
public:

	void init(int c){
		cnt = c;
	}

private:
	int cnt = 0;

	void main_func(void) override {
		log_msg(MSG_DEBUG, "Custom_bg_task::main_func() [%d]\n", cnt++);
	}
};


int main(int argc, char *argv[])
{
	logger.init(MSG_TRACE, "", 1, 1000);

	// Background_task bg_tsk;
	Custom_bg_task bg_tsk;
	bg_tsk.period_sec = 10;
	bg_tsk.init(10);

	try{
		bg_tsk.start();

		log_msg(MSG_DEBUG, "[M] main going to sleep\n");
		this_thread::sleep_for(chrono::seconds(2));

		log_msg(MSG_DEBUG, "[M] main sending wake-up signal\n");
		bg_tsk.send_signal(Background_task::signal::WAKE_UP);
		this_thread::sleep_for(chrono::seconds(4));

		log_msg(MSG_DEBUG, "[M] main sending stop signal to backgrond task\n");

		bg_tsk.send_signal(Background_task::signal::STOP);
		bg_tsk.wait();
		bg_tsk.wait();
		bg_tsk.wait();

		bg_tsk.cancel();
		// std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	catch(const std::runtime_error& e){
	    cerr << "Error: " << e.what() << endl;
	    return 1;
	}

	return 0;
}

#endif
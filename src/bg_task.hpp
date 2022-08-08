/*============================================================================== 
Описание: 	Модуль реализации фоновой задачи (потока).

Автор: 		berezhanov.m@gmail.com
Дата:		16.12.2021
Версия: 	1.0
==============================================================================*/

#ifndef _BG_TASK_HPP_ 
#define _BG_TASK_HPP_

#include <mutex>
#include <thread>
#include <atomic>

class Background_task
{
public:

	Background_task(const std::string &name = ""): task_name(name) {}
	virtual ~Background_task(){this->cancel();}

	enum class signal
	{
		SLEEP = 0,
		WAKE_UP,	// Вызвать main_func принудительно
		STOP 		// Остановить мягко (после завершения main_func)
	};

	// Основная функция задачи
	virtual signal main_func(void) { return signal::SLEEP; }

	void start(bool exec = false);

	virtual void stop(){ send_signal(signal::STOP); }

	virtual void wake_up() { send_signal(signal::WAKE_UP); }

	void send_signal(signal val){
		std::lock_guard<std::mutex> lck(this->sig_mtx);
		this->sig = val;
	}

	signal get_current_state() const {
		std::lock_guard<std::mutex> lck(this->sig_mtx);
		return this->sig;
	}

	// Ожидание завершения при мягкой остановке
	virtual void wait(){
		if( !this->t.joinable() ){
			return;
		} 
		this->t.join();
	}

	// Остановить жестко, не дожидаясь завершения
	virtual void cancel(){
		if( !this->t.joinable() ){
			return;
		} 
		pthread_cancel(t.native_handle());
		t.join();
	}

	void set_period(int sec){
		using namespace std::chrono;
		if(sec <= 0) return;
		std::lock_guard<std::mutex> lck(this->period_mtx);
		this->period_ms = duration_cast<milliseconds>( seconds(sec) );
	}

	void set_period(std::chrono::milliseconds ms, std::chrono::milliseconds tick_ms = std::chrono::milliseconds(250)){
		std::lock_guard<std::mutex> lck(this->period_mtx);
		this->period_ms = ms; 
		this->tick_ms = tick_ms;
	}

	std::chrono::seconds get_period_sec() const { 
		using namespace std::chrono;
		std::lock_guard<std::mutex> lck(this->period_mtx);
		auto period_sec = duration_cast<seconds>(this->period_ms);
	 	return period_sec;
	}

	std::string get_name() const noexcept {
		return this->task_name;
	}

	// Сон с ожиданием сигналов 
	signal sleep(std::chrono::milliseconds period);

private:
	mutable std::mutex sig_mtx;
	signal sig = signal::SLEEP;

	bool exec_at_start = false;

	mutable std::mutex period_mtx;
	std::chrono::milliseconds period_ms{0};	// Периодичность запуска основной функции в фоне
	std::chrono::milliseconds tick_ms{250};	// Периодичность пробуждений внутри sleep для проверка сигналов

	std::thread t;
	std::string task_name;
};

#endif

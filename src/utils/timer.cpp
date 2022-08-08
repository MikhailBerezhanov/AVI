// #include <ctime>
#include "utility.hpp"
#include "timer.hpp"

namespace utils{

void Timer::timeout_handler(union sigval param)
{
	Timer *obj = static_cast<Timer*>(param.sival_ptr);

	timeout_cb on_timeout = nullptr;

	// Колбек может отрабатываться долго - работаем с копией указателя
	// чтобы не ждать завершения обработки в случае замены функции
	{
		std::lock_guard<std::mutex> lock(obj->mutex_);
		on_timeout = obj->on_timeout_;
	}
	
	if( on_timeout ){
		on_timeout();
	}
}

// Создание таймера 
void Timer::create(timeout_cb on_timeout)
{
	std::lock_guard<std::mutex> lock(mutex_);

	if(timer_id_){
		return;
	}

	struct sigevent sev;
	struct itimerspec spec;
	memset(&sev, 0, sizeof(sev));
	memset(&spec, 0, sizeof(spec));

	sev.sigev_notify = SIGEV_THREAD; 	// sigev_notify_function выполняется в отдельном потоке
	sev.sigev_notify_function = Timer::timeout_handler;
	sev.sigev_value.sival_ptr = this;	// Передаем указатель на текущий объект таймера в колбек

	if(timer_create(CLOCK_REALTIME, &sev, &this->timer_id_) < 0){
		throw std::runtime_error(excp_func(std::string("timer_create() failed: ") + strerror(errno)));
	}

	on_timeout_ = on_timeout;
}

// Сброс таймера (Запуск таймера)
void Timer::reset(int period_sec)
{
	struct itimerspec spec;
	memset(&spec, 0, sizeof(spec));

	if(period_sec){
		this->is_set.store(true);
	}
	else{
		this->is_set.store(false);
	}

	spec.it_value.tv_sec = period_sec;
  	spec.it_value.tv_nsec = 0;
  	spec.it_interval.tv_sec = 0;
	spec.it_interval.tv_nsec = 0;

	std::lock_guard<std::mutex> lock(mutex_);
	if( !this->timer_id_ ){
		return;
	}

	if(timer_settime(this->timer_id_, 0, &spec, nullptr) < 0){
		throw std::runtime_error(excp_func(std::string("timer_settime() failed: ") + strerror(errno)));
	} 
}

void Timer::destroy()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if(this->timer_id_){
		timer_delete(this->timer_id_);
		this->timer_id_ = 0;
	} 
}


} //namespace utils
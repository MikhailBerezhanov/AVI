#pragma once

#include <csignal>
#include <functional>
#include <atomic>
#include <mutex>

namespace utils{

class Timer
{
	using timeout_cb = std::function<void()>;
public:

	Timer(timeout_cb cb = nullptr){
		if(cb){
			this->create(cb);
		}
	}

	void create(timeout_cb cb);
	void set_timeout_callback(timeout_cb cb){
		std::lock_guard<std::mutex> lock(mutex_);
		on_timeout_ = cb;
	}

	void reset(int period_sec);	// 0 - stop

	void destroy();

	std::atomic<bool> is_set{false};

private:
	mutable std::mutex mutex_;
	timer_t timer_id_ = 0;
	timeout_cb on_timeout_ = nullptr;
	
	static void timeout_handler(union sigval param);
};

} //namespace
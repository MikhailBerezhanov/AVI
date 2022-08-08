#pragma once

#include <mutex>
#include <stdexcept>
#include <functional>
#include <array>
#include <thread>

namespace hw{

class GPIO
{
public:
	GPIO(const char *pin): pin_(pin){}

	GPIO(GPIO &&other) 
	{
		std::lock_guard<std::mutex> rhs_lock(other.mutex_);
		pin_ = std::move(other.pin_);
	}

	bool get_value() const;
	void set_value(bool val) const;
	const char* get_pin() const { return pin_; }

	void export_pin() const;
	void set_direction(const char *dir) const;

	// "edge" ... reads as either "none", "rising", "falling", or
	// 		"both". Write these strings to select the signal edge(s)
	// 		that will make poll(2) on the "value" file return.
	// 		Only for the pin that can be configured as an
	// 		interrupt generating input pin.
	void set_edge(const char *edge) const;

	// "active_low" ... reads as either 0 (false) or 1 (true). Write
	// 		any nonzero value to invert the value attribute both
	// 		for reading and writing. Existing and subsequent
	// 		poll(2) support configuration via the edge attribute
	// 		for "rising" and "falling" edges will follow this
	// 		setting.
	void set_active_low(bool value = true) const;

private:
	mutable std::mutex mutex_;
	const char *pin_ = nullptr;

	bool pin_control(const char *driver_path, const char *ctrl, bool rw) const;
};




class LED: public GPIO
{
public:
	LED(const char *pin): GPIO(pin) {}
	LED(LED &&other) = default;

	void init();
	void blink(int period_ms) const;
private:

};

class Button: public GPIO
{
public:
	using callback = std::function<void(void)>;

	Button(const char *pin, const char *edge = "falling");
	Button(Button &&other) = default;
	~Button(){ reset(); }

	void reset();
	void set_long_press_threshold(double sec);
	void set_callbacks(callback short_press, callback long_press = nullptr);
	bool get_edge() const { return edge_; }

private:
	bool edge_ = false; // falling
	std::thread irq_thread_;

	double long_press_sec_ = 2.0;

	callback short_press_cb_ = nullptr;
	callback long_press_cb_ = nullptr;
	// callback double_press_cb_ = nullptr;

	static void interrupt_handler(const Button *btn);

	inline void execute_callback(const callback &cb) const {
		if(cb){
			cb();
		}
	}
};

} // namespace hw

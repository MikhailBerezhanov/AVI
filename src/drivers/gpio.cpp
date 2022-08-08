#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

extern "C"{
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>	
}

#include "gpio.hpp"

#define SYSFS_PATH 			"/sys/class/gpio"


namespace hw{

// ----- Common GPIO interface -----

bool GPIO::pin_control(const char *driver_path, const char *ctrl, bool rw = false) const
{
	errno = 0;
	bool value = true;
	int open_mode = rw ? O_RDONLY : O_WRONLY;

	std::lock_guard<std::mutex> lock(mutex_);
	int fd = open(driver_path, open_mode);

	if(fd < 0){
		throw std::runtime_error(std::string("open ") + driver_path + " failed: " + strerror(errno));
	}

	if( !rw ){
		if(write(fd, ctrl, strlen(ctrl)) < 0){
			// pin might have this estting already

			if(errno != EBUSY){ 
				close(fd);
				throw std::runtime_error(std::string("write ") + driver_path + " failed: " + strerror(errno));
			}
		}
	}
	else{
		char val[2] = {0};
		if(read(fd, val, 2) < 0){
			close(fd);
			throw std::runtime_error(std::string("read ") + driver_path + " failed: " + strerror(errno));
		}

		value = (val[0] == '0') ? false : true;
	}

	close(fd);
	return value;
}

void GPIO::export_pin() const
{
	const char *path = SYSFS_PATH "/export";
	pin_control(path, pin_);
}

void GPIO::set_direction(const char *dir) const
{
	char path[64] = {0};
	snprintf(path, sizeof(path), SYSFS_PATH "/gpio%s/direction", pin_);
	pin_control(path, dir);
}	

void GPIO::set_value(bool val) const
{
	char path[64] = {0};
	snprintf(path, sizeof(path), SYSFS_PATH "/gpio%s/value", pin_);
	const char *value = val ? "1" : "0";

	pin_control(path, value);
}

bool GPIO::get_value() const
{
	char path[64] = {0};
	snprintf(path, sizeof(path), SYSFS_PATH "/gpio%s/value", pin_);
	return pin_control(path, "", true);
}

void GPIO::set_edge(const char *edge) const
{
	char path[64] = {0};
	snprintf(path, sizeof(path), SYSFS_PATH "/gpio%s/edge", pin_);
	pin_control(path, edge);
}

void GPIO::set_active_low(bool value) const
{
	char path[64] = {0};
	snprintf(path, sizeof(path), SYSFS_PATH "/gpio%s/active_low", pin_);
	const char *val = value ? "1" : "0";
	pin_control(path, val);
}


//  ----- LED implementation ----- 
void LED::init()
{
	export_pin(); 
	set_direction("out");
}


//  ----- Button implementation ----- 
Button::Button(const char *pin, const char *edge): GPIO(pin)
{
	export_pin();
	set_direction("in");
	// enable interrupt at button release
	set_edge(edge);	
	edge_ = (strcmp(edge, "falling") == 0) ? false : true;
	set_active_low(false);
}

void Button::reset()
{
	if(irq_thread_.joinable()){
		pthread_cancel(irq_thread_.native_handle());
		irq_thread_.join();
	}
}

void Button::set_long_press_threshold(double sec)
{
	if(sec <= 0.0){
		return;
	}

	reset();
	long_press_sec_ = sec;
}

void Button::set_callbacks(callback short_press, callback long_press) 
{ 
	reset();
	short_press_cb_ = short_press; 
	long_press_cb_ = long_press;
	// double_press_cb_ = double_press;

	if(short_press_cb_ || long_press_cb_){
		std::thread tmp{interrupt_handler, this};
		irq_thread_ = std::move(tmp);
	}
}

void Button::interrupt_handler(const Button *btn)
{
	using namespace std::chrono;
	time_point<high_resolution_clock> start;
	time_point<high_resolution_clock> end;

	errno = 0;
	struct pollfd fdset;
	int rc = 0;
	char value = 0;
	bool double_click = false;

	char path[64] = {0};
	snprintf(path, sizeof(path), SYSFS_PATH "/gpio%s/value", btn->get_pin());
	int gpio_fd = open(path, O_RDONLY | O_NONBLOCK);

	if(gpio_fd < 0){
		std::cerr << "open " << path << " failed: " << strerror(errno) << std::endl;
		return;
	}

	// fd must be read before any new poll to prevent false interrupt
	if(read(gpio_fd, &value, 1) < 0){
		close(gpio_fd);
		std::cerr << "read " << path << " failed: " << strerror(errno) << std::endl;
		return;
	}

	for(;;) {
		memset((void*)&fdset, 0, sizeof(fdset));

		fdset.fd = gpio_fd;
		fdset.events = POLLPRI;

		rc = poll(&fdset, 1, -1);      

		if(rc < 0) {
			close(gpio_fd);
			std::cerr << "poll " << path << " failed: " << strerror(errno) << std::endl;
			return;
		}
      
		if(rc == 0) {
			// timeout
			continue;
		}
            
		if(fdset.revents & POLLPRI) {

			// button chatter protection
			std::this_thread::sleep_for(std::chrono::milliseconds(50));

			lseek(fdset.fd, 0, SEEK_SET);
			if(read(gpio_fd, &value, 1) < 0){
				close(gpio_fd);
				std::cerr << "read " << path << " failed: " << strerror(errno) << std::endl;
				return;
			}

			if(value == '0'){
				// pressed
				start = high_resolution_clock::now();
			}

			if(value == '1'){
				// released
				end = high_resolution_clock::now(); 

				auto time_span = duration_cast<duration<double>>(end - start);

				if(time_span.count() < btn->long_press_sec_){
					btn->execute_callback(btn->short_press_cb_);
				}
				else{
					btn->execute_callback(btn->long_press_cb_);
				}
			}

			// if(!btn->get_edge() && value == '0'){
			// 	btn->execute_callback();
			// }

			// if(btn->get_edge() && value == '1'){
			// 	btn->execute_callback();
			// }

		}
	}
}

} // namespace hw

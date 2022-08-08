#pragma once

#include <string>
#include <mutex>
#include <thread>
#include <functional>

namespace hw{

class UART
{
public:
	UART(const std::string &port_name): port_name_(port_name) {}

	// uses 8N1 mode
	void init(int baudrate = 115200);
	void deinit();

	void write(const uint8_t *buf, size_t count) const;
	size_t read(uint8_t *buf, size_t count) const;

	void listen(long no_data_timeout_sec) const;

	void set_data_received_callback(std::function<void(void)> cb);

private:
	mutable std::recursive_mutex port_mutex_;
	std::string port_name_;
	int port_fd_ = -1;

	std::function<void(void)> data_received_callback_ = nullptr;

	std::thread main_thread_;
	void main_thread_func();
	void main_thread_shutdown();
};

} // namespace hw
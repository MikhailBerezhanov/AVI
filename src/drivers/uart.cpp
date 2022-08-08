
#include <cstring>
#include <cerrno>

extern "C"{
#include <unistd.h> 
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
}

#include "uart.hpp"

namespace hw{

static speed_t select_speed(int baudrate)
{
	if(baudrate < 1800){
		return B1200;
	}
	else if(baudrate < 2400){
		return B1800;
	}
	else if(baudrate < 4800){
		return B2400;
	}
	else if(baudrate < 9600){
		return B4800;
	}
	else if(baudrate < 19200){
		return B9600;
	}
	else if(baudrate < 38400){
		return B19200;
	}
	else if(baudrate < 57600){
		return B38400;
	}
	else if(baudrate < 115200){
		return B57600;
	}
	else if(baudrate < 230400){
		return B115200;
	}
	else if(baudrate < 460800){
		return B230400;
	}
	else if(baudrate < 500000){
		return B460800;
	}
	else if(baudrate < 576000){
		return B500000;
	}
	else if(baudrate < 921600){
		return B576000;
	}
	else{
		return B921600;
	}
}


void UART::init(int baudrate)
{
	std::lock_guard<std::recursive_mutex> lck(port_mutex_);

	struct termios options;
	memset(&options, 0, sizeof(struct termios));

	port_fd_ = open(port_name_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
	if(port_fd_ < 0){
		throw std::runtime_error("open '" + port_name_ + "' failed: " + strerror(errno));
	}

	fcntl(port_fd_, F_SETFL, 0);

	tcgetattr(port_fd_, &options);
	speed_t bspd = select_speed(baudrate);
	cfsetispeed(&options, bspd);
	cfsetospeed(&options, bspd);

	options.c_cflag |= (CLOCAL | CREAD);

	// Raw data stream
	cfmakeraw(&options);

	// Timeout: 1 sec
	options.c_cc[VMIN]  = 0;
	options.c_cc[VTIME] = 10;

	// No parity check, 8 bytes, 1 stop bit (8N1)
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;

	if(tcsetattr(port_fd_, TCSANOW, &options) < 0){
		throw std::runtime_error(std::string("set port parameters failed: ") + strerror(errno));    
	}

	if(tcflush(port_fd_, TCIOFLUSH) < 0){
		throw std::runtime_error(std::string("port flush failed: ") + strerror(errno));
	}
}

void UART::deinit()
{
	main_thread_shutdown();

	std::lock_guard<std::recursive_mutex> lck(port_mutex_);
	if(port_fd_ >= 0){
		close(port_fd_);
		// port_ready.store(false);
		port_fd_ = -1;
	}
}

size_t UART::read(uint8_t *buf, size_t count) const
{
	std::lock_guard<std::recursive_mutex> lck(port_mutex_);
	ssize_t bytes_read_count = ::read(port_fd_, buf, count);

	if(bytes_read_count < 0){
		// this->serial_port_ready.store(false);
		throw std::runtime_error(std::string("UART::read failed: ") + strerror(errno));
	}

	return static_cast<size_t>(bytes_read_count);
}

void UART::write(const uint8_t *buf, size_t count) const
{
	std::lock_guard<std::recursive_mutex> lck(port_mutex_);
	ssize_t bytes_written_count = ::write(port_fd_, buf, count);

	if(bytes_written_count < 0){
		// this->serial_port_ready.store(false);
		throw std::runtime_error(std::string("UART::write failed: ") + strerror(errno));
	}

	if(static_cast<size_t>(bytes_written_count) != count){
		throw std::runtime_error(std::string("UART::write written count: ") + 
			std::to_string(bytes_written_count) + " less that count: " + std::to_string(count));
	}

	// waits until all output written to the object referred
    // to by fd has been transmitted
	tcdrain(port_fd_);
}

void UART::listen(long no_data_timeout_sec) const
{
	// std::array<uint8_t, PVC_PCKT_DATA_SIZE + 10> buf{};
	ssize_t res = 0;
	
	{
		// std::lock_guard<std::recursive_mutex> lck(port_mutex_);
		fd_set fs;
		FD_ZERO(&fs);
		FD_SET(port_fd_, &fs);

		struct timeval timeout;

		// Data waiting timeout
		timeout.tv_sec = no_data_timeout_sec;
		timeout.tv_usec = 0;

		// Blocks after select call
		
		res = select(port_fd_ + 1 , &fs, NULL, NULL, &timeout);

		if(res < 0){
			throw std::runtime_error(std::string("UART::listen select() failed: ") + strerror(errno));
		}
		else if( !FD_ISSET(port_fd_, &fs) ){
			// select() timeout - no data in port
			printf("select timedout\n");

			// tcflush(port_fd_, TCIOFLUSH);
			return;
		}

		// Data is ready for read
		if(data_received_callback_){
			data_received_callback_();
		}

	}
}

void UART::main_thread_func()
{
	for(;;){
		// printf("calling listen\n");
		this->listen(1);
	}
}

void UART::main_thread_shutdown()
{
	if(main_thread_.joinable()){
		pthread_cancel(main_thread_.native_handle());
		main_thread_.join();
	}
}

void UART::set_data_received_callback(std::function<void(void)> cb)
{
	// Stop existing thread (if already exists)
	main_thread_shutdown();

	data_received_callback_ = cb;

	// Start new thread
	std::thread t([this](){ main_thread_func(); });
	main_thread_ = std::move(t);
}

} // namespace hw
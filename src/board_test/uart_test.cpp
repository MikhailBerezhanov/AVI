
#include "logger.hpp"
#include "drivers/hardware.hpp"
#include "uart_test.hpp"

void UARTTest::init() const 
{
	Hardware::enable_uart();
	Hardware::uart->init(115200);

	interrupt_hook = [](){ Hardware::uart->deinit(); };
}


void UARTTest::deinit() const 
{
	Hardware::uart->deinit();
}

void UARTTest::loopback(const std::vector<std::string> &params)
{
	logger.msg(MSG_DEBUG, _BOLD "NOTE: Connect UART TX with RX" _RESET "\n");

	auto data_read = [](){ 

		try{
			uint8_t buf[512];
			memset(buf, 0, sizeof(buf));

			size_t bytes_read = Hardware::uart->read(buf, sizeof(buf));

			logger.msg(MSG_DEBUG, "UART received %zu bytes: '%s'\n", bytes_read, buf);
		}
		catch(const std::exception &e){
			logging_excp(logger, "%s\n", e.what());
		}
		
	};

	Hardware::uart->set_data_received_callback( data_read );

	int counter = 1;

	for(;;){
		std::string msg = "test uart message #" + std::to_string(counter);
		++counter;

		logger.msg(MSG_DEBUG, "UART write: %s\n", msg);
		Hardware::uart->write(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());

		wait(7);
	}

}

void UARTTest::send(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::runtime_error("No text provided for sending");
	}

	const std::string &text = params[0] + "\n";

	Hardware::uart->write(reinterpret_cast<const uint8_t*>(text.c_str()), text.size());
}

void UARTTest::listen(const std::vector<std::string> &params)
{
	auto data_read = [](){ 

		try{
			uint8_t buf[512];
			memset(buf, 0, sizeof(buf));

			size_t bytes_read = Hardware::uart->read(buf, sizeof(buf));

			logger.msg(MSG_DEBUG, "UART received %zu bytes: '%s'\n", bytes_read, buf);
		}
		catch(const std::exception &e){
			logging_excp(logger, "%s\n", e.what());
		}
		
	};

	Hardware::uart->set_data_received_callback( data_read );

	wait(-1);
}
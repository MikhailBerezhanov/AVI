#pragma once

#include "tests.hpp"

class UARTTest: public BaseTest
{
public:
	UARTTest(const std::string &type = "UARTTest"): BaseTest(type){
		table_.insert({"loopback", {loopback, "check board TX -> RX line"}});
		table_.insert({"send", {send, "<text> sends text via TX"}});
		table_.insert({"listen", {listen, "waits for data from RX"}});
	}

	void init() const override;
	void deinit() const override;

private:
	static void loopback(const std::vector<std::string> &params);
	static void send(const std::vector<std::string> &params);
	static void listen(const std::vector<std::string> &params);
};
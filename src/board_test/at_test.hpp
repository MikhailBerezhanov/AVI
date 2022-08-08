#pragma once

#include "tests.hpp"


class ATTest: public BaseTest
{
public:
	ATTest(const std::string &type = "ATTest"): BaseTest(type){
		table_.insert({"info", {[this](const std::vector<std::string> &params){ this->info(params); }, 
			"- get module info"}});
		table_.insert({"send", {send, "<command> - AT+ command"}});
		table_.insert({"imei", {get_imei, "- get module IMEI"}});
		table_.insert({"gps", {get_GPS, "- get current GPS data"}});
	}

	void init() const override;
	void deinit() const override;

private:
	void info(const std::vector<std::string> &params);
	static void send(const std::vector<std::string> &params);
	static void get_imei(const std::vector<std::string> &params);
	static void get_GPS(const std::vector<std::string> &params);
};
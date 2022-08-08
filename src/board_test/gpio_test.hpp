#pragma once

#include "tests.hpp"

class GPIOTest: public BaseTest
{
public:
	GPIOTest(const std::string &type = "SDcardTest"): BaseTest(type){
		table_.insert({"led", {led_set, "<led_num> [value] - set/get LED value (led_num: 1,2)"}});
		table_.insert({"btn", {process_buttons, "- activate buttons polling"}});
		// table_.insert({"check", {check_written_files, ""}});
	}

private:
	static void led_set(const std::vector<std::string> &params);
	static void process_buttons(const std::vector<std::string> &params);
	// static void check_written_files(const std::vector<std::string> &params);
};
#pragma once

#include "tests.hpp"

class SDcardTest: public BaseTest
{
public:
	SDcardTest(const std::string &type = "SDcardTest"): BaseTest(type){
		table_.insert({"copy", {copy, "<filename> [num] - copy _filename_ _num_ times from SD"}});
		table_.insert({"write", {write, "[num] [size] - write _num_ files of _size_ KB to SD"}});
		table_.insert({"check", {check_written_files, "- check written files MD5 sum"}});
	}

private:
	static void copy(const std::vector<std::string> &params);
	static void write(const std::vector<std::string> &params);
	static void check_written_files(const std::vector<std::string> &params);
};
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "nmea_parser.hpp"


class GPS_Generator
{
public:
	void load_route(const std::string &file_path = "output.rmc");
	RMC_data get_route_point(int *index = nullptr);
	void refresh();
	void reset(int index = 0, int step = 1);

	void reverse_route();
	size_t route_size() const { return cache_.size(); }

private:
	mutable std::recursive_mutex mutex_;
	int cache_index_ = 0;
	int cache_step_ = 1;
	std::vector<RMC_data> cache_;	// route cache (array of pionts)
	NMEA_Parser parser_;
};
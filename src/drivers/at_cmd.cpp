
#include <stdexcept>
#include <cstring>
#include <vector>
#include <sstream>

extern "C" {
#include <ATControl.h>
}

#include "at_cmd.hpp"

namespace hw{

// class static members
std::mutex AT_port::mutex_;
std::atomic<bool> AT_port::inited_{false};
std::atomic<bool> AT_port::gps_is_on{false};

void AT_port::init()
{
	std::lock_guard<std::mutex> lck(mutex_);

	if(inited_.load()){
		return;
	}

	if(int ret = atctrl_init() < 0){
		throw std::runtime_error(std::string("atctrl_init failed: ") + std::to_string(ret));
	}

	inited_.store(true);
}

void AT_port::deinit()
{
	if( !inited_ ){
		return;
	}

	std::lock_guard<std::mutex> lck(mutex_);
	atctrl_deinit();
	inited_.store(false);
}


std::string AT_port::send(const std::string &command, const std::string &expected_answer, long timeout_ms)
{
	char command_echo[100] = {0};
	std::array<char, 1024> answer{};

	{
		std::lock_guard<std::mutex> lck(mutex_);
		if(sendATCmd((char*)(command.c_str()), command_echo, answer.data(), answer.size(), timeout_ms) < 0){
			
			if(errno != 0){
				throw std::runtime_error(command + ": sendAT_port() failed - " + strerror(errno));
			}
			
		}
	}

	// printf("command echo: %s\n", command_echo);

	if( !strstr(answer.data(), expected_answer.c_str()) ){
		throw std::runtime_error(command + ": expected_answer (" + expected_answer + ") not received");
	}

	return answer.data();
}


std::string AT_port::get_IMEI()
{
	std::array<char, 100> answer{};

	if(getIMEI(answer.data(), answer.size()) < 0){
		throw std::runtime_error("getIMEI() failed");
	}

	return answer.data();
}

std::string AT_port::get_module_revision()
{
	return AT_port::send("AT+SIMCOMATI", "");
}

std::string AT_port::find_answer_data(const std::string &answer, const std::string &key)
{
	std::string::size_type pos = answer.find(key);

	if(pos == std::string::npos){
		throw std::runtime_error("no '" + key + "' found at the answer (" + answer + ")");
	}

	// shift pos to answer data start
	pos += key.size();
	std::string::size_type end_pos = answer.find_first_of('\r', pos);
	std::string data = answer.substr(pos, end_pos - pos);

	return data;  
}

// ---- GPS methods -----

// input format: 
// "5548.228488,N,03748.839542,E,130522,093037.0,420.6,0.0,0.0"
// ",,,,,,,,"
// "0.0,"
void GPSinfo::parse_from_string(const std::string &s)
{
	std::array<size_t, 8> positions; // holds all the positions that sub occurs within str
	valid_ = false;
	size_t idx = 0;

	std::string::size_type pos = s.find(",", 0);
	while((pos != std::string::npos) && (idx < positions.size())){
		positions[idx] = pos;
		pos = s.find(",", pos + 1);
		++idx;
	}

	if(idx != positions.size()){
		// printf("GPSinfo::parse_from_string: invalid format (%s)\n", s.c_str());
		return;
	}

	// Latitude of current position. Output format is ddmm.mmmmmm
	latitude_ = s.substr(0, positions[0]);

	if(latitude_.empty() || (latitude_.size() < 5)){
		// printf("GPSinfo::parse_from_string: invalid latitude (%s)\n", s.c_str());
		return;
	}

	north_south_ = s.substr(positions[0] + 1, 1);

	longitude_ = s.substr(positions[1] + 1, positions[2] - positions[1] - 1);

	if(longitude_.empty() || (longitude_.size() < 6)){
		// printf("GPSinfo::parse_from_string: invalid longitude (%s)\n", s.c_str());
		return;
	}

	east_west_ = s.substr(positions[2] + 1, 1);
	date_ = s.substr(positions[3] + 1, positions[4] - positions[3] - 1);

	if(date_.empty() || (date_.size() < 6)){
		// printf("GPSinfo::parse_from_string: invalid date (%s)\n", s.c_str());
		return;
	}

	utc_time_ = s.substr(positions[4] + 1, positions[5] - positions[4] - 1);

	if(utc_time_.empty() || (utc_time_.size() < 8)){
		// printf("GPSinfo::parse_from_string: invalid utc_time (%s)\n", s.c_str());
		return;
	}

	std::string altitude = s.substr(positions[5] + 1, positions[6] - positions[5] - 1);
	std::string speed = s.substr(positions[6] + 1, positions[7] - positions[6] - 1);
	std::string course = s.substr(positions[7] + 1);

	if( north_south_.empty() || east_west_.empty() || altitude.empty() || speed.empty() || course.empty() )
	{
		// printf("GPSinfo::parse_from_string: empty fields (%s)\n", s.c_str());
		return;
	}

	try{
		altitude_ = std::stod(altitude);
		speed_ = std::stod(speed);
		course_ = std::stod(course);
	}
	catch(const std::exception &e){
		// printf("GPSinfo::parse_from_string: conversion error (%s): %s\n", s.c_str(), e.what());
		return;
	}

	if(AT_port::gps_is_on.load()){
		valid_ = true;
	}
}

std::string GPSinfo::as_string(bool verbose) const
{
	std::ostringstream out;
	out.precision(6);
	out << "vld: " << valid_ << " lat: " << std::fixed << latitude_ << "[" << north_south_ << "] long: " << longitude_
		<< "[" << east_west_ << "] date: " << date_ << " utc_tm: " << utc_time_;

	if(verbose){
		out.precision(1);
		out << " alt: " << std::fixed << altitude_ << " spd: " << speed_ << " crs: " << course_;
	}
	
	return out.str();
}

double GPSinfo::NMEA_to_dec_degrees(const std::string &nmea, const std::string &quadrant) const
{
	double dec_res = 0.0;

	if( !valid_ ){
		return dec_res;
	}

	try{
		size_t int_len = (nmea.at(4) == '.') ? 2 : 3;

		std::string int_part = nmea.substr(0, int_len);
		std::string min_part = nmea.substr(int_len);

		dec_res = std::stod(int_part) + std::stod(min_part) / 60.;
		if(quadrant == "S" || quadrant == "W"){
			dec_res *= -1;
		}

		return dec_res;
	}
	catch(const std::exception &e){
		throw std::runtime_error("GPSinfo::NMEA_to_dec_degrees(" + nmea + ":" + quadrant + ") failed: " + e.what());
	}
}

std::pair<double, double> GPSinfo::get_decimal_coordinates() const 
{
	std::pair<double, double> lat_lon;

	lat_lon.first = NMEA_to_dec_degrees(latitude_, north_south_);
	lat_lon.second = NMEA_to_dec_degrees(longitude_, east_west_);

	return lat_lon;
}



bool AT_port::check_GPS_status()
{
	std::string answer = send("AT+CGPS?");
	std::string data = find_answer_data(answer, "+CGPS: ");

	if(data[0] != '1'){
		AT_port::gps_is_on.store(false);
		return false;
	}

	AT_port::gps_is_on.store(true);
	return true;
}

void AT_port::enable_GPS(bool increased_update_rate)
{ 
	// Check if GPS module already on
	bool on = check_GPS_status();

	if( !on ){

		if(increased_update_rate){
			// NMEA rate to 10 Hz (update period = 100ms)
			send("AT+CGPSNMEARATE=1");	
		}
		
		// Start GPS
		send("AT+CGPS=1");			
	}

	AT_port::gps_is_on.store(true);
}

void AT_port::disable_GPS() 
{ 
	send("AT+CGPS=0"); 
	AT_port::gps_is_on.store(false);
}

GPSinfo AT_port::get_GPSinfo()
{
	std::string answer = send("AT+CGPSINFO");
	std::string info = find_answer_data(answer, "+CGPSINFO: ");

	return GPSinfo(info);
}

} // namespace hw


#ifdef _AT_CMD_TEST

#include <iostream>

using namespace hw;

int main(int argc, char* argv[])
{
	// AT_port cmd; // deleted
	std::string cmd;
	if(argc == 2){
		cmd = argv[1];
	}

	try{
		AT_port::init();

		if(cmd == "imei"){
			std::cout << "IMEI: " << AT_port::get_IMEI() << std::endl;
		}
		else if(cmd == "gps"){
			// AT_port::enable_GPS();
			AT_port::check_GPS_status();
			auto info = AT_port::get_GPSinfo();
			std::cout << info.as_string(true) << std::endl;

			auto coord = info.get_decimal_coordinates();
			std::cout.precision(6);
			std::cout << "decimal: " << std::fixed << coord.first << " " << coord.second << std::endl;
		}
		else{
			std::string answer = AT_port::send("AT" + cmd, "", 3000);
			std::cout << answer << std::endl;
		}
		
		AT_port::deinit();
	}
	catch(const std::exception &e){
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
#endif




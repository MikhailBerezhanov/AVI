
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
			throw std::runtime_error(command + ": sendAT_port() failed - " + strerror(errno));
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

	std::string data = answer.substr(pos , end_pos - pos);
	return data;  
}

// ---- GPS methods -----

// string format: "5548.228488,N,03748.839542,E,130522,093037.0,420.6,0.0,0.0"
void GPSinfo::parse_from_string(const std::string &s)
{
	std::vector<size_t> positions; // holds all the positions that sub occurs within str

	std::string::size_type pos = s.find(",", 0);
	while(pos != std::string::npos){
		positions.push_back(pos);
		pos = s.find(",", pos + 1);
	}

	if(positions.size() != 8){
		throw std::runtime_error("invalid GPSinfo string format");
	}

	// Latitude of current position. Output format is ddmm.mmmmmm
	latitude_ = s.substr(0, positions[0]);

	if(latitude_.empty()){
		// Coordinates are not valid
		return;
	}

	north_south_ = s.substr(positions[0] + 1, positions[1] - positions[0] - 1);
	longitude_ = s.substr(positions[1] + 1, positions[2] - positions[1] - 1);
	east_west_ = s.substr(positions[2] + 1, positions[3] - positions[2] - 1);
	date_ = s.substr(positions[3] + 1, positions[4] - positions[3] - 1);
	utc_time_ = s.substr(positions[4] + 1, positions[5] - positions[4] - 1);
	std::string altitude = s.substr(positions[5] + 1, positions[6] - positions[5] - 1);
	std::string speed = s.substr(positions[6] + 1, positions[7] - positions[6] - 1);
	std::string course = s.substr(positions[7] + 1);

	// printf("lat: %s, n/s: %s, long: %s, e/w: %s, date: %s, time: %s, alt: %s, speed: %s, course: %s\n",
	// 	latitude.c_str(), north_south_.c_str(), longitude.c_str(), east_west_.c_str(), this->date_.c_str(), 
	// 	this->utc_time_.c_str(), altitude.c_str(), speed.c_str(), course.c_str());

	altitude_ = std::stod(altitude);
	speed_ = std::stod(speed);
	course_ = std::stod(course);

	if(AT_port::gps_is_on.load()){
		valid_ = true;
	}
}

std::string GPSinfo::as_string(bool verbose) const
{
	std::ostringstream out;
	out.precision(6);
	out << "valid: " << valid_ << " lat: " << std::fixed << latitude_ << "," << north_south_ << " long: " << longitude_
		<< "," << east_west_ << " date: " << date_ << " utc_time: " << utc_time_;

	if(verbose){
		out.precision(1);
		out << " alt: " << std::fixed << altitude_ << " speed: " << speed_ << " course: " << course_;
	}
	
	return out.str();
}

double GPSinfo::NMEA_to_dec_degrees(const std::string &nmea, const std::string &quadrant) const
{
	size_t int_len = (nmea.at(4) == '.') ? 2 : 3;

	std::string int_part = nmea.substr(0, int_len);
	std::string min_part = nmea.substr(int_len);

	double dec = std::stod(int_part) + std::stod(min_part) / 60.;
	if(quadrant == "S" || quadrant == "W"){
		dec *= -1;
	}

	return dec;
}

std::pair<double, double> GPSinfo::get_decimal_coordinates() const 
{
	double lat_dec = NMEA_to_dec_degrees(latitude_, north_south_);
	double long_dec = NMEA_to_dec_degrees(longitude_, east_west_);

	return {lat_dec, long_dec};
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

void AT_port::enable_GPS()
{ 
	// check if GPS module already on
	bool on = check_GPS_status();

	if( !on ){
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




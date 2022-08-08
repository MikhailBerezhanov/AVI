#include <sstream>
#include <stdexcept>

#include "nmea_parser.hpp"

using namespace std;


// Checks if GGA sentence is valid.
bool NMEA_Parser::is_valid_GGA(const string &GGASentence) const
{
	vector<std::string> elementVector = split_by_comma(GGASentence);

	if( (elementVector[0] != "$GPGGA") ||    
		(elementVector.size() != 15) ||
		(std::stoi(elementVector[6]) == 0) || 
		(std::stoi(elementVector[7]) == 0) )
	{
		return false;
	}

	return true;
}

// Input: GGA NMEA_Parser sentence
// Output: set values in class.
// void NMEA_Parser::set_values_GGA(const string &GGA)
// {
// 	vector<std::string> elementVector = split_by_comma(GGA);
// 	// Assert we have a GGA sentence
// 	assert(elementVector[0] == "$GPGGA");

// 	this->UTC = atoi(elementVector[1].c_str());
// 	this->latitude = get_coordinates(elementVector[2]);
// 	if (elementVector[3] == "S") this->latitude  = -this->latitude;
// 	this->longitude = get_coordinates(elementVector[4]);
// 	if (elementVector[5] == "W") this->longitude  = -this->longitude;
// 	this->altitude = std::stod(elementVector[9]);
// 	this->numberSatellites = atoi(elementVector[7].c_str());
// }


// RMC sentence

std::ostream& operator<< (std::ostream &os, const RMC_data &data)
{
	os << "valid: " << data.valid << " utc_time: " << data.utc_time << " date: " << data.date << 
		" lat: " << data.latitude << " lon: " << data.longitude << " speed: " << data.speed <<
		" course: " << data.course;

	return os;
}

// Check if RMC sentence is valid with NMEA_Parser standard.
bool NMEA_Parser::is_valid_RMC(const string &RMCSentence, vector<std::string> *elements) const
{
	vector<std::string> elementVector = split_by_comma(RMCSentence);

	if(elementVector[0] != "$GPRMC"){
		throw std::runtime_error("Invalid RMC sentence beginning (" + elementVector[0] + ")");
	}

	if(elementVector.size() != 12){
		throw std::runtime_error("Invalid RMC sentence size (" + RMCSentence + ")");
	}

	if(elements){
		*elements = std::move(elementVector);
	}

	return true;
}

RMC_data NMEA_Parser::parse_RMC(const string &RMCSentence) const
{
	vector<std::string> elementVector;

	is_valid_RMC(RMCSentence, &elementVector);

	RMC_data output;

	output.date = elementVector[9];
	output.utc_time = elementVector[1];
	output.latitude = get_dec_coordinate(elementVector[3], elementVector[4]);
	output.longitude = get_dec_coordinate(elementVector[5], elementVector[6]);
	output.speed = std::stod(elementVector[7]);
	output.course = std::stod(elementVector[8]);
	output.valid = (elementVector[2] == "A") ? true : false;

	return output;
}

/*-----Auxiliary functions-----*/

// Input: coma separated string
// Output: Vector with all the elements in input.
vector<string> NMEA_Parser::split_by_comma(const string &input) const
{
	vector<string> returnVector;
	stringstream ss(input);
	string element;

	while(std::getline(ss, element, ',')) {
		returnVector.push_back(element);
	}

	return returnVector;
}

double NMEA_Parser::degrees_to_decimal(int degrees, double minutes, int seconds)
{
	return static_cast<double>(degrees) + minutes / 60.0 + static_cast<double>(seconds) / 3600.0;
}

double NMEA_Parser::get_dec_coordinate(const std::string &nmea_coordinate, const std::string &quadrant) const
{
	size_t int_len = (nmea_coordinate.at(4) == '.') ? 2 : 3;

	std::string degree_part = nmea_coordinate.substr(0, int_len);
	std::string minutes_part = nmea_coordinate.substr(int_len);

	double dec_degrees = degrees_to_decimal(std::stoi(degree_part), std::stod(minutes_part));
	if(quadrant == "S" || quadrant == "W"){
		dec_degrees *= -1;
	}

	return dec_degrees;
}

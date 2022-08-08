#pragma once

#include <vector>
#include <string>
#include <iostream>

// Knots to Kilometers / Hour convertion
constexpr double knots_to_kmh(double knots)
{
	return knots * 1.852;
}

// Knots to Meters / Sec convertion
constexpr double knots_to_mps(double knots)
{
	return knots * 0.514444;
}

struct RMC_data
{
	std::string utc_time;
	std::string date;
	double latitude = 0.0;	// in decimal format
	double longitude = 0.0;	// in decimal format
	double speed = 0.0;		// in knots
	double course = 0.0;	// in degrees (0 - to the North, 90 - East, 180 - South, 270 - West)
	bool valid = false;

	friend std::ostream& operator<< (std::ostream &os, const RMC_data &data);
};

// TODO
struct GGA_data
{

};

// TODO
struct GSA_data
{

};

class NMEA_Parser
{
public:

	// GGA sentences 
	bool is_valid_GGA(const std::string &GGASentence) const;
	
	// RMC sentences
	bool is_valid_RMC(const std::string &RMCSentence, std::vector<std::string> *elements = nullptr) const;
	RMC_data parse_RMC(const std::string &RMCSentence) const;

private:
	// Auxualiary functions
	std::vector<std::string> split_by_comma(const std::string&) const;
	double get_dec_coordinate(const std::string &nmea_coordinate, const std::string &quadrant) const;

	static double degrees_to_decimal(int degrees, double minutes, int seconds = 0);
};



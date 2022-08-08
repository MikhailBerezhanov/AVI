/*============================================================================== 
Описание: 	Модуль вызова АТ команд SIMCOM

Автор: 		berezhanov.m@gmail.com
Дата:		21.04.2022
Версия: 	1.0
==============================================================================*/

#pragma once

#include <string>
#include <mutex>
#include <atomic>

namespace hw{

struct GPSinfo
{
	GPSinfo() = default;
	GPSinfo(const std::string &s){ parse_from_string(s); }

	using nmea_fmt = std::pair<std::string, std::string>;  // value , direction (N\S | E\W)

	// Check if data is actual and can be used
	bool is_valid() const { return valid_; }

	// Default format is NMEA <latitude, longitude>
	std::pair<nmea_fmt, nmea_fmt> get_NMEA_coordinates() const {
		return { {latitude_, north_south_}, {longitude_, east_west_} };
	}

	// Convertion to degrees with minutes <latitude, longitude>
	std::pair<double, double> get_decimal_coordinates() const;

	std::string get_date() const { return date_; }
	std::string get_utc_time() const { return utc_time_; }
	double get_altitude() const { return altitude_; }
	double get_speed() const { return speed_; }
	double get_course() const { return course_; }

	void parse_from_string(const std::string &s);
	std::string as_string(bool verbose = false) const;

private:
	std::string latitude_;		// Latitude of current position. Output format is ddmm.mmmmmm
	std::string north_south_; 	// North/South Indicator, 'N' = North, 'S' = South
	std::string longitude_;		// Longitude of current position. Output format is dddmm.mmmmmmm
	std::string east_west_;		// Eest/West Indicator, 'E' = East, 'W' = West
	std::string date_; 			// Date. Output format is ddmmyy
	std::string utc_time_; 		// UTC Time. Output format is hhmmss.s
	double altitude_ = 0; 		// MSL Altitude. Unit in meters
	double speed_ = 0; 			// Speed Over Ground. Unit in knots.
	double course_ = -1; 		// Course. Degrees. 0 - to North, 90 - East, 180 - South, 270 - West.
	bool valid_ = false;		// Data validness

	double NMEA_to_dec_degrees(const std::string &nmea, const std::string &quadrant) const;
};


class AT_port
{
public:
	// AT_port() = delete;

	void init();
	void deinit();

	std::string send(
		const std::string &command, 
		const std::string &expected_answer = "", 
		long timeout_ms = 1000);

	std::string find_answer_data(const std::string &answer, const std::string &key);

	std::string get_IMEI();
	std::string get_module_revision();

	void enable_GPS();
	void disable_GPS();
	bool check_GPS_status();

	GPSinfo get_GPSinfo();

	static std::atomic<bool> gps_is_on;

private:
	static std::mutex mutex_;
	static std::atomic<bool> inited_;
};

} // namespace hw

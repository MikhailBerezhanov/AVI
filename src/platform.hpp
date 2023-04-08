#pragma once

#include <string>
#include "gps_gen.hpp"			// RMC_data
#include "drivers/lcd1602.hpp"	// LCD1602::Alignment
#include "drivers/gpio.hpp"		// hw::Button::callback

namespace platform
{

using button_callback = hw::Button::callback;

// GPS данные
struct GPS_data
{
	GPS_data() = default;
	GPS_data(const std::string &dtime, 
				 const std::pair<double, double> &dec_lat_lon, 
				 double spd,
				 double crs,
				 bool vld = false):
		date_time(dtime), lat_lon(dec_lat_lon), speed_kmh( knots_to_kmh(spd) ), course(crs), valid(vld) {}

	GPS_data(const RMC_data &rmc): lat_lon({rmc.latitude, rmc.longitude}), speed_kmh( knots_to_kmh(rmc.speed) ),
		course(rmc.course), valid(rmc.valid) { date_time = rmc.date + "_" + rmc.utc_time; }

	GPS_data(const std::string &str){ this->from_string(str); }

	std::string to_string() const
	{
		char buf[128] = {0};
		snprintf(buf, sizeof(buf), "vld: %d, lat: %.6lf, long: %.6lf, crs: %.2lf, spd: %.2lf", 
			static_cast<int>(this->valid), this->lat_lon.first, this->lat_lon.second, this->course, this->speed_kmh);

		std::string res{buf};

		return res;
	}

	void from_string(const std::string &str)
	{
		int tmp = 0;

		int res = sscanf(str.c_str(), "vld: %d, lat: %lf, long: %lf, crs: %lf, spd: %lf", 
			&tmp, &this->lat_lon.first, &this->lat_lon.second, &this->course, &this->speed_kmh);

		if(res <= 0){
			throw std::runtime_error(excp_method("failed to parse " + str));
		}

		this->valid = static_cast<bool>(tmp);
	}

	std::string date_time;
	std::pair<double, double> lat_lon{0.0, 0.0};	// in decimal format
	double speed_kmh = 0.0;	// in kilometers per hour
	double course = 0.0;	// in degrees (0 - to the North, 90 - East, 180 - South, 270 - West)
	bool valid = false;
};

void init(int gps_poll_period_ms, const std::string &gps_generator_file = "");
bool ready();
void deinit();

std::string get_IMEI();
GPS_data get_GPS_data();

void set_LED(bool enable);

void audio_play(const std::string &mp3);
bool audio_is_playing();
void audio_setup_stop_callback(std::function<void(void)> func);
void audio_set_gain_level(int value);
int audio_get_gain_level();
void audio_stop();

// Buttons ids (3 buttons available in current revision)
typedef enum
{
	S1 = 0,
	S2 = 1,
	S3 = 2,
}button_t;

void set_button_cb(button_t id, button_callback short_press, button_callback long_press = nullptr);
void set_button_long_press_threshold(button_t id, double sec);
void set_buttons_long_press_threshold(double sec);

void lcd_backlight(bool on_off);
bool lcd_get_backlight_state();
void lcd_print(const std::string &str, LCD1602::Alignment align = LCD1602::Alignment::NO);
void lcd_print(char ch);
void lcd_print_with_padding(const std::string &str, char symb = ' ');
uint8_t lcd_get_num_rows();
uint8_t lcd_get_num_cols();
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_clear();
void lcd_home();
void lcd_add_custom_char(uint8_t location, const uint8_t *charmap);
void lcd_print_custom_char(uint8_t location);

} // namespace platform


#include <stdexcept>

#include "hardware.hpp"
#include "logger.hpp"
#include "at_test.hpp"

void ATTest::init() const
{
	Hardware::enable_AT();
	Hardware::AT->init();
}

void ATTest::deinit() const
{
	Hardware::AT->deinit();
}

void ATTest::info(const std::vector<std::string> &params)
{
	logger.msg(MSG_DEBUG, "IMEI: %s\n", Hardware::AT->get_IMEI());
	logger.msg(MSG_DEBUG, "Module Revision: %s\n", Hardware::AT->get_module_revision());
	logger.msg(MSG_DEBUG, "Software release: %s\n", Hardware::AT->send("AT+GMR"));
	logger.msg(MSG_DEBUG, "CQCNV version: %s\n", Hardware::AT->send("AT+CQCNV"));

	logger.msg(MSG_DEBUG, "SIM card access: %s\n", Hardware::AT->send("AT+CPIN?"));
	logger.msg(MSG_DEBUG, "SIM card ICCID: %s\n", Hardware::AT->send("AT+CICCID"));
	logger.msg(MSG_DEBUG, "SIM card signal: %s\n", Hardware::AT->send("AT+CSQ"));

	logger.msg(MSG_DEBUG, "Power Voltage: %s\n", Hardware::AT->send("AT+CBC"));

	logger.msg(MSG_DEBUG, "CPU Temp: %s\n", Hardware::AT->send("AT+CPMUTEMP"));

	logger.msg(MSG_DEBUG, "ADC1 value: %s\n", Hardware::AT->send("AT+CADC=2"));
	logger.msg(MSG_DEBUG, "ADC2 value: %s\n", Hardware::AT->send("AT+CADC2=2"));

	logger.msg(MSG_DEBUG, "GPS status: %s\n", Hardware::AT->check_GPS_status());

	std::string s = Hardware::AT->send("AT+CGPSINFO");
	logger.msg(MSG_DEBUG, "GPS data: %s\n", s);
	logger.hex_dump(MSG_DEBUG, s.c_str(), s.length(), "GPS data as hex_dump: ");

	// Hardware::AT->get_IMEI();
	// Hardware::AT->get_module_revision();
	// Hardware::AT->send("AT+GMR");
	// Hardware::AT->send("AT+CQCNV");

	// Hardware::AT->send("AT+CPIN?");
	// Hardware::AT->send("AT+CICCID");
	// Hardware::AT->send("AT+CSQ");

	// Hardware::AT->send("AT+CBC");

	// Hardware::AT->send("AT+CPMUTEMP");

	// Hardware::AT->send("AT+CADC=2");
	// Hardware::AT->send("AT+CADC2=2");

	// Hardware::AT->check_GPS_status();
	// Hardware::AT->send("AT+CGPSINFO");
}

void ATTest::send(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no AT command provided");
	}

	printf("%s", Hardware::AT->send(params[0]).c_str());
}

void ATTest::get_imei(const std::vector<std::string> &params)
{
	printf("%s\n", Hardware::AT->get_IMEI().c_str());
}

void ATTest::get_GPS(const std::vector<std::string> &params)
{
	hw::GPSinfo gps_data = Hardware::AT->get_GPSinfo();
	logger.msg(MSG_DEBUG, "%s\n", gps_data.as_string(true));
}
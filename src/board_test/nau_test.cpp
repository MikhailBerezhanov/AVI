#include "logger.hpp"
#include "drivers/hardware.hpp"
#include "nau_test.hpp"

void NAUTest::init() const
{
	Hardware::enable_nau();
}

uint16_t NAUTest::read_register(uint16_t reg_addr, const char *reg_name)
{
	uint16_t value = Hardware::nau->read_reg(reg_addr);

	logger.msg(MSG_DEBUG, "(0x%02X) '%s': 0x%04X\n", reg_addr, reg_name, value);
	return value;
}

void NAUTest::read(const std::vector<std::string> &params)
{
	logger.msg(MSG_DEBUG, "------------ POWER MANAGEMENT -----------\n");
	read_register(0x01, "Power Management 1"); //
	read_register(0x02, "Power Management 2"); //
	read_register(0x03, "Power Management 3"); //

	logger.msg(MSG_DEBUG, "------------- AUDIO CONTROL -------------\n");
	read_register(0x04, "Audio Interface"); //
	read_register(0x05, "Companding"); //
	read_register(0x06, "Clock Control 1"); //
	read_register(0x07, "Clock Control 2"); //
	read_register(0x0A, "DAC CTRL"); // 
	read_register(0x0B, "DAC Volume"); // 
	read_register(0x0E, "ADC CTRL"); // 
	read_register(0x0F, "ADC Volume"); // 

	logger.msg(MSG_DEBUG, "-------------- DAC LIMITER --------------\n");
	read_register(0x18, "DAC Limiter 1"); // 
	read_register(0x19, "DAC Limiter 2"); // 

	// logger.msg(MSG_DEBUG, "-------------- ALC CONTROL --------------\n");
	// read_register(chip, 0x20, "ALC CTRL 1");
	// read_register(chip, 0x21, "ALC CTRL 2");
	// read_register(chip, 0x22, "ALC CTRL 3");
	// read_register(chip, 0x23, "Noise Gate");

	logger.msg(MSG_DEBUG, "-------------- PLL CONTROL --------------\n");
	read_register(0x24, "PLL N CTRL"); // 
	read_register(0x25, "PLL K 1"); //
	read_register(0x26, "PLL K 2"); //
	read_register(0x27, "PLL K 3"); // 

	logger.msg(MSG_DEBUG, "----- INPUT, OUTPUT & MIXER CONTROL -----\n");
	read_register(0x28, "Attenuation CTRL");
	read_register(0x2C, "Input CTRL"); // 
	read_register(0x2D, "PGA Gain");
	read_register(0x2F, "ADC Boost");
	read_register(0x31, "Output CTRL"); // 
	read_register(0x32, "Mixer CTRL");
	read_register(0x36, "SPKOUT Volume"); // 
	read_register(0x38, "MONO Mixer Control"); // 

	logger.msg(MSG_DEBUG, "-------------- Register ID --------------\n");
	// read_register(chip, 0x3E, "Silicon Revision"); // 
	// read_register(chip, 0x3F, "2-Wire ID"); //         (0x001A)
	// read_register(chip, 0x40, "Additional ID"); //     (0x00CA)
	// read_register(chip, 0x41, "Reserved"); //          (0x0124)
	read_register(0x4E, "Control and Status"); //
	read_register(0x4F, "Output tie-off CTRL"); //
}


void NAUTest::reset(const std::vector<std::string> &params)
{
	Hardware::nau->soft_reset();
}

void NAUTest::sidetone(const std::vector<std::string> &params)
{
	Hardware::nau->MIC_sidetone();
}

void NAUTest::mic_bias(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->MIC_bias(enable);
}

void NAUTest::mout_attenuate(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->MOUT_attenuate(enable);
}

void NAUTest::mout_mute(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->MOUT_mute(enable);
}

void NAUTest::mout_boost(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->MOUT_boost(enable);
}

void NAUTest::mout_resistance(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->MOUT_add_resistance(enable);
}

void NAUTest::pga_boost(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->PGA_boost(enable);
}

void NAUTest::pga_gain(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no value provided");
	}

	uint8_t value = std::stoi(params[0]);
	Hardware::nau->PGA_set_gain(value);
}

void NAUTest::dac_gain(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no value provided");
	}

	uint8_t value = std::stoi(params[0]);
	Hardware::nau->DAC_set_volume(value);
}

void NAUTest::dac_mout(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->DAC_to_MOUT(enable);
}

void NAUTest::dac_best_SNR(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->DAC_best_SNR(enable);
}

void NAUTest::dac_soft_mute(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->DAC_soft_mute(enable);
}

void NAUTest::dac_automute(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->DAC_automute(enable);
}

void NAUTest::dac_deemp(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no value provided");
	}

	uint8_t value = std::stoi(params[0]);
	Hardware::nau->DAC_de_emphasis(value);
}

void NAUTest::dac_lim(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	Hardware::nau->PGA_boost(enable);
}

void NAUTest::dac_lim_thr(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no value provided");
	}

	int8_t value = std::stoi(params[0]);
	Hardware::nau->DAC_enable_limiter(value);
}

void NAUTest::dac_lim_boost(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no value provided");
	}

	uint16_t value = std::stoi(params[0]);
	Hardware::nau->DAC_set_limiter_boost(value);
}





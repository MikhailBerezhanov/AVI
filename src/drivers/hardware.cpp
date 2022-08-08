#include "hardware.hpp"

#define BOARD_REV_1_1

#ifdef BOARD_REV_1_1

// System devices
#define I2C_DEV				"/dev/i2c-5"
#define UART_DEV 			"/dev/ttyHS1"

// AVI Buttons gpio
#define BUTTON_S1 			"1023" 
#define BUTTON_S2 			"52"
#define BUTTON_S3 			"26"

// AVI LEDs gpio
#define LED_HL1  			"18"
#define LED_HL2  			"79"

#endif

// static memders
std::unique_ptr<hw::AT_port> Hardware::AT = nullptr;
std::unique_ptr<hw::NAU8810> Hardware::nau = nullptr;
std::unique_ptr<WH1602B_CTK> Hardware::lcd = nullptr;
std::unique_ptr<hw::Audio> Hardware::audio = nullptr;
std::unique_ptr<hw::UART> Hardware::uart = nullptr;

std::vector<hw::LED> Hardware::leds;
std::vector<hw::Button> Hardware::buttons;

const char* Hardware::get_board_revision()
{
	#ifdef BOARD_REV_1_1
	return "1.1";
	#endif

	return "";
}

void Hardware::enable_AT()
{
	std::unique_ptr<hw::AT_port> uptr {new hw::AT_port};
	AT = std::move(uptr);
}

void Hardware::enable_lcd()
{
	std::unique_ptr<WH1602B_CTK> uptr {new WH1602B_CTK{PCF8574A_ADDR}};
	lcd = std::move(uptr);
}

void Hardware::lcd_init()
{
	lcd->init(PCF8574A_ADDR, I2C_DEV);
}

void Hardware::enable_audio()
{
	std::unique_ptr<hw::Audio> uptr {new hw::Audio};
	audio = std::move(uptr);
}

void Hardware::enable_nau()
{
	std::unique_ptr<hw::NAU8810> uptr {new hw::NAU8810{I2C_DEV}};
	nau = std::move(uptr);
}

void Hardware::enable_leds()
{
	leds.emplace_back(LED_HL1);
	leds.emplace_back(LED_HL2);
}

void Hardware::enable_buttons()
{
	buttons.emplace_back(BUTTON_S1, "both"/*"falling"*/);
	buttons.emplace_back(BUTTON_S2, "both");
	buttons.emplace_back(BUTTON_S3, "both");
}

void Hardware::enable_uart()
{
	std::unique_ptr<hw::UART> uptr {new hw::UART{UART_DEV}};
	uart = std::move(uptr);
}


#pragma once

#include <memory>
#include <vector>

#include "drivers/audio.hpp"
#include "drivers/at_cmd.hpp"
#include "drivers/gpio.hpp"
#include "drivers/lcd1602.hpp"
#include "drivers/nau8810.hpp"
#include "drivers/uart.hpp"

// Board hardware units
class Hardware
{
public:
	Hardware() = delete;

	static const char* get_board_revision();

	static void enable_AT();
	static void enable_nau(); 
	static void enable_lcd();
	static void lcd_init();
	static void enable_audio();
	static void enable_uart();
	static void enable_leds();
	static void enable_buttons();

	// Disabled by default - i.e. not allocated (nullptr)
	static std::unique_ptr<hw::AT_port> AT;
	static std::unique_ptr<hw::NAU8810> nau;
	static std::unique_ptr<WH1602B_CTK> lcd;
	static std::unique_ptr<hw::Audio> audio;
	static std::unique_ptr<hw::UART> uart;

	static std::vector<hw::LED> leds;
	static std::vector<hw::Button> buttons;

private:

};
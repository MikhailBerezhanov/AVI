
#include "logger.hpp"
#include "utils/fs.hpp"
#include "drivers/i2c.hpp"
#include "drivers/hardware.hpp"
#include "lcd_test.hpp"

#define LCD_ADDR_FILE 	"lcd.addr"
#define I2C_DEV			"/dev/i2c-5"

static uint8_t lcd_addr = PCF8574A_ADDR;

void LCDTest::init() const
{
	hw::i2c_init(I2C_DEV);
	Hardware::enable_lcd();

	if(utils::file_exists(LCD_ADDR_FILE)){
		// restore address from the file
		std::string addr = utils::read_text_file_to_str(LCD_ADDR_FILE);
		lcd_addr = std::stoi(addr);
		// logger.msg(MSG_DEBUG, "Restored lcd addr: %u (0x%02x)\n", lcd_addr, lcd_addr);
	}

	Hardware::lcd->set_addr(lcd_addr);

	interrupt_hook = [](){ Hardware::lcd->clear(); };
}


void LCDTest::init_lcd(const std::vector<std::string> &params)
{
	if( !params.empty() ){
		lcd_addr = std::stoi(params[0]);
		// save lcd_addr to the file
		utils::write_text_file(LCD_ADDR_FILE, params[0] + "\n");
		logger.msg(MSG_DEBUG, "Saving lcd addr: %u (0x%02x)\n", lcd_addr, lcd_addr);
	}
	else{
		logger.msg(MSG_DEBUG, "Using default lcd addr: %u (0x%02x)\n", lcd_addr, lcd_addr);
	}

	Hardware::lcd->init(lcd_addr);
}

void LCDTest::test(const std::vector<std::string> &params)
{
	Hardware::lcd->init(lcd_addr);

	logger.msg(MSG_DEBUG, "Printing test messages...\n");

	for(int i = 0; i < 10; ++i){
		Hardware::lcd->print("АБВГДЕЖЗИЙКЛМНОП");
		Hardware::lcd->set_cursor(1, 0);
		Hardware::lcd->print("РСТУФХЦЧШЩЪЫЬЭЮЯ");
		wait(5);
		Hardware::lcd->clear();

		Hardware::lcd->print("абвгдеёжзиклмноп");
		Hardware::lcd->set_cursor(1, 0);
		Hardware::lcd->print("рстуфхцчшщъыьэюя");
		wait(5);
		Hardware::lcd->clear();

		const char *s = "bibka";
		std::string str = "Бабушка";
		int ival = 123;
		double dval = 43.567890;

		Hardware::lcd->print("%s_%lf", s, dval);
		Hardware::lcd->set_cursor(1, 0);
		Hardware::lcd->print("Wру: %s %d", str.c_str(), ival);
		wait(5);
		Hardware::lcd->clear();
	}
}

void LCDTest::user_chars(const std::vector<std::string> &params)
{
	uint8_t charmap0[8] = {
		0b11111,
		0b00001,
		0b00001,
		0b11111,
		0b10000,
		0b10000,
		0b11111,
		0b00000
	};

	uint8_t charmap1[8] = {
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b00000,
		0b00000,
		0b00000,
		0b00000
	};

	uint8_t charmap2[8] = {
		0b00100,
		0b01110,
		0b11111,
		0b00000,
		0b00000,
		0b11111,
		0b01110,
		0b00100
	};

	Hardware::lcd->user_char_create(0, charmap0);
	Hardware::lcd->user_char_create(1, charmap1);
	Hardware::lcd->user_char_create(2, charmap2);

	Hardware::lcd->clear();
	Hardware::lcd->return_home();
	Hardware::lcd->print("User characters: ");
	Hardware::lcd->set_cursor(1, 0);
	Hardware::lcd->user_char_print(0);
	Hardware::lcd->print(" ");
	Hardware::lcd->user_char_print(1);
	Hardware::lcd->print(" ");
	Hardware::lcd->user_char_print(2);
	Hardware::lcd->print(" ");
}

void LCDTest::backlight(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	logger.msg(MSG_DEBUG, "Setting lcd backlight: %s\n", enable);
	Hardware::lcd->control(enable);
}
		
void LCDTest::cursor(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	logger.msg(MSG_DEBUG, "Highlighting lcd cursor: %s\n", enable);
	Hardware::lcd->control(true, enable);
}

void LCDTest::blink(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	logger.msg(MSG_DEBUG, "Setting lcd blink: %s\n", enable);
	Hardware::lcd->control(true, true, enable);
}

void LCDTest::clear(const std::vector<std::string> &params)
{
	Hardware::lcd->clear();
}

void LCDTest::home(const std::vector<std::string> &params)
{
	Hardware::lcd->return_home();
}

void LCDTest::print(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no text provided");
	}

	Hardware::lcd->print(params[0]);
}

void LCDTest::print_wc(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::invalid_argument("no unicode provided");
	}

	wchar_t wc = std::stoi(params[0]);
	Hardware::lcd->print_ru(wc);
}

void LCDTest::set_cursor(const std::vector<std::string> &params)
{
	uint8_t row = 0;
	uint8_t col = 0;

	if(params.size() > 1){
		row = static_cast<uint8_t>(std::stoi(params[0]));
		col = static_cast<uint8_t>(std::stoi(params[1]));
	}

	Hardware::lcd->set_cursor(row, col);
}

void LCDTest::scroll(const std::vector<std::string> &params)
{
	bool left = params.empty() ? false : (params[0] == "l") ? true : false;
	logger.msg(MSG_DEBUG, "Scroll lcd: %s\n", left ? "left" : "right");
	
	if(left){
		Hardware::lcd->scroll_left();
	}
	else{
		Hardware::lcd->scroll_right();
	}
}

void LCDTest::autoscroll(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	logger.msg(MSG_DEBUG, "Setting lcd autoscroll: %s\n", enable);
	Hardware::lcd->autoscroll(enable);
}

void LCDTest::left_to_right(const std::vector<std::string> &params)
{
	bool enable = params.empty() ? true : std::stoi(params[0]);
	logger.msg(MSG_DEBUG, "Setting lcd left_to_right: %s\n", enable);
	Hardware::lcd->left_to_right(enable);
}

#pragma once

#include "tests.hpp"

class LCDTest: public BaseTest
{
public:
	LCDTest(const std::string &type = "LCDTest"): BaseTest(type){
		table_.insert({"init", {init_lcd, "[addr] - first time init LCD"}});
		table_.insert({"test", {test, "init and print test messages"}});
		table_.insert({"user_chars", {user_chars, "print user characters"}});
		table_.insert({"bl", {backlight, "[0 | 1] - eenable backlight"}});
		table_.insert({"c", {cursor, "[0 | 1] - enable cursor highlighting"}});
		table_.insert({"b", {blink, "[0 | 1] - enable blinking cursor"}});
		table_.insert({"clear", {clear, "clear screen"}});
		table_.insert({"home", {home, "return cursor to 0,0 position"}});
		table_.insert({"print", {print, "<str> - print string"}});
		table_.insert({"print_wc", {print_wc, "<unicode> - print wide char"}});
		table_.insert({"set_c", {set_cursor, "<row> <col> - set cursor position"}});
		table_.insert({"scroll", {scroll, "<l | r> - scroll screen (left or right)"}});
		table_.insert({"ascroll", {autoscroll, "<0 | 1> - enable autoscroll"}});
		table_.insert({"ltr", {left_to_right, "<0 | 1> - enable left to right printing"}});
	}

	void init() const override;

private:
	static void init_lcd(const std::vector<std::string> &params);
	static void test(const std::vector<std::string> &params);
	static void user_chars(const std::vector<std::string> &params);
	static void backlight(const std::vector<std::string> &params);
	static void cursor(const std::vector<std::string> &params);
	static void blink(const std::vector<std::string> &params);
	static void clear(const std::vector<std::string> &params);
	static void home(const std::vector<std::string> &params);
	static void print(const std::vector<std::string> &params);
	static void print_wc(const std::vector<std::string> &params);
	static void set_cursor(const std::vector<std::string> &params);
	static void scroll(const std::vector<std::string> &params);
	static void autoscroll(const std::vector<std::string> &params);
	static void left_to_right(const std::vector<std::string> &params);
};
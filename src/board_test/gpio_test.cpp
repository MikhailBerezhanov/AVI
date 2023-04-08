#include <thread>
#include <chrono>

#include "drivers/gpio.hpp"
#include "drivers/hardware.hpp"
#include "logger.hpp"
#include "gpio_test.hpp"

using namespace hw;

void GPIOTest::led_set(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::runtime_error(excp_method("no led_num provided"));
	}

	Hardware::enable_leds();

	int led_num = std::stoi(params[0]);

	if((led_num <= 0) || (led_num > Hardware::leds.size())){
		throw std::runtime_error(excp_method("unsupported led_num. Try 1 or 2"));
	}

	size_t led_idx = led_num - 1;

	if(params.size() > 1){
		bool value = std::stoi(params[1]);
		Hardware::leds[led_idx].init();
		Hardware::leds[led_idx].set_value(value);
	}
	else{
		bool value = Hardware::leds[led_idx].get_value();
		printf("%d\n", value);
	}
}

void GPIOTest::process_buttons(const std::vector<std::string> &params)
{
	// if(params.empty()){
	// 	throw std::runtime_error(excp_method("no btn_num provided"));
	// }

	// int btn_num = std::stoi(params[0]);

	Hardware::enable_buttons();

	std::array<int, 9> pressed_cnt = {0 ,0, 0};

	for(size_t i = 0; i < Hardware::buttons.size(); ++i){
		Hardware::buttons[i].set_callbacks(
			[&pressed_cnt, i](){ 
				++pressed_cnt[i];
				printf("Button %d short-press: %d\n", i + 1, pressed_cnt[i]); 
			},

			[&pressed_cnt, i](){ 
				++pressed_cnt[i + 3];
				printf("Button %d long-press: %d\n", i + 1, pressed_cnt[i + 3]); 
			}

			// [&pressed_cnt, i](){ 
			// 	++pressed_cnt[i + 6];
			// 	printf("Button %d double-press: %d\n", i + 1, pressed_cnt[i + 6]); 
			// }
		);
	}

	// Thread reset func test
	// Hardware::buttons[2].set_callbacks(nullptr);
	// Hardware::buttons[2].set_callbacks([&pressed_cnt](){ 
	// 	++pressed_cnt[2];
	// 	printf("Button 3 short-press: %d\n", pressed_cnt[2]); 
	// });

	wait(-1);
}
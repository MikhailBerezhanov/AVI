
#include <stdexcept>
#include <cinttypes>

#define LOG_MODULE_NAME		"[ MEN ]"
#include "logger.hpp"

#include "platform.hpp"
#include "app_menu.hpp"
#include "app.hpp"

// Custom characters location in LCD memory
#define ARROWS 		0
#define UP_ARROW 	1
#define DOWN_ARROW 	2

#define LAST_ROW  	( platform::lcd_get_num_rows() - 1)
#define LAST_COL 	( platform::lcd_get_num_cols() - 1)

namespace avi{

utils::Timer BaseMenu::timer_;

std::vector<std::string> BaseMenu::get_content_values(size_t from, size_t to) const
{
	if(static_cast<long>(to - from) < 0){
		throw std::invalid_argument(excp_method("'to' less than 'from'"));
	}

	std::lock_guard<std::recursive_mutex> lock(mutex_);

	auto last = content_.cend();

	// to >= from
	if((to - from) < content_.size()){
		last = content_.cbegin() + to + 1;
	}

	std::vector<std::string> res(content_.cbegin() + from, last);
	return res;
}

void BaseMenu::centrify_content_item(size_t idx)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if(idx >= content_.size()){
		return;
	}

	size_t len = number_of_symbols(content_.at(idx).c_str());

	int indent_len = (platform::lcd_get_num_cols() - len) / 2;
	if(indent_len < 0){
		return;
	} 

	content_[idx] = std::string(indent_len, ' ') + content_.at(idx);
}

void BaseMenu::centrify_content()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	for(size_t i = 0; i < content_.size(); ++i){
		this->centrify_content_item(i);
	}

	was_centrified_ = true;
}

// void BaseMenu::add_padding_after_content(char symb)
// {
// 	std::lock_guard<std::recursive_mutex> lock(mutex_);

// 	for(auto &elem : content_){
// 		size_t len = number_of_symbols(elem.c_str());
// 		int indent_len = platform::lcd_get_num_cols() - len;

// 		if(indent_len > 0){
// 			elem += std::string(indent_len, symb);
// 		}

// 	}
// }

// -----------------------------------------------------------------------------------------------

void StaticMenu::show() const
{
	size_t rows = platform::lcd_get_num_rows();

	std::vector<std::string> content = BaseMenu::get_content_values(0, rows - 1);

	for(size_t i = 0; i < content.size(); ++i){
		platform::lcd_set_cursor(i, 0);
		platform::lcd_print(content[i]);
	}
}

// -----------------------------------------------------------------------------------------------

inline size_t StaticMenuWithDynamicSymbol::get_symbol_position(uint8_t &row, uint8_t &col) const
{
	size_t symb_count = 0;
	size_t symb_pos = 0;

 	for(size_t i = 0; i < BaseMenu::get_content_size(); ++i){

 		const std::string curr_value = BaseMenu::get_content_value(i);

		size_t start_pos = curr_value.find_first_of(dyn_symdol_);
		if(start_pos == std::string::npos){
			continue;
		}

		row = static_cast<uint8_t>(i);
		// str can contain not only ASCII symbols
		col = number_of_symbols(curr_value.substr(0, start_pos).c_str());

		size_t end_pos = curr_value.find_last_of(dyn_symdol_);
		symb_count = end_pos - start_pos + 1;
		break;
	 }

 	return symb_count;
}

inline void StaticMenuWithDynamicSymbol::animate_symbol(uint8_t row, uint8_t col, size_t count) const
{
	size_t cnt = 0;
	bool reverse = true;
	for(;;){

		if(!cnt){
			cnt = count;
			reverse = !reverse;
			platform::lcd_set_cursor(row, col);
		}

		platform::lcd_print(reverse ? dyn_symdol_ : ' ');

		--cnt;

		if(LCD_Interface::wait(update_period_)){
			break;
		}
	}
}

void StaticMenuWithDynamicSymbol::show() const
{
	StaticMenu::show();

	uint8_t symb_row = 0;
	uint8_t symb_col = 0;
	size_t count = this->get_symbol_position(symb_row, symb_col);

	// printf("Symb %c row: %u, col: %u, num: %zu\n", dyn_symdol_, symb_row, symb_col, symb_num);

	if(count == 0){
		return;
	}

	// Blocking
	this->animate_symbol(symb_row, symb_col, count);
}

// -----------------------------------------------------------------------------------------------

ListMenu::ListMenu(const std::string &main_header, const std::string &sub_header, const std::initializer_list<std::string> &content): 
	BaseMenu(content), main_header_(main_header), sub_header_(sub_header) 
{
	short_move_up = [this]()
	{
		if(position_ <= 1){
			
			if(position_ == 1){
				--position_;
			}
			
			// Show headers
			platform::lcd_clear();
			platform::lcd_home();
			this->show();
			return;
		}

		--position_;

		platform::lcd_set_cursor(0, 0);
		platform::lcd_print_with_padding(BaseMenu::get_content_value(position_ - 1));
		platform::lcd_set_cursor(0, LAST_COL);
		platform::lcd_print('<'); 
		platform::lcd_set_cursor(1, 0);
		platform::lcd_print_with_padding(BaseMenu::get_content_value(position_));
	};

	short_move_down = [this]()
	{
		const size_t list_size = BaseMenu::get_content_size();

		if(position_ >= list_size){
			return;
		}

		// platform::lcd_clear();

		platform::lcd_set_cursor(0, 0);
		platform::lcd_print_with_padding(BaseMenu::get_content_value(position_));
		platform::lcd_set_cursor(0, LAST_COL);
		platform::lcd_print('<');

		++position_;

		platform::lcd_set_cursor(1, 0);
		platform::lcd_print_with_padding( position_ < list_size ? BaseMenu::get_content_value(position_) : " ");
	};

	// select
	short_enter = [this]()
	{
		if( !on_select ){
			return;
		}

		if(position_ < 1){
			on_select("");
		}
		else{
			on_select(BaseMenu::get_content_value(position_ - 1));
		}
	};
}

void ListMenu::show() const
{
	if( !main_header_.empty() ){
		platform::lcd_print(main_header_);
		platform::lcd_set_cursor(0, LAST_COL);
		platform::lcd_print_custom_char(ARROWS);
		platform::lcd_set_cursor(1, 0);
	}

	if( !sub_header_.empty() ){
		platform::lcd_print(sub_header_);
	}
}

// -----------------------------------------------------------------------------------------------

OneLineListMenu::OneLineListMenu(const std::string &header, const std::initializer_list<std::string> &content, const std::string &empty_list_msg):
	 ListMenu(header, empty_list_msg, content)
{
	// Переопределяем колбеки отображения пролистывания

	short_move_up = [this]()
	{
		if(position_ <= 1){
			return;
		}

		--position_;

		platform::lcd_set_cursor(1, 0);
		platform::lcd_print_with_padding(BaseMenu::get_content_value(position_ - 1));

		platform::lcd_set_cursor(1, LAST_COL);
		if(position_ == 1){
			platform::lcd_print_custom_char(DOWN_ARROW);
		}
		else{
			platform::lcd_print_custom_char(ARROWS);
		}
	};

	short_move_down = [this]()
	{
		const size_t list_size = BaseMenu::get_content_size();

		if(position_ >= list_size){
			return;
		}

		platform::lcd_set_cursor(1, 0);
		platform::lcd_print_with_padding(BaseMenu::get_content_value(position_));
		
		++position_;

		platform::lcd_set_cursor(1, LAST_COL);
		if(position_ < list_size){
			platform::lcd_print_custom_char(ARROWS);
		}
		else{
			platform::lcd_print_custom_char(UP_ARROW);
		}
	};
}


void OneLineListMenu::show() const
{
	if( !main_header_.empty() ){
		platform::lcd_print(main_header_);
	}

	const size_t list_size = BaseMenu::get_content_size();

	if( !list_size ){
		platform::lcd_set_cursor(1, 0);
		platform::lcd_print(sub_header_);
		return;
	}

	position_ = 1; // Первая опция в списке выбрана по умолчанию
	platform::lcd_set_cursor(1, 0);
	platform::lcd_print(BaseMenu::get_content_value(0));

	platform::lcd_set_cursor(1, LAST_COL);
	if(list_size == 1){
		platform::lcd_print('<');
	}
	else{
		platform::lcd_print_custom_char(DOWN_ARROW);
	}
	
}

// -----------------------------------------------------------------------------------------------

// ----- Имплементация LCD Интерфейса -----

StaticMenuWithDynamicSymbol LCD_Interface::init_phase_menu{ {"ИНИЦИАЛИЗАЦИЯ...", "Версия: ХХ.ХХ"}, '.', std::chrono::milliseconds(250) };
StaticMenu LCD_Interface::error_menu{ {"ОШИБКА!", "ХХХ"} };
StaticMenuWithDynamicSymbol LCD_Interface::download_phase_menu{ {"ЗАГРУЗКА", "ДАННЫХ ..."}, '.', std::chrono::milliseconds(250) };
StaticMenu LCD_Interface::main_menu{ {"МАРШРУТ: ХХХХ", "ФРЕЙМ: XXXX"} };
OneLineListMenu LCD_Interface::route_selection_menu{"ВЫБОР МАРШРУТА:", {}, "XXXX"};

std::atomic<bool> LCD_Interface::need_render_{false};

void LCD_Interface::setup_menus()
{
	init_phase_menu.centrify_content();

	download_phase_menu.centrify_content();

	error_menu.centrify_content();

	// route_selection_menu.setup_timeout(5, [this](){ display(&main_menu); });
	// route_selection_menu.on_select = [this](std::string s){ 
	// 	printf("selected: '%s'\n", s.c_str()); 
	// 	const std::string &value = s.empty() ? "XXXX" : s;
	// 	display(&main_menu, { {0, "МАРШРУТ: " + value} }); 
	// };

	main_menu.long_enter = [this](){ display(&route_selection_menu); };
}

void LCD_Interface::prepare()
{
	if( !platform::ready() ){
		throw std::runtime_error(excp_method("platform not ready"));
	}

	if( !app_ ){
		throw std::invalid_argument(excp_method("app was not set (nullptr)"));
	}

	// Load custom characters to the LCD memory
	uint8_t arrows[8] = {
		0b00100,
		0b01110,
		0b11111,
		0b00000,
		0b00000,
		0b11111,
		0b01110,
		0b00100
	};

	uint8_t up_arrow[8] = {
		0b00100,
		0b01110,
		0b11111,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000
	};

	uint8_t down_arrow[8] = {
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b01110,
		0b00100
	};

	platform::lcd_add_custom_char(ARROWS, arrows);
	platform::lcd_add_custom_char(UP_ARROW, up_arrow);
	platform::lcd_add_custom_char(DOWN_ARROW, down_arrow);

	this->setup_menus();

	platform::set_buttons_long_press_threshold(app_->settings.btn_long_press_sec);

	// Выключение подсветки дисплея при таймауте
	backlight_timer_.create([](){ platform::lcd_backlight(false); });
	backlight_timer_.reset(app_->settings.lcd_backlight_timeout);
}

void LCD_Interface::run()
{
	if(thandle_.joinable()){
		return;
	}

	thandle_ = std::thread{[this](){ serve(); }};
}

void LCD_Interface::stop()
{
	if( !thandle_.joinable() ){
		return;
	}

	pthread_cancel(thandle_.native_handle());
	thandle_.join();

	backlight_timer_.destroy();

	platform::lcd_clear();
	platform::lcd_home();
	platform::lcd_backlight(false);
}

bool LCD_Interface::wait(std::chrono::milliseconds period_ms)
{
	using namespace std::chrono;

	const milliseconds tick_ms{50};
	milliseconds elapsed_ms{0};

	while(elapsed_ms < period_ms)
	{
		if(need_render_.load()){
			return true;
		}

		std::this_thread::sleep_for(tick_ms);
		elapsed_ms += tick_ms;
	}

	return false;
}

void LCD_Interface::display(BaseMenu *menu_ptr, const std::vector<std::pair<size_t, std::string>> &new_content)
{
	if( !new_content.empty() ){
		for(const auto &kv : new_content){
			menu_ptr->update_content(kv.first, kv.second);
		}
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		for_render_ = menu_ptr;
	}

	need_render_.store(true);
}

void LCD_Interface::render()
{
	const BaseMenu *menu = nullptr;

	{
		// Работаем с копией сохраненного указателя
		std::lock_guard<std::mutex> lock(mutex_);
		menu = for_render_;
	}

	// Обобщенная реакция на нажатие кнопок
	auto btn_cb_wrapper = [this](const BaseMenu *pmenu, const BaseMenu::button_callback &cb){
		try{
			// Прежде чем выполнять колбек кнопки проверяем 
			// была ли включена подсветка. Если нет, то назначение  
			// кнопки - только включить подстветку, во избежании
			// случайных переходов по меню.
			bool backlight_was_on = platform::lcd_get_backlight_state();

			platform::lcd_backlight(true);
			LCD_Interface::backlight_timer_.reset(app_->settings.lcd_backlight_timeout);

			pmenu->reset_timer();

			if(backlight_was_on && cb){
				cb();
			}
		}
		catch(const std::exception &e){
			log_excp("%s\n", e.what());
		}
	};

	try{
		// Установка событий нажатия кнопок
		platform::set_button_cb(platform::S3, [menu, btn_cb_wrapper](){
			btn_cb_wrapper(menu, menu->short_move_up);
		});

		platform::set_button_cb(platform::S2, [menu, btn_cb_wrapper]{
			btn_cb_wrapper(menu, menu->short_move_down);
		});

		platform::set_button_cb(platform::S1, [menu, btn_cb_wrapper]{
			btn_cb_wrapper(menu, menu->short_enter);
		},
		[menu, btn_cb_wrapper]{
			btn_cb_wrapper(menu, menu->long_enter);
		});

		// Установка обработчика таймаута 
		menu->start_timer();

		// Отображение содержимого меню
		platform::lcd_clear();
		platform::lcd_home();
		platform::lcd_backlight(true);
		LCD_Interface::backlight_timer_.reset(app_->settings.lcd_backlight_timeout);
		menu->show();
	}
	catch(const std::exception &e){
		log_excp("%s\n", e.what());
	}
}

void LCD_Interface::serve()
{
	// Обработка запросов отображения
	for(;;){

		if(this->wait(std::chrono::milliseconds(1000))){
			need_render_.store(false);
			this->render();
		}
	}
}


}//namespace



#ifdef _settings_MENU_TEST
int main(int argc, char *argv[])
{
	using namespace avi;

	try{
		logger.init(MSG_VERBOSE, "", 0, KB_to_B(1));
		platform::init("dummy_path");
		bool exit = false;

		LCD_Interface iface;
		iface.prepare();

		// StaticMenu smenu{ {"СтатикМеню", "Центрируем"} };
		// smenu.centrify_content();

		// StaticMenuWithDynamicSymbol sdmenu({"ДинамичМеню", "Loading ..."}, '.', std::chrono::milliseconds(250));
		// sdmenu.centrify_content();

		// ListMenu list_menu("Выбор маршрута", "Текущий: ???", {"1111", "2222", "3333", "4444", "5555"});

		// OneLineListMenu one_line_list_menu("Выбор маршрута:", {}, "список пуст");
		// one_line_list_menu.update_content({"1111", "АБВГДУЖ", "abc", "0", "5555"});
		// one_line_list_menu.add_content("6666666");
		// one_line_list_menu.on_select = [&](std::string value){  
		// 	printf("Selected: '%s'\n", value.c_str());
		// 	iface.display(&sdmenu, { {1, "Загрузка ....."} });
		// };
		// one_line_list_menu.long_enter = [&](){ platform::lcd_backlight(false); exit = true; };
		// one_line_list_menu.setup_timeout(5, [&](){ iface.display(&smenu); } );

		// smenu.short_enter = [&](){ iface.display(&sdmenu); };

		// sdmenu.long_enter = [&](){ iface.display(&list_menu); };
		// sdmenu.short_enter = [&](){ iface.display(&one_line_list_menu); };

		// list_menu.long_enter = [&](){ platform::lcd_backlight(false); exit = true; };
		

		LCD_Interface::init_phase_menu.short_enter = [&](){ iface.display(&LCD_Interface::download_phase_menu); }; // TEST
		LCD_Interface::download_phase_menu.short_enter = [&](){ iface.display(&LCD_Interface::error_menu); }; // TEST
		LCD_Interface::error_menu.short_enter = [&](){ iface.display(&LCD_Interface::route_selection_menu); }; // TEST
		LCD_Interface::route_selection_menu.long_enter = [&](){ platform::lcd_backlight(false); exit = true; };

		iface.run();
		//iface.display(&smenu);
		iface.display(&LCD_Interface::init_phase_menu, { {1, "Версия 1.2"} });

		LCD_Interface::route_selection_menu.update_content({
			"1111", "grob", "2222", "aasdasdsadsadasdada123", "zzzz"
		});

		while( !exit ){
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		
		iface.stop();
		platform::deinit();
	}
	catch(const std::runtime_error &e){
		log_err("%s\n", e.what());
		return 1;
	}
	catch(const std::exception &e){	
		log_msg(MSG_DEBUG | MSG_TO_FILE, "%s\n", e.what());
		return 2;
	}
	catch(...){
		printf("unknown error\n");
		return 99;
	}

	return 0;
}
#endif
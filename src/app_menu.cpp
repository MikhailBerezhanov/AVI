#include <stdexcept>
#include <cinttypes>

#define LOG_MODULE_NAME		"[ MEN ]"
#include "logger.hpp"

#include "platform.hpp"
#include "app_menu.hpp"
#include "app.hpp"

// Месторасположение пользовательских символов в памяти дисплея
#define ARROWS 		0
#define UP_ARROW 	1
#define DOWN_ARROW 	2

#define LAST_ROW  	( platform::lcd_get_num_rows() - 1)
#define LAST_COL 	( platform::lcd_get_num_cols() - 1)

namespace avi{

// Таймер отключения подсветки дисплея
utils::Timer BaseMenu::timer_;

bool Ticker::sleep(int ms) const
{
	int cnt = 0;

	while(cnt < ms){

		if(stopped_.load()){
			return false;
		}

		cnt += 50;
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	return true;
}

// Получение массива расстояний до символов строки в байтах (UTF-8)
static std::vector<int> get_symb_distances(const std::string &s)
{
	// Массив расстояний до символов строки в байтах содержит 
	// кол-во байт до начала символа на позиции, соответствующей 
	// индексу. Индексация с нуля. (Перед 0-вым символом 0 байт) 
	std::vector<int> res = {0};

	size_t curr_distance = 0;
	const uint8_t *ptr = reinterpret_cast<const uint8_t*>(s.c_str());

	while(*ptr){

		if((*ptr >> 7) == 0x00){
			++curr_distance;
			++ptr;
		}
		else{
			// Остальное считаем двухбайтовыми символами (Кириллица)
			curr_distance += 2;
			ptr += 2;
		}

		res.push_back(curr_distance);
	}

	return res;
}

// Обработчик отображения бегущей строки
void Ticker::periodic_shift()
{
	std::vector<int> symb_distances = get_symb_distances(content_);
	const size_t symb_num = symb_distances.size() - 1;

	int curr_symb_num = 1;
	int curr_pos = 0;
	int max_pos = symb_distances.at(symb_num - visible_len_);	// symb_num > visible_len
	int direction = 1;
	uint8_t sleep_miltiplier = 2;	// На первом и последнем символе замираем дольше 

	stopped_.store(false);

	for(;;){

		if(platform::lcd_get_backlight_state() == false){
			
			if( !sleep(tick_ms_.count()) ){
				return;
			}

			continue;
		}

		int shift = 0;
		int count = symb_distances.at(curr_symb_num + visible_len_ - 1) - symb_distances.at(curr_symb_num - 1);

		if(direction > 0){
			// Прямое направление (сдвиг слева направо)
			
			// Проверка UTF-8 октета начала
			shift = (content_.at(curr_pos) >> 7 == 0x00) ? 1 : 2;
		}
		else if(direction < 0){
			// В обработную строноу (сдвиг справа налево)

			size_t idx = (curr_symb_num >= 2) ? curr_symb_num - 2 : 0;

			shift = (content_.at( symb_distances.at(idx) ) >> 7 == 0x00) ? 1 : 2;
		}

		// printf("(%s) curr_pos: %d,count : %d, shift: %d\n", content_.c_str(), curr_pos, count, shift);

		platform::lcd_set_cursor(start_pos_.first, start_pos_.second);
		platform::lcd_print(content_.substr(curr_pos, count));

		curr_symb_num += direction;
		curr_pos += direction * shift;
		sleep_miltiplier = 1;

		if(curr_pos > max_pos){
			curr_symb_num = symb_num - visible_len_ + 1;
			curr_pos = max_pos;
			direction = -1;	// Обратный сдвиг
			sleep_miltiplier = 2;
		}

		if(curr_pos < 0){
			curr_symb_num = 1;
			curr_pos = 0;
			direction = 1;
			sleep_miltiplier = 2;
		}

		if( !sleep(sleep_miltiplier * tick_ms_.count()) ){
			return;
		}
	}
}

void Ticker::start()
{
	if(number_of_symbols(content_.c_str()) <= visible_len_){
		// Нет необходимости в бегущей строке для 
		// длины соответствующей области видимости
		platform::lcd_set_cursor(start_pos_.first, start_pos_.second);
		platform::lcd_print(content_);
		return;
	}

	if(handler_.joinable()){
		return;
	}

	// Запуск потока отображения бегущей строки
	std::thread t([this](){ this->periodic_shift(); });
	handler_ = std::move(t);
}

void Ticker::stop()
{
	stopped_.store(true);

	if(handler_.joinable()){
		handler_.join();
	}
}


// -----------------------------------------------------------------------------------------------

void BaseMenu::hide() const
{
	this->stop_tickers();

	platform::lcd_clear();
}

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
		// Строка содержит UTF-8 (не только ASCII) символы - вычисляем их кол-во
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

	if(count == 0){
		return;
	}

	// Блокирующий вызов
	this->animate_symbol(symb_row, symb_col, count);
}

// -----------------------------------------------------------------------------------------------

void StaticMenuWithTicker::update_static_content(int start_row, const std::string &content)
{
	if(start_row >= this->get_content_size()){
		this->add_content(content);
	}
	else{
		this->update_content(start_row, content);
	}
}

void StaticMenuWithTicker::update_ticker_content(int start_row, const std::string &content)
{
	const size_t symb_num = number_of_symbols(content.c_str());

	std::string curr_content;

	// Есть ли статическое содержимое на заданной строке
	if(start_row < this->get_content_size()){
		curr_content = this->get_content_value(start_row);
	}

	// Вычисляем доступное для отображения бегущей строки место
	int start_col = number_of_symbols(curr_content.c_str());
	int visible_len = platform::lcd_get_num_cols() - start_col;

	// Создаем новую или обновляем существующую бегущую строку
	Ticker ticker(content, {start_row, start_col}, visible_len);

	// Если текущий существует - останавливаем 
	this->stop_ticker(start_row);
	this->add_ticker(start_row, std::move(ticker));
}

void StaticMenuWithTicker::show() const
{
	// Статический контент не требуется отображать каждый вызов
	if( !static_content_shown_.load() ){
		StaticMenu::show();
		static_content_shown_ = true;
	}

	this->start_tickers();
}

void StaticMenuWithTicker::hide() const
{
	static_content_shown_.store(false);
	BaseMenu::hide();
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
			
			// Отобразить заголовки
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

		platform::lcd_set_cursor(0, 0);
		platform::lcd_print_with_padding(BaseMenu::get_content_value(position_));
		platform::lcd_set_cursor(0, LAST_COL);
		platform::lcd_print('<');

		++position_;

		platform::lcd_set_cursor(1, 0);
		platform::lcd_print_with_padding( position_ < list_size ? BaseMenu::get_content_value(position_) : " ");
	};

	// Нажатие кнопки "выбор"
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

		this->show_option(position_ - 1);

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

		this->show_option(position_);

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

void OneLineListMenu::show_option(size_t content_idx) const
{
	std::string content = BaseMenu::get_content_value(content_idx);
	const int max_len = platform::lcd_get_num_cols() - 2;

	this->stop_ticker(1);

	if(number_of_symbols(content.c_str()) > max_len){

		Ticker ticker{content, {1, 0}, max_len};

		this->add_ticker(1, std::move(ticker));
		this->start_ticker(1);
	}
	else{
		platform::lcd_set_cursor(1, 0);
		platform::lcd_print_with_padding(content);
	}
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
	
	this->show_option(0);

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
StaticMenu LCD_Interface::ok_menu{ {"ОК"} };

StaticMenuWithDynamicSymbol LCD_Interface::download_phase_menu{ {"ЗАГРУЗКА", "ДАННЫХ ..."}, '.', std::chrono::milliseconds(250) };
StaticMenuWithTicker LCD_Interface::main_menu{ {"МАРШРУТ: ", "ФРЕЙМ: "} };

OneLineListMenu LCD_Interface::settings_menu{"НАСТРОЙКИ", {"СМЕНА МАРШРУТА", "ГРОМКОСТЬ", "ИНФОРМАЦИЯ"}};
OneLineListMenu LCD_Interface::route_selection_menu{"ВЫБОР МАРШРУТА:", {}, "XXXX"};
OneLineListMenu LCD_Interface::audio_level_menu{"ГРОМКОСТЬ", {"0", "1", "2", "3", "4"}};
OneLineListMenu LCD_Interface::info_menu{"ИНФОРМАЦИЯ"};
std::function< std::vector<std::string>() > LCD_Interface::update_info_menu = nullptr;

std::atomic<bool> LCD_Interface::need_render_{false};

void LCD_Interface::setup_menus()
{
	init_phase_menu.centrify_content();
	download_phase_menu.centrify_content();
	error_menu.centrify_content();
	ok_menu.centrify_content();

	ok_menu.setup_timeout(1, [this](){ display(&main_menu); });

	// Меню настроек
	settings_menu.on_select = [this](std::string option){

		if(option == "СМЕНА МАРШРУТА"){
			display(&route_selection_menu);
		}
		else if(option == "ГРОМКОСТЬ"){
			display(&audio_level_menu);
		}
		else if(option == "ИНФОРМАЦИЯ"){

			if( !update_info_menu ){
				return;
			}

			std::vector<std::string> info_content = update_info_menu();
			info_menu.update_content(std::move(info_content));
			display(&info_menu);
		}
	};

	settings_menu.setup_timeout(menu_timeout, [this](){ display(&main_menu); });

	// Настройки Аудио
	audio_level_menu.on_select = [this](std::string option){

		try{
			int level = std::stoi(option);
			platform::audio_set_gain_level(level);

			display(&ok_menu);
		}
		catch(const std::exception &e){
			log_excp("audio_level_menu.on_select: %s\n", e.what());
			display(&main_menu);
		}
	};

	audio_level_menu.setup_timeout(menu_timeout, [this](){ display(&main_menu); });

	// Меню Информации об устройстве
	info_menu.setup_timeout(menu_timeout, [this](){ display(&main_menu); });
	info_menu.on_select = [this](std::string option){ display(&settings_menu); };

	// Главное меню
	main_menu.long_enter = [this](){ display(&settings_menu); };
	main_menu.update_ticker_content(0, "НЕ ВЫБРАН");
	main_menu.update_ticker_content(1, "XXXX");	// по умолчанию нет попадани ни в один фрейм 
}

void LCD_Interface::prepare()
{
	if( !platform::ready() ){
		throw std::runtime_error(excp_method("platform not ready"));
	}

	if( !app_ ){
		throw std::invalid_argument(excp_method("app was not set (nullptr)"));
	}

	// Загрузка пользовательских символов в память дисплея
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

	const BaseMenu* curr_menu = this->curr_menu();
	if( curr_menu && (curr_menu != menu_ptr) ){
		// Прячем текущее меню при запросе отображения нового
		curr_menu->hide();
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
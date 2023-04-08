/*============================================================================== 
Описание: 	Модуль обслуживания дисплейного интерфейса  .

Автор: 		berezhanov.m@gmail.com
Дата:		28.06.2022
Версия: 	1.0
==============================================================================*/

#pragma once

#include <string> 
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include <chrono>
#include <utility>
#include <unordered_map>

#include "utils/timer.hpp"

namespace avi{

class AVI;

// Бегущая строка
// (может быть частью любого меню)
class Ticker
{
public:

	Ticker(const std::string &content = "", std::pair<int, int> start = {-1, -1}, int len = 0):
		content_(content), start_pos_(start), visible_len_(len){}

	~Ticker()
	{
		if(handler_.joinable()){
			pthread_cancel(handler_.native_handle());
			handler_.join();
		}
	}

	// Объекты можно только перемещать 

	Ticker(Ticker &&t): handler_(std::move(t.handler_)), tick_ms_(std::move(t.tick_ms_)),
		start_pos_(std::move(t.start_pos_)), content_(std::move(t.content_)), visible_len_(std::move(t.visible_len_)) 
	{
		stopped_.store(t.stopped_.load());
	}

	Ticker& operator= (Ticker &&t)
	{
		this->stop();

		if(handler_.joinable()){
			pthread_cancel(handler_.native_handle());
			handler_.join();
		}

		handler_ = std::move(t.handler_);
		tick_ms_ = std::move(t.tick_ms_);
		start_pos_ = std::move(t.start_pos_);
		content_ = std::move(t.content_);
		stopped_.store(t.stopped_.load());
		visible_len_ = std::move(t.visible_len_);
	}

	void start();
	void stop();

	bool running() const { return handler_.joinable(); }

private:
	std::thread handler_;
	std::chrono::milliseconds tick_ms_{350};
	std::pair<int, int> start_pos_;	// start (row, col)
	std::string content_;
	std::atomic<bool> stopped_{false};
	int visible_len_ = 0;

	bool sleep(int ms) const;
	void periodic_shift();
};

// Абстрактный класс меню
class BaseMenu
{
public:
	using timeout_callback = std::function<void()>;
	using button_callback = std::function<void()>;

	BaseMenu(const std::initializer_list<std::string> &content = {}): content_(content) {}

	virtual ~BaseMenu() 
	{ 
		timer_.destroy(); 
		this->stop_tickers();
	}

	virtual void show() const = 0;	// Вызов отображения меню на экране дисплея
	virtual void hide() const; 		// Вызов при переходе на другое меню (при сворачивании)

	void update_content(std::vector<std::string> &&new_content){
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		content_ = std::move(new_content);
		if(was_centrified_){
			this->centrify_content();
		}
	}

	void update_content(const std::initializer_list<std::string> &new_content){
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		content_.assign(new_content);
		if(was_centrified_){
			this->centrify_content();
		}
	}

	void update_content(size_t idx, const std::string &value){
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if(idx < content_.size()){
			content_[idx] = value;
		}
		if(was_centrified_){
			this->centrify_content_item(idx);
		}
	}

	void add_content(const std::string &value){
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		content_.push_back(value);
		if(was_centrified_){
			this->centrify_content_item(content_.size() - 1);
		}
	}
	
	inline size_t get_content_size() const{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return content_.size();
	}

	inline std::string get_content_value(size_t idx) const{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return content_.at(idx);
	}

	std::vector<std::string> get_content_values(size_t from, size_t to) const;

	inline std::vector<std::string> get_content() const{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return content_;
	}

	void setup_timeout(int timeout_sec, timeout_callback cb){
		timeout_sec_ = timeout_sec;
		on_timeout_ = cb;
	}

	void start_timer() const {
		// Создаем таймер для обработки событий таймаутов меню
		timer_.create(nullptr);
		timer_.set_timeout_callback(this->on_timeout_);
		// останавливаем таймер если колбек на задан
		timer_.reset(this->on_timeout_ ? timeout_sec_ : 0);
	}

	void reset_timer() const {
		if(timeout_sec_){
			timer_.reset(timeout_sec_);
		}
	}
	
	button_callback short_move_up = nullptr;
	button_callback short_move_down = nullptr;
	button_callback short_enter = nullptr;
	button_callback long_enter = nullptr;

	
	void add_ticker(int id, Ticker &&ticker) const 
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		tickers_[id] = std::move(ticker);
	}

	void start_ticker(int id) const 
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		auto it = tickers_.find(id);
		if(it != tickers_.end()){
			it->second.start();
		}
	}

	void start_tickers() const 
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		for(auto &elem : tickers_){
			elem.second.start();
		}
	}

	void stop_ticker(int id) const 
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		auto it = tickers_.find(id);
		if(it != tickers_.end()){
			it->second.stop();
		}
	}

	void stop_tickers() const 
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		for(auto &elem : tickers_){
			elem.second.stop();
		}
	}

	void centrify_content(); 
	void centrify_content_item(size_t idx);

private:
	mutable std::recursive_mutex mutex_;
	std::vector<std::string> content_;
	bool was_centrified_ = false;
	int timeout_sec_ = 0;
	timeout_callback on_timeout_ = nullptr;
	static utils::Timer timer_;

	mutable std::unordered_map<int, Ticker> tickers_;
};

class StaticMenu : public BaseMenu
{
public:
	StaticMenu(const std::initializer_list<std::string> &content = {}): BaseMenu(content) {}

	void show() const override;

private:
};

class StaticMenuWithDynamicSymbol : public StaticMenu
{
public:
	StaticMenuWithDynamicSymbol(const std::initializer_list<std::string> &content = {}, 
		char dyn_symdol = '.', std::chrono::milliseconds period_ms = std::chrono::milliseconds(500)): 
		StaticMenu(content), dyn_symdol_(dyn_symdol), update_period_(period_ms) {}

	void show() const override;

private:
	char dyn_symdol_ = '.';
	std::chrono::milliseconds update_period_{500};

	inline size_t get_symbol_position(uint8_t &row, uint8_t &col) const;
	inline void animate_symbol(uint8_t row, uint8_t col, size_t count) const;
};

class StaticMenuWithTicker : public StaticMenu
{
public:
	StaticMenuWithTicker(const std::initializer_list<std::string> &static_content = {}):
		StaticMenu(static_content), static_content_shown_(false) {}

	void update_static_content(int start_row, const std::string &content);
	void update_ticker_content(int start_row, const std::string &content);

	void show() const override;
	void hide() const override; 

private:
	mutable std::atomic<bool> static_content_shown_{false};
};

class ListMenu : public BaseMenu
{
	using selection_callback = std::function<void(std::string)>;

public:
	ListMenu(const std::string &main_header = "ListMenu", const std::string &sub_header = "", const std::initializer_list<std::string> &content = {});

	void show() const override;

	selection_callback on_select = nullptr;

protected:
	std::string main_header_;
	std::string sub_header_;

	// Позиции списка нумеруются с 1 (0 - ничего не выбрано)
	mutable size_t position_ = 0;	
};

class OneLineListMenu : public ListMenu
{
public:
	OneLineListMenu(const std::string &header = "OneLineListMenu", const std::initializer_list<std::string> &content = {}, const std::string &empty_list_msg = "List is empty");

	void show() const override;

private:
	void show_option(size_t content_idx) const;
};

// Класс, обслуживающий отображение на дисплее и переназначение кнопок 
// в зависимости от настроек заданного меню.
//
// Меню приложения создаются как статические члены класса и доступны на
// протяжении всего выполнения программы.
//
// Содержимое каждого меню может изменяться по ходу выполнения программы 
// (runtime) методом update_content() соответствующего экземпляра меню.
// Для отображения меню на дисплее используется метод display() с указателем
// на требуемый экземпляр.
//
// Связи и переходы между разными меню инициализируются в методе prepare().
class LCD_Interface
{
public:
	LCD_Interface(const AVI *app = nullptr): app_(app) {}

	// Подготовка дисплея и инициализация связей меню
	void prepare();
	// Запуск интерфейса
	void run();
	// Остановка интерфейса
	void stop();
	
	// Ожидание сигнала need_render_. Используется в 
	// динамических меню в качестве прерываемых задержек
	static bool wait(std::chrono::milliseconds ms);

	// Основная функция - запрос отображения того или иногда меню на дисплее
	// с возможностью модификации содержимого (индекс - значение)
	// Если указанного индекса нет - игнорирует 
	void display(BaseMenu *menu_ptr, const std::vector<std::pair<size_t, std::string>> &new_content = {});
	
	const BaseMenu* curr_menu() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return for_render_;
	}

	// Экземпляры меню приложения 
	static StaticMenuWithDynamicSymbol init_phase_menu;
	static StaticMenu error_menu;
	static StaticMenu ok_menu;
	static StaticMenuWithDynamicSymbol download_phase_menu;
	static StaticMenuWithTicker main_menu;
	static OneLineListMenu settings_menu;
	static OneLineListMenu route_selection_menu;
	static OneLineListMenu audio_level_menu;
	static OneLineListMenu info_menu;

	static const int menu_timeout = 5; // sec
	static std::function< std::vector<std::string>() > update_info_menu;

private:
	const AVI *const app_ = nullptr;

	std::thread thandle_;

	static std::atomic<bool> need_render_;
	mutable std::mutex mutex_;
	const BaseMenu *for_render_ = nullptr;

	// Таймер для выключения подсветки дисплея 
	// при долгом отсутствии нажатий на кнопки
	utils::Timer backlight_timer_;

	void setup_menus();
	void serve();
	void render();
};

}//namespace

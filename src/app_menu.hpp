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

#include "utils/timer.hpp"

namespace avi{

class AVI;

class BaseMenu
{
public:
	using timeout_callback = std::function<void()>;
	using button_callback = std::function<void()>;

	BaseMenu(const std::initializer_list<std::string> &content = {}): content_(content) {}

	virtual ~BaseMenu() { timer_.destroy(); };

	virtual void show() const = 0;

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

	void centrify_content(); 
	void centrify_content_item(size_t idx);
	// inline void add_padding_after_content(char symb = ' ');

private:
	mutable std::recursive_mutex mutex_;
	std::vector<std::string> content_;
	bool was_centrified_ = false;
	int timeout_sec_ = 0;
	timeout_callback on_timeout_ = nullptr;
	static utils::Timer timer_;
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
	static StaticMenuWithDynamicSymbol download_phase_menu;
	static StaticMenu main_menu;
	static OneLineListMenu route_selection_menu;

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

/*============================================================================== 
Описание: 	Основной модуль приложения, осуществляющий инициализацию всех подсистем.

Автор: 		berezhanov.m@gmail.com
Дата:		22.04.2022
Версия: 	1.0
==============================================================================*/

#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "logger.hpp"
#include "utils/utility.hpp"
#include "utils/fs.hpp"

#include "app_lc.hpp"
#include "app_menu.hpp"
#include "announ.hpp"

#ifndef APP_VERSION
#define APP_VERSION					"1.0"
#endif

#define APP_NAME 					"AVI"

namespace avi
{

class AVI
{
public:

	// Настройки приложения								
	struct Settings{	
		std::string update_command;				// Команда вызова скрипта обновления ПО
		std::string i2c_dev;					// Название устройства I2C
		
		uint64_t log_max_size = utils::KB_to_B(512);	// Максимальный размер лог-файла в Kбайтах
		uint64_t log_backup_max_size = utils::KB_to_B(1024);	// Максимальный размер директории для хранения логов в Kбайтах
		uint64_t main_db_max_size = utils::KB_to_B(512);		// Максимальный размер основной БД в Kбайтах

		log_lvl_t log_level = MSG_DEBUG;		// Уровень логирования
		int sys_ev_level = 0;					// Уровень подробности системных событий
		int lc_poll_period = 60;				// Период запуска клиента ЛЦ (сек)
		int lc_download_period = 60;			// Период запроса обновлений файлов из ЛЦ (сек)
		int gps_poll_period_ms = 1000; 			// Период обновления GPS координат (мс)
		int gps_valid_threshold = 4;			// Порог валидности GPS координат
		int lcd_backlight_timeout = 10;			// Таймаут выключения подстветки дисплея (сек)
		double btn_long_press_sec = 2.0;		// Порог длительного нажатия на кнопку (сек)
		double gps_min_valid_speed = 6.0;		// Минимальная валидная скорость по GPS (км\ч) (курс может быть неустановившимся)
	};

	// Рабочие директории приложения
	struct Directories{
		const std::string base_dir = utils::get_cwd();				// Корневая Рабочая директория

		std::string config_path = base_dir + "/avi.conf";			// Путь к конфиг-файлу 
		const std::string backup_dir = base_dir + "/backup";		// Директория для хранения резервных копий
		std::string log_path = base_dir + "/avi.log";				// Путь к лог-файлу
		std::string log_backup_dir = base_dir + "/log_backup";		// Директория для бэкапа логов
		std::string main_db_path = base_dir + "/avi_state.db";		// Путь к основной базе 
		std::string data_dir = "/sdcard/avi_data";					// Директория для хранения данных
		std::string updates_dir = data_dir + "/updates"; 			// Директория для скачивания обновлений
		std::string tmp_dir = data_dir + "/tmp";					// Поддиректория временных файлов в директории данных СУВ
		std::string nsi_db_path = data_dir + "/nsi.db";				// Путь к базе НСИ
		std::string media_dir = data_dir + "/media"; 				// Путь к директории медиа-файлов
		std::string gps_gen_path; 									// Путь к файлу симуляции GPS
		std::string gps_track_path = data_dir + "/gps.track";		//

		void create() const;
		void reset_data_subdirs();
	};

	// Идентификатор Устройства
	struct DeviceId{
		DeviceId(uint32_t val = 0): value_(val) {}

		inline uint32_t get() const { std::lock_guard<std::mutex> lck(mutex_); return value_; }
		inline void set(uint32_t val) { std::lock_guard<std::mutex> lck(mutex_); value_ = val; }

		void generate_from(const std::string &in);

	private:
		mutable std::mutex mutex_;
		uint32_t value_ = 0;
	};

	void init(bool conf_trace = false);
	void deinit();

	void create_sys_event(const std::string &ev_name, const std::string &ev_data = "") const;

	Settings settings;				// Настройки приложения
	Directories dirs;				// Рабочие директории
	DeviceId dev_id;				// Идентификатор устройства
	mutable MainDatabase mdb; 		// Основная БД приложения
	mutable LCD_Interface iface{this};	// Интерфейс (ЖК дисплей + кнопки)

private:
	LC_client_task lc_task{this};			// Фоновая задачи связи с сервером ЛЦ
	Announcement_task announ_task{this};	// Фоновая задача оповещения

	mutable std::mutex init_phase_mutex;

	static void backup_log_file(void *obj);

	void preinit(bool conf_trace, bool first_time_called);
	void interface_init();
	void lc_client_init();

	void data_check();
	bool nsi_reload() const;

	void regular_mode();		// Штатный режим работы
	void wait_for_data(const std::string &data_type);

	void wait_for_route_selection();
};




} // namespace avi

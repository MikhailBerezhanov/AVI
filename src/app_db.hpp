/*============================================================================== 
Описание: 	Модуль работы с базами данных sqlite приложения.

Автор: 		berezhanov.m@gmail.com
Дата:		26.04.2022
Версия: 	1.0
==============================================================================*/

#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

extern "C"{
#include <sqlite3.h>
}

namespace avi{

struct invalid_file_type: public std::runtime_error
{
	invalid_file_type() = default;
	invalid_file_type(const std::string &s): std::runtime_error(s) {}
};

// Абстрактный класс представления таблицы
class Base_table
{
public:
	virtual ~Base_table() = default;

	// Колбек на SQL запрос
	typedef int (*sq3_cb_func)(void*, int, char**, char**);

	// Создание таблицы
	virtual void create() = 0;

	// Очистка таблицы
	virtual void clear();

	// Добавление столбца если не существует
	bool add_column_if_not_exists(
		const std::string &col_name, 
		const std::string &col_type, 
		const std::string &col_value = "");

	void set_fd_ptr(sqlite3 **db) { fd_ptr = db; }
	void set_name(const std::string &n) { name = n; }

protected:
	Base_table() = default;
	Base_table(const std::string &n, sqlite3 **db = nullptr): name(n), fd_ptr(db) {}

	std::string name;			// Название таблицы
	sqlite3 **fd_ptr = nullptr;	// Указатель на дескриптор базы данных 

	void send_sql(const std::string &sql, const std::string &caller = "", sq3_cb_func cb = nullptr, void *param = nullptr) const; 
};


// Типы используемых счетчиков
#define APP_CNT_TRIP_NUM 	"trip_number"	
#define APP_CNT_SYS_EV	 	"sys_events"		

// Таблица содержащая системные счетчики 
class Counters_table final: public Base_table
{
public:
	Counters_table(const std::string &n = "Counters", sqlite3 **sq = nullptr): Base_table(n, sq) {}

	void create() override;

	// Типы используемых счетчиков cnt_type задаются макро-определениями:
	// 		APP_CNT_TRIP_NUM		-	Номер рейса
	// 		APP_CNT_SYS_EV			-	Счетчик системных событий устройств

	void set(const std::string &cnt_type, uint32_t value);
	uint32_t get(const std::string &cnt_type);

	// Инкремент и получение нового значения с обновлением записи в таблице
	uint32_t inc_and_get(const std::string &cnt_type);

private:
	// Кешируемые значения храним с хеш-таблице
	mutable std::recursive_mutex cache_mtx;
	std::unordered_map<std::string, uint32_t> cache;

	void update_cache(const std::string &cnt_type, uint32_t value) 
	{
		std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
		cache[cnt_type] = value;
	}
};

#define APP_FILE_NSI 		"nsi"
#define APP_FILE_MEDIA  	"media"

// Версии рабочих файлов приложения
// Ключи:
// 		"nsi"						// Текущая вресия НСИ
// 		"stoplist"					// Текущая вресия стоп-листа
// 		"magiclist",				// Текущая вресия мега-листа
// 		"firmw_validator"			// Текущая вресия прошивки валидатора
// 		"firmw_reader"				// Текущая вресия прошивки ридера 
// 		"softw_suv"					// Текущая вресия ПО СУВ
using files_versions = std::unordered_map<std::string, std::string>;

// Таблица содержащая данные о файлах приложения
class FilesMetadata_table final: public Base_table
{
public:
	FilesMetadata_table(const std::string &n = "FilesMetadata", sqlite3 **sq = nullptr): Base_table(n, sq) {}

	void create() override;

	// Интерфейс доступа к таблице новой версии
	void set_fver(const std::string &prot_ft, const std::string &ver, const std::string &date = "");
	void set_fdate(const std::string &prot_ft, const std::string &date);
	files_versions get_fvers();
	std::string get_fver(const std::string &prot_ft);
	files_versions get_fdates();	// в поля files_versions записываются даты обновления файлов
	std::string get_fdate(const std::string &prot_ft);

private:
	std::string file_type_to_db_type(const std::string &prot_type);

	mutable std::recursive_mutex cache_mtx;
	files_versions vers_cache;
	files_versions dates_cache;
};

// Таблица информации об устройстве 
class DeviceInfo_table final: public Base_table
{
public:
	DeviceInfo_table(const std::string &n = "DeviceInfo", sqlite3 **sq = nullptr): Base_table(n, sq) {}

	void create() override;

	void set_start_time(time_t unix_time);
	time_t get_start_time() const;

	void set_device_id(uint32_t id);
	uint32_t get_device_id() const;

	void set_device_id_source(const std::string &src);
};


// Режимы работа с базой
#define DB_RO			SQLITE_OPEN_READONLY	// Только чтение
#define DB_RW			SQLITE_OPEN_READWRITE	// Чтение и запись
#define DB_FMTX 		SQLITE_OPEN_FULLMUTEX	// Защитить доступ к БД мьютексом 
#define DB_CREATE		SQLITE_OPEN_CREATE		// Создать БД если не существует

// Локальная база состояний приложения
class MainDatabase
{
public:
	void init(const std::string &path, int modes = DB_RW | DB_FMTX | DB_CREATE);
	void flush();
	void deinit();
	void remove_old_records() { /*sys_status.clear();*/ }

	// SystemStatus_table sys_status;
	Counters_table sys_counters;

	FilesMetadata_table f_data;
	DeviceInfo_table dev_info;

private:
	sqlite3 *fd = nullptr;
	std::string path;
};



// База нормативно-справочной информации (НСИ)
// (выполняет роль конфигурации работы приложения) 
class NSIDatabase
{
public:
	NSIDatabase() = delete;

	// таблица, содержащая информацию о сигнатуре и типе маршрута
	class kRoute_table final: public Base_table
	{
	public:
		kRoute_table(const std::string &n = "kRoute", sqlite3 **sq = nullptr): Base_table(n, sq) {}

		void create() override {} 	// Таблица уже присутствует в базе - пропускаем создание
		
		struct route
		{
			std::string number;		// Номер маршрута состоит из трёх первых цифр и последней литеры (в общем случае пробел (ASCII 0x20))
			std::string name;		// Название маршрута
			std::string tpscode; 	// ТПС маршрута
			int code = 0;			// Код маршрута по реестру маршрутов
			int id = 0;				// Идентификатор записи, выгруженной из общей БД
			int townflag = 0;		// Тип маршрута (городской/не городской), 1 – городской, 0 – не городской
			int stops = 0;			// Количество остановок на маршруте
			int tariffmin = 0;		// Минимальная сумма на кошельке для входа в салон (в копейках. Например, 2000 = 20 рублей)
		};

		// Идентификатор маршрута (id) -> данные о маршруте
		using routes = std::unordered_map<int, route>;	

		routes read();	
	};

	// таблица, содержащая конфигурацию работы устройства
	class kCfg_table final: public Base_table
	{
	public:
		kCfg_table(const std::string &n = "kCfg", sqlite3 **sq = nullptr): Base_table(n, sq) {}

		// cfgParam -> paramMeaning
		using params = std::unordered_map<std::string, std::string>;

		void create() override {} 

		params read();

		// Список поддерживаемых параметров ( ключей ):
		// VEHICLE			// Вид транспорта (например, «автобус»)
		// FIRM_NAME		// Наименование фирмы-заказчика (например, «МОСАВТОТРАНС»)
		// dbType			// Тип базы (фактически в это поле пишется значение «zones»)
		// devType			// Тип устройства (берётся из общего списка обозначений (для AVI 10))
		// dbStructVersion	// Версия структуры БД. Записывается в виде AAAABBBB (8 символов в hex, с лидирующими нулями)
		// dataVersion		// Версия данных в базе 
		// dataDateYear		// Дата (год) выпуска версии данных. Хранится в десятичном формате YYYY (например, 2008)
		// dataDateMon		// Дата (месяц) выпуска версии данных. Хранится в десятичном формате MM (от 1 до 12)
		// dataDateDay		// Дата (день) выпуска версии данных. Хранится в десятичном формате DD (от 1 до 31)
		// ClockShift		// Часовой пояс для расчета времени относительно UTC0 
		// TNAV				// Период сохранения навигационных отметок в лог (в секундах)
		// Route			// Флаг проверки маршрута
		void read_param(const std::string &param_name, params &out);
	};

	// таблица, содержащая конфигурацию работы устройства
	class kFrames_table final: public Base_table
	{
	public:
		kFrames_table(const std::string &n = "kFrames", sqlite3 **sq = nullptr): Base_table(n, sq) {}

		void create() override {} 

		// struct frame
		// {
		// 	double lon_start = 0.0;	// Долгота начала (центра для зоны в виде окружности)
		// 	double lat_start = 0.0;	// Широта начала (центра для зоны в виде окружности)
		// 	double lon_end = 0.0;	// Долгота окончания (NULL для зоны в виде окружности)
		// 	double lat_end = 0.0; 	// Широта окончания (NULL для зоны в виде окружности)
		// 	double radius = 0.0;	// Радиус зоны в виде окружности (метры)	
		// 	int course = 0;			// Курс (целое число от 0 до 255, битовая маска сектора) 
		// 	int id_next = -1;		// Идентификатор следующего воспроизводимого фрейма
		// 	int is_child = 0;		// 0 не дочерний, 1 - дочерний
		// 	int play_mode = 0;		// Режим воспроизведения
		// 	std::string filename;	// Имя файла с mp3 для проигрования
		// };

		// Абстрактная Зона действия фрейма
		struct Zone
		{
			Zone(double lat_start, double lon_start, uint8_t course_bitmap): 
				lat_start_(lat_start), lon_start_(lon_start), course_bitmap_(course_bitmap) {}
			virtual ~Zone() = default;

			// Курс в градусах: [0 .. 360]
			// Курс по-умолчанию не учитывается
			virtual bool contains(const std::pair<double, double> &lat_lon, double course = -1.0) const = 0;
			virtual std::string show() const = 0;

			// Преобразование курса в градусах в битовое представление
			static uint8_t course_to_bitmask(double course_degrees) noexcept;
			bool course_check(double course_degrees) const noexcept;

		protected:
			double lat_start_ = 0.0;	// Широта начала (центра для зоны в виде окружности)	
			double lon_start_ = 0.0;	// Долгота начала (центра для зоны в виде окружности)
			uint8_t course_bitmap_ = 0;	// Курс (целое число от 0 до 255, битовая карта сектора)
		};

		// Прямоугольная зона
		struct RectangleZone: public Zone
		{
			RectangleZone(double lat_start, double lon_start, uint8_t course, double lat_end, double lon_end):
				Zone(lat_start, lon_start, course), lat_end_(lat_end), lon_end_(lon_end) {}

			bool contains(const std::pair<double, double> &lat_lon, double course) const override;
			std::string show() const override;

		protected:
			double lat_end_ = 0.0; 	// Широта окончания (NULL для зоны в виде окружности)
			double lon_end_ = 0.0;	// Долгота окончания (NULL для зоны в виде окружности)
		};

		// Круглая зона
		struct CircleZone: public Zone
		{
			CircleZone(double lat_start, double lon_start, uint8_t course, double radius):
				Zone(lat_start, lon_start, course), radius_(radius) {}

			bool contains(const std::pair<double, double> &lat_lon, double course) const override;
			std::string show() const override;

		protected:
			double radius_ = 0.0;	// Радиус зоны в виде окружности (метры)
		};

		// Медиа информация
		struct MediaInfo
		{
			std::string filename;	// Имя файла с mp3 для проигрования
			int id_next = -1;		// Идентификатор следующего воспроизводимого фрейма
			int pause = 0;			// Пауза перед воспроизведением (сек)
			// uint8_t is_child = 0;	// 0 не дочерний, 1 - дочерний
			uint8_t play_mode = 0;	// Режим воспроизведения
			// padd 
		};

		struct Frame
		{
			int id = -1;			// Идентификатор фрейма
			std::unique_ptr<Zone> zone;	// Указатель на конкретную зону (может быть nullptr)		 
			MediaInfo minfo;
		};

		// Массив основных фреймов.
		// Среди основных фреймов поиск идет по текущим координатам и вызывается часто -
		// используем последовательный контейнер для увеличения частоты попаданий в кеш.
		// Сортируется по возрастанию id.
		using main_frames = std::vector<Frame>;	

		// Таблица дочерних фреймов (идентификатор -> медиа-данные).
		// Среди дочерних фреймов поиск идет по id и обращения редкие - 
		// используем хеш-таблицу.
		using child_frames = std::unordered_map<int, MediaInfo>;

		// Фреймы распределемы по идентификаторам маршрутов
		std::pair<main_frames, child_frames> read(int route_id);

	private:

	};

	static void set_path(const std::string &path){ path_ = path; }

	static bool open(int modes = DB_RO | DB_FMTX);

	static void update(const std::string &path);

	static void close();

	static void read();

	static bool ready(){
		return get_version() != "unknown";
	}

	static void show_routes();
	static void show_cfg_params();
	static void show_frames();

	static std::vector<std::string> get_available_route_names();

	static void select_route(int route_id);
	static void select_route(const std::string &route_name);

	static int get_current_route(){
		std::lock_guard<std::mutex> lock(curr_route_mutex_);
		return curr_route_id_;
	}

	static bool route_selected(){
		std::lock_guard<std::mutex> lock(curr_route_mutex_);
		return curr_route_id_ != -1;
	}

	// Заполняет структуру frames_ в соответствии с выбранным 
	// при помощи select_route() идентификатором маршрута 
	static void reload_route_frames();

	static bool check_media_content_presence(const std::string &media_dir);

	template<typename T>
	static T get_cfg_param(const std::string &param_name, T default_value)
	{
		std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

		auto it = cfg_params_.find(param_name);

		if(it != cfg_params_.end()){
			std::stringstream ss{it->second};
			T ret;
			ss >> ret;

			return ret;
		}

		return default_value;
	}

	static std::string get_version();	

	static const kFrames_table::MediaInfo* find_media_info(const std::pair<double, double> &lat_lon, double course, int *frame_id = nullptr); 
	static const kFrames_table::MediaInfo* get_media_info_of_child(int id);

private:
	static std::recursive_mutex db_file_mutex_;

	static sqlite3 *fd_;
	static std::string path_;

	static kRoute_table kroute_;
	static kCfg_table kcfg_;
	static kFrames_table kframe_;

	static std::mutex curr_route_mutex_;
	static int curr_route_id_;

	// Текущие маршруты
	static kRoute_table::routes routes_;
	// Текущие Параметры конфигурации
	static kCfg_table::params cfg_params_;
	// Текущие фреймы воспроизведения аудио оповещений
	static std::pair<kFrames_table::main_frames, kFrames_table::child_frames> frames_;
};


}//namespace avi

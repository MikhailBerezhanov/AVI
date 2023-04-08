#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cinttypes>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <limits>
#include <algorithm>

#include "utils/utility.hpp"
#include "utils/iconvlite.hpp"
#include "utils/fs.hpp"

#define LOG_MODULE_NAME		"[ MDB ]"
#include "logger.hpp"

#include "app_db.hpp"


#define to_s(x) 	std::to_string(x)

namespace avi{

void Base_table::send_sql(const std::string &sql, const std::string &caller, sq3_cb_func cb, void *param) const
{
	if( !this->fd_ptr || !(*this->fd_ptr) ){
		throw std::runtime_error(caller + "failed: No db handle provided");
	} 

	if(sql.empty()){
		throw std::runtime_error(caller + "failed: No SQL provided");
	} 

	char *err = nullptr;

	if(sqlite3_exec(*this->fd_ptr, sql.c_str(), cb, param, &err) != SQLITE_OK){
    	std::string msg = "'" + sql + "' failed: " + err;
    	sqlite3_free(err);
    	throw std::runtime_error(caller + msg);
	}
}

void Base_table::clear()
{
	std::string sql = "DELETE FROM " + name + " ;";
	send_sql(sql, excp_method(""));
	sql = "VACUUM;";
	send_sql(sql, excp_method(""));
}

bool Base_table::add_column_if_not_exists(
	const std::string &col_name, 
	const std::string &col_type, 
	const std::string &col_value)
{
	struct column_info{
		std::string name;
		bool exists = false;
	}ci;
	ci.name = col_name;

	// Колбек вызывается для каждого столбца таблицы. 
	auto callback = [](void *param, int argc, char **argv, char **colname) -> int { 
		log_msg_ns(MSG_TRACE, "|%s|", argv[1]);
		column_info *ci = static_cast<column_info*>(param);
		if(ci->name == argv[1]){
			log_msg_ns(MSG_TRACE, "<-- column found\n");
			ci->exists = true;
		} 
		else{
			log_msg_ns(MSG_TRACE, "\n");
		}
		return 0;
	};

	std::string sql = "PRAGMA table_info(" + this->name + ");";
	log_msg(MSG_TRACE, "Table '%s' contains columns:\n", this->name);

	send_sql(sql, excp_method(""), callback, &ci);

	if(ci.exists){
		log_msg(MSG_VERBOSE | MSG_TO_FILE, "Column '%s' already exists in '%s' table\n", col_name, this->name);
		return true;
	}

	sql = "ALTER TABLE " + this->name + " ADD COLUMN " + col_name + " " + col_type;
	
	if(!col_value.empty()){
		sql += " DEFAULT " + col_value;
	}
	sql += ";";

	send_sql(sql, excp_method(""));

	return false;
}



// --- Таблица системных счетчиков ---

void Counters_table::create()
{
	std::string sql = "CREATE TABLE IF NOT EXISTS " + name + " ( \
type TEXT UNIQUE, \
value INTEGER); \
INSERT OR IGNORE INTO " + name + " VALUES(\"" APP_CNT_TRIP_NUM "\", 0); \
INSERT OR IGNORE INTO " + name + " VALUES(\"" APP_CNT_SYS_EV "\", 0); \
";

	send_sql(sql, excp_method(""));
}

void Counters_table::set(const std::string &cnt_type, uint32_t value)
{
	std::string sql = "UPDATE " + name + " SET value=" ;
	sql += to_s(value) + " WHERE type=\"" + cnt_type + "\";";
	send_sql(sql, excp_method(""));

	// Обновляем кеш 
	this->update_cache(cnt_type, value);
}

uint32_t Counters_table::get(const std::string &cnt_type)
{
	{
		std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
		auto it = this->cache.find(cnt_type);
		if(it != this->cache.end()){
			return it->second;
		}
	}

	uint32_t res = 0;

	std::string sql = "SELECT value FROM " + name + " WHERE type=\"" + cnt_type + "\";" ;

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		sscanf(argv[0], "%" PRIu32 "", (uint32_t*)param);
		return 0; 
	};

	send_sql(sql, excp_method(""), callback, &res);

	this->update_cache(cnt_type, res);
	return res;
}

uint32_t Counters_table::inc_and_get(const std::string &cnt_type)
{
	// Делаем операцию атомарной
	std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);

	uint32_t value = this->get(cnt_type);
	++value;

	this->set(cnt_type, value);

	return value;
}

// --- Таблица метаданных файлов приложения ---

void FilesMetadata_table::create()
{
	std::string sql = "CREATE TABLE IF NOT EXISTS " + name + "( \
type TEXT UNIQUE, \
version TEXT, \
update_date TEXT); \
INSERT OR IGNORE INTO " + name + " VALUES('nsi', \"unknown\", \"no date\"); ";

	send_sql(sql, excp_method(""));
}

std::string FilesMetadata_table::file_type_to_db_type(const std::string &prot_type)
{
	std::string ret;

	if(prot_type == APP_FILE_NSI) return "nsi";
	else if(prot_type == APP_FILE_MEDIA) return "media";

	else throw invalid_file_type("Unsupported file type: '" + prot_type + "'");

	return ret;
}

void FilesMetadata_table::set_fver(const std::string &prot_ft, const std::string &ver, const std::string &date)
{
	std::string type = file_type_to_db_type(prot_ft);
	std::string sql = "UPDATE " + name + " SET version='" + ver + "' WHERE type='" + type + "';";
	send_sql(sql, excp_method(""));
	// Обновляем кеш
	std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
	this->vers_cache[type] = ver;

	if( !date.empty() ){
		set_fdate(prot_ft, date);
	}
}

void FilesMetadata_table::set_fdate(const std::string &prot_ft, const std::string &date)
{
	std::string type = file_type_to_db_type(prot_ft);
	std::string sql = "UPDATE " + name + " SET update_date='" + date + "' WHERE type='" + type + "';";
	send_sql(sql, excp_method(""));

	std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
	this->dates_cache[type] = date;
}

files_versions FilesMetadata_table::get_fvers()
{
	std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
	if( !this->vers_cache.empty() ){
		return this->vers_cache;
	}

	std::string sql = "SELECT type, version FROM " + name + ";";

	// Вызывается для каждой строки отдельно
	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		files_versions *tmp = static_cast<files_versions*>(param);

		tmp->insert( {argv[0], argv[1]} );
		return 0;
	};

	send_sql(sql, excp_method(""), callback, &this->vers_cache);

	return this->vers_cache;
}

std::string FilesMetadata_table::get_fver(const std::string &prot_ft)
{	
	std::string type = file_type_to_db_type(prot_ft);

	{
		std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
		auto it = this->vers_cache.find(type);
		if(it != this->vers_cache.end()){
			return it->second;
		}
	}

	std::string ver;
	std::string sql = "SELECT version FROM " + name + " WHERE type='" + type + "';";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		std::string *tmp = static_cast<std::string*>(param);
		*tmp = argv[0];
		return 0;
	};

	send_sql(sql, excp_method(""), callback, &ver);

	std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
	this->vers_cache[type] = ver;
	return ver;
}

files_versions FilesMetadata_table::get_fdates()
{
	if( !this->dates_cache.empty() ){
		return this->dates_cache;
	}

	std::string sql = "SELECT type, update_date FROM " + name + ";";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		files_versions *tmp = static_cast<files_versions*>(param);

		tmp->insert( {argv[0], argv[1]} );
		return 0;
	};

	send_sql(sql, excp_method(""), callback, &this->dates_cache);

	return this->dates_cache;
}

std::string FilesMetadata_table::get_fdate(const std::string &prot_ft)
{
	std::string type = file_type_to_db_type(prot_ft);

	{
		std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
		auto it = this->dates_cache.find(type);
		if(it != this->dates_cache.end()){
			return it->second;
		}
	}

	std::string date;
	std::string sql = "SELECT update_date FROM " + name + " WHERE type='" + type + "';";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		std::string *tmp = static_cast<std::string*>(param);
		*tmp = argv[0];
		return 0;
	};

	send_sql(sql, excp_method(""), callback, &date);

	std::lock_guard<std::recursive_mutex> lck(this->cache_mtx);
	this->dates_cache[type] = date;
	return date;
}



// --- Таблица системных флагов ---
void DeviceInfo_table::create()
{
	std::string sql = "CREATE TABLE IF NOT EXISTS "  + name + "( \
type TEXT UNIQUE, \
value TEXT); \
INSERT OR IGNORE INTO " + name + " VALUES(\"start_time_unix\", \"0\"); \
INSERT OR IGNORE INTO " + name + " VALUES(\"device_id\", \"0\"); \
INSERT OR IGNORE INTO " + name + " VALUES(\"device_id_src\", \"\"); ";

	send_sql(sql, excp_method(""));
}

void DeviceInfo_table::set_start_time(time_t unix_time)
{
	std::string sql = "UPDATE " + name + " SET value = \"" + to_s(unix_time) + "\" WHERE type=\"start_time_unix\";";
	send_sql(sql, excp_method(""));
}

time_t DeviceInfo_table::get_start_time() const
{
	time_t res = 0;
	std::string sql = "SELECT value FROM " + name + " WHERE type=\"start_time_unix\";";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 

		time_t *tmp = static_cast<time_t*>(param);

		if( !argc ){
			*tmp = 0;
		}

		std::stringstream ss{argv[0]};
		ss >> *tmp;

		return 0;
	};

	send_sql(sql, excp_method(""), callback, &res);
	return res;
}

void DeviceInfo_table::set_device_id(uint32_t id)
{
	std::string sql = "UPDATE " + name + " SET value = \"" + to_s(id) + "\" WHERE type=\"device_id\";";
	send_sql(sql, excp_method(""));
}

uint32_t DeviceInfo_table::get_device_id() const
{
	uint32_t res = 0;
	std::string sql = "SELECT value FROM " + name + " WHERE type=\"device_id\";";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 

		uint32_t *tmp = static_cast<uint32_t*>(param);

		if( !argc ){
			*tmp = 0;
		}

		if(argv[0]){
			sscanf(argv[0], "%" PRIu32 "", tmp);
		}

		return 0;
	};

	send_sql(sql, excp_method(""), callback, &res);
	return res;
}

void DeviceInfo_table::set_device_id_source(const std::string &src)
{
	std::string sql = "UPDATE " + name + " SET value = \"" + src + "\" WHERE type=\"device_id_src\";";
	send_sql(sql, excp_method(""));
}




// Представление основной базы в целом

void MainDatabase::init(const std::string &path, int flags)
{
	if(this->fd){
		// Уже проинициализирована
		return;
	} 	

	if(sqlite3_open_v2(path.c_str(), &this->fd, flags, nullptr)){
		throw std::runtime_error(excp_method("sqlite3_open_v2 failed: " + (std::string)sqlite3_errmsg(fd)));
	}

	// sys_status.set_fd_ptr(&this->fd);
	// sys_status.create();
	// sys_status.load_cache();
	
	sys_counters.set_fd_ptr(&this->fd);
	sys_counters.create();

	// sys_conf.set_fd_ptr(&this->fd);
	// sys_conf.create();

	f_data.set_fd_ptr(&this->fd);
	f_data.create();

	dev_info.set_fd_ptr(&this->fd);
	dev_info.create();

	this->path = path;
}

void MainDatabase::flush()
{
	if( !this->fd ){
		return;
	} 

	// if(sqlite3_db_cacheflush(this->fd) != SQLITE_OK){
	// 	log_err("MainDatabase::flush() failed\n");
	// } 
}

void MainDatabase::deinit()
{
	if( !this->fd ){
		return;
	} 

	sqlite3_close(this->fd);
	this->fd = nullptr;
	this->path = "";
}




// --- Представление БД НСИ

using kRoute = NSIDatabase::kRoute_table;
using kFrames = NSIDatabase::kFrames_table;
using kCfg = NSIDatabase::kCfg_table;

std::recursive_mutex NSIDatabase::db_file_mutex_;
sqlite3 *NSIDatabase::fd_ = nullptr;
std::string NSIDatabase::path_;
kRoute NSIDatabase::kroute_;
kCfg NSIDatabase::kcfg_;
kFrames NSIDatabase::kframe_;
kRoute::routes NSIDatabase::routes_;
kCfg::params NSIDatabase::cfg_params_;
std::pair<kFrames::main_frames, kFrames::child_frames> NSIDatabase::frames_;
std::mutex NSIDatabase::curr_route_mutex_;
int NSIDatabase::curr_route_id_ = -1;

bool NSIDatabase::open(int modes)
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	if(fd_){
		// Уже проинициализирована
		return true;
	} 	

	if(sqlite3_open_v2(path_.c_str(), &fd_, modes, nullptr)){
		return false;
		// throw std::runtime_error(excp_method("sqlite3_open_v2 (" + path_ + ") failed: " + sqlite3_errmsg(fd_)));
	}

	kroute_.set_fd_ptr(&fd_);
	kcfg_.set_fd_ptr(&fd_);
	kframe_.set_fd_ptr(&fd_);

	return true;
}

void NSIDatabase::close()
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	if( !fd_ ){
		return;
	} 

	sqlite3_close(fd_);
	fd_ = nullptr;
}

void NSIDatabase::update(const std::string &path)
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	close();

	if(std::rename(path.c_str(), path_.c_str())){
		throw std::runtime_error(excp_method(std::string("rename() failed: ") + strerror(errno)));
	}
}

// Кодировка строк - UTF-8
kRoute::routes kRoute::read()
{
	routes res;

	const std::string sql = "SELECT id, number, townflag, stops, tariffmin, code, name, tpscode FROM kRoute;";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		routes *res = static_cast<routes *>(param);

		if(argc < 8){
			throw std::runtime_error(excp_method("invalid row size " + std::to_string(argc) + " (expected 8)"));
		}

		route data;
		int id = -1;

		if(argv[0]){
			sscanf(argv[0], "%d", &id);
		}
		if(argv[1]){
			data.number = argv[1]; //utils::cp1251_to_utf8(argv[1]);
		}
		if(argv[2]){
			sscanf(argv[2], "%d", &data.townflag);
		}
		if(argv[3]){
			sscanf(argv[3], "%d", &data.stops);
		}
		if(argv[4]){
			sscanf(argv[4], "%d", &data.tariffmin);
		}
		if(argv[5]){
			sscanf(argv[5], "%d", &data.code);
		}
		if(argv[6]){
			// log_hexdump(MSG_DEBUG, argv[6], strlen(argv[6]), "route.name");
			data.name = argv[6]; // UTF-8   // utils::cp1251_to_utf8(argv[6]);
		}
		if(argv[7]){
			data.tpscode = argv[7]; //utils::cp1251_to_utf8(argv[7]);
		}

		res->insert({id, data});

		return 0;
	};

	send_sql(sql, excp_method(""), callback, &res);

	return res;
}



void kCfg::read_param(const std::string &param_name, params &out)
{
	std::string value;
	const std::string sql = "SELECT CAST(paramMeaning AS TEXT) FROM kCfg \
WHERE CAST(cfgParam as TEXT) =\"" + param_name + "\";";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		std::string *res = static_cast<std::string *>(param);

		if(argc < 1){
			throw std::runtime_error(excp_method("invalid row size " + std::to_string(argc) + " (expected 1)"));
		}

		*res = argv[0]; //utils::cp1251_to_utf8(argv[0]);

		return 0;
	};

	send_sql(sql, excp_method(""), callback, &value);

	if( !value.empty() ){
		out.insert({param_name, value});
	}
}

kCfg::params kCfg::read()
{
	params res;

	this->read_param("VEHICLE", res);
	this->read_param("FIRM_NAME", res);
	this->read_param("dbType", res);
	this->read_param("devType", res);
	this->read_param("dbStructVersion", res);
	this->read_param("dataVersion", res);
	this->read_param("ClockShift", res);
	this->read_param("TNAV", res);
	this->read_param("Route", res);

	return res;
}


// Zones
uint8_t kFrames::Zone::course_to_bitmask(double course_degrees) noexcept
{
	if((course_degrees < 0.0) || (course_degrees > 360.0)){
		return 0x00;
	}

	if((course_degrees == 360.0) || (course_degrees < 45.0)){
		return 0x01;	// 0-й сектор
	}
	else if(course_degrees < 90.0){
		return 0x02;	// 1-й сектор
	}
	else if(course_degrees < 135.0){
		return 0x04;	// 2-й сектор
	}
	else if(course_degrees < 180.0){
		return 0x08;	// 3-й сектор
	}
	else if(course_degrees < 225.0){
		return 0x10;	// 4-й сектор
	}
	else if(course_degrees < 270.0){
		return 0x20;	// 5-й сектор
	}
	else if(course_degrees < 315.0){
		return 0x40;	// 6-й сектор
	}
	else if(course_degrees < 360.0){
		return 0x80;	// 7-й сектор
	}
}

// Проверка курса на соответствие текущей битовой карте направлений
bool kFrames::Zone::course_check(double course) const noexcept
{
	const uint8_t course_bitmask = course_to_bitmask(course);

	if((course_bitmap_ & course_bitmask) == 0){
		// Такой сектор не выставлен
		log_msg(MSG_TRACE, "course mismatch (current: 0x%02X vs bitmap: 0x%02X)\n", course_bitmask, course_bitmap_);
		return false;
	}

	return true;
}

bool kFrames::RectangleZone::contains(const std::pair<double, double> &lat_lon, double course) const
{
	// Признак вхождения точки в прямоугольник заданным 
	// Левым Нижним углом (start) и Правым верхним (end):
	// 
	// end.lon > point.lon  > start.lon
	// 				&&
	// end.lat > point.lat  > start.lat 
	//
	const double &lat = lat_lon.first;
	const double &lon = lat_lon.second;

	if((lon < lon_end_) && (lon > lon_start_) && (lat < lat_end_) && (lat > lat_start_)){

		log_msg(MSG_TRACE, "inside RectangleZone S(%lf, %lf), E(%lf, %lf)\n", lat_start_, lon_start_, lat_end_, lon_end_);

		if(course >= 0.0){
			return course_check(course);
		}

		return true;
	}

	return false;
}

std::string kFrames::RectangleZone::show() const
{ 
	return "S(" + std::to_string(lat_start_) + ", " + std::to_string(lon_start_) + "), E(" +
		std::to_string(lat_end_) + ", " + std::to_string(lon_end_) + "), C:" + std::to_string(course_bitmap_);
}

#define PI			3.14159265
#define EARTH_R 	6372795		// Радиус Земли в метрах

// Сферическая теорема косинусов (d - угловое расстояние между точками А и С)
static double spherical_cosinus_distance(double latA, double longA, double latC, double longC)
{
	const double long_delta = longC - longA;

	// cos(d) = sin(latА)·sin(latC) + cos(latА)·cos(latC)·cos(longА − longC)
	const double d = acos( sin(latA) * sin(latC) + cos(latA) * cos(latC) * cos(long_delta) );

	// Перевод углового расстояния в метрическое 
	// L = d * EARTH_R (meters)
	return d * static_cast<double>(EARTH_R);
}

// Модифицированный гаверсинус (решает проблему точек-антиподов и небольших расстояний)
static double modified_hoversine_distance(double latA, double longA, double latC, double longC)
{
	const double long_delta = longC - longA;
	const double clat1 = cos(latA);
	const double clat2 = cos(latC);
	const double slat1 = sin(latA);
	const double slat2 = sin(latC);
	const double sldelta = sin(long_delta);
	const double cldelta = cos(long_delta);

	const double top = sqrt( pow(clat2 * sldelta, 2) + pow(clat1 * slat2 - slat1 * clat2 * cldelta, 2) );
	const double bot = slat1 * slat2 + clat1 * clat2 * cldelta;

	const double d = atan(top / bot);

	return d * static_cast<double>(EARTH_R);
}

bool kFrames::CircleZone::contains(const std::pair<double, double> &lat_lon, double course) const
{
	// Проверяем вначале курс
	if((course >= 0) && !course_check(course)){
		return false;
	}

	// Преобразование lat, long в радианы
	const double latA = lat_lon.first * PI / 180.0;
	const double longA = lat_lon.second * PI / 180.0;
	const double latC = lat_start_ * PI / 180.0;	// координаты центра 
	const double longC = lon_start_ * PI / 180.0;	// окружности
	
	double distance = 0.0;
	double sec = 0.0;

	// sec = utils::calc_exec_time([&distance, latA, longA, latC, longC](){ 
	// 	distance = spherical_cosinus_distance(latA, longA, latC, longC); 
	// });

	// printf("spherical_cosinus_distance between points (%lf, %lf) and (%lf, %lf) : %lf meters (took: %lf sec)\n",
	// 	lat_lon.first, lat_lon.second, lat_start_, lon_start_, distance, sec);
	
	sec = utils::calc_exec_time([&distance, latA, longA, latC, longC](){ 
		distance = modified_hoversine_distance(latA, longA, latC, longC); 
	});

	// printf("distance_modified_hoversine between points (%lf, %lf) and (%lf, %lf) : %lf meters (took: %lf sec)\n",
	// 	lat_lon.first, lat_lon.second, lat_start_, lon_start_, distance, sec);

	if(distance < radius_){
		return true;
	}

	return false;
}

std::string kFrames::CircleZone::show() const
{ 
	return "S(" + std::to_string(lat_start_) + ", " + std::to_string(lon_start_) + "), R:" +
		std::to_string(radius_) + ", C:" + std::to_string(course_bitmap_);
}


bool operator<(const kFrames::Frame &lhs, const kFrames::Frame &rhs)
{ 
	return lhs.id < rhs.id;
}

bool operator>(const kFrames::Frame &lhs, const kFrames::Frame &rhs)
{ 
	return lhs.id > rhs.id;
}

bool operator==(const kFrames::Frame &lhs, const kFrames::Frame &rhs)
{ 
	return lhs.id == rhs.id;
}

bool operator!=(const kFrames::Frame &lhs, const kFrames::Frame &rhs)
{ 
	return !(lhs == rhs);
}

bool operator<=(const kFrames::Frame &lhs, const kFrames::Frame &rhs)
{ 
	return !(lhs > rhs);
}

bool operator>=(const kFrames::Frame &lhs, const kFrames::Frame &rhs)
{ 
	return !(lhs < rhs);
}


std::pair<kFrames::main_frames, kFrames::child_frames> kFrames::read(int route_id)
{
	std::pair<main_frames, child_frames> res;

	const std::string sql = "SELECT id, lon_start, lat_start, lon_end, lat_end, radius, course, \
play_mode, id_next, is_child, filename, pause FROM kFrames WHERE id_route=" + to_s(route_id) + ";";

	auto callback = [](void *param, int argc, char **argv, char **col_name) -> int { 
		std::pair<main_frames, child_frames> *res = static_cast<std::pair<main_frames, child_frames> *>(param);

		if(argc < 11){
			throw std::runtime_error(excp_method("invalid row size " + std::to_string(argc) + " (expected 11)"));
		}

		Frame frm_data;
		const double NOT_SET = std::numeric_limits<double>::max();
		double lon_start = NOT_SET;
		double lat_start = NOT_SET;
		double lon_end = NOT_SET;
		double lat_end = NOT_SET;
		double radius = 0.0;
		uint8_t course = 0;
		uint8_t is_child = 0;	// 0 не дочерний, 1 - дочерний

		sscanf(argv[0], "%d", &frm_data.id);

		// Считываем описание зоны 
		if(argv[1]){
			sscanf(argv[1], "%lf", &lon_start);
		}
			
		if(argv[2]){
			sscanf(argv[2], "%lf", &lat_start);
		}

		if(argv[3]){
			sscanf(argv[3], "%lf", &lon_end);
		}
		
		if(argv[4]){
			sscanf(argv[4], "%lf", &lat_end);
		}	
		
		if(argv[5]){
			sscanf(argv[5], "%lf", &radius);
		}

		sscanf(argv[6], "%" SCNu8 "", &course);

		// Считываем медиа-данные		
		sscanf(argv[7], "%" SCNu8 "", &frm_data.minfo.play_mode);	

		if(argv[8]){
			sscanf(argv[8], "%d", &frm_data.minfo.id_next);
		}

		sscanf(argv[9], "%" SCNu8 "", &is_child);	
		frm_data.minfo.filename = argv[10];

		sscanf(argv[11], "%d", &frm_data.minfo.pause);

		// Определяем что это за фрейма - основной или дочерний 
		if( !is_child && (lon_start != NOT_SET) && (lat_start != NOT_SET) ){
			// Фрейм оснвной => Распределяем зоны 
			if((lon_end == NOT_SET) || (lat_end == NOT_SET) || (radius > 0.0)){
				std::unique_ptr<Zone> zptr{new CircleZone(lat_start, lon_start, course, radius)};
				frm_data.zone = std::move(zptr);
			}
			else{
				std::unique_ptr<Zone> zptr{new RectangleZone(lat_start, lon_start, course, lat_end, lon_end)};
				frm_data.zone = std::move(zptr);
			}

			res->first.push_back(std::move(frm_data));
		}
		else{
			// Выставлен флаг или нет начала зоны => Фрейм дочерний.
			res->second.insert({frm_data.id, frm_data.minfo});
		}

		return 0;
	};

	send_sql(sql, excp_method(""), callback, &res);

	// Сортировка основных фреймов в порядке возрастания идентификаторов
	std::sort(res.first.begin(), res.first.end());

	return res;	
}

void NSIDatabase::read()
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	try{
		routes_ = kroute_.read();
	}
	catch(const std::exception &e){
		log_err("Could not read kRoutes: %s\n", e.what());
	}

	try{
		cfg_params_ = kcfg_.read();
	}
	catch(const std::exception &e){
		log_err("Could not read kCfg: %s\n", e.what());
	}

	reload_route_frames();	// Маршрут изначально не задан

	show_routes();
	show_cfg_params();
	show_frames();
}

std::vector<std::string> NSIDatabase::get_available_route_names()
{
	std::vector<std::string> res;

	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	for(const auto &elem : routes_){
		res.push_back(elem.second.name);
	}

	return res;
}

void NSIDatabase::select_route(int route_id)
{
	{
		std::lock_guard<std::mutex> lock(curr_route_mutex_);
		curr_route_id_ = route_id;
	}
	
	log_msg(MSG_INFO, "Route %d selected\n", route_id);
}

void NSIDatabase::select_route(const std::string &route_name)
{
	int route_id = -1; 

	for(const auto &elem : routes_){
		if(elem.second.name == route_name){
			route_id = elem.first;
			break;
		}
	}

	if(route_id == -1){
		log_warn("No route_id found for name '%s'\n", route_name);
		return;
	}

	select_route(route_id);
}

void NSIDatabase::reload_route_frames()
{
	int route_id = get_current_route();

	{
		std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

		try{
			frames_ = kframe_.read(route_id);
		}
		catch(const std::exception &e){
			log_err("Could not read kFrames: %s\n", e.what());
		}
	}
}

bool NSIDatabase::check_media_content_presence(const std::string &media_dir)
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	// Check content for Main frames
	for(const auto &frame : frames_.first){
		if( !utils::file_exists(media_dir + "/" + frame.minfo.filename) ){
			log_warn("main frame media '%s' not found\n", frame.minfo.filename);
			return false;
		}
	}

	// Check content for Child frames
	for(const auto &elem : frames_.second){
		if( !utils::file_exists(media_dir + "/" + elem.second.filename) ){
			log_warn("child frame media '%s' not found\n", elem.second.filename);
			return false;
		}
	}

	return true;
}

// bool NSIDatabase::ready()
// {
// 	if( (get_version() != "unknown") && !frames_.first.empty() ){
// 		return true;
// 	}

// 	return false;
// }

void NSIDatabase::show_routes()
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	const int norm_col = 40;
	const int short_col = 6;
	const int tiny_col = 3;

	const int total_col = norm_col * 1 + short_col * 6 + tiny_col * 1 + 9;

	log_msg(MSG_DEBUG, " " + Logging::padding(total_col - 2, "", '_') + " \n");
	log_msg(MSG_DEBUG, "|" + Logging::padding(total_col - 2, " [ kRoutes ] ", '_') + "|\n");
	log_msg(MSG_DEBUG, "|%s|%s|%s|%s|%s|%s|%s|%s|\n",
		Logging::padding(short_col, "ID", '_'), Logging::padding(short_col, "NUMBER", '_'), Logging::padding(tiny_col, "FLG", '_'), 
		Logging::padding(short_col, "STOPS", '_'), Logging::padding(short_col, "TARIFF", '_'), Logging::padding(short_col, "CODE", '_'),
		Logging::padding(norm_col, "NAME", '_'), Logging::padding(short_col, "TPSCD", '_'));

	for(const auto &kv : routes_){
		log_msg(MSG_DEBUG, "|%s|%s|%s|%s|%s|%s|%s|%s|\n", 
			Logging::padding(short_col, std::to_string(kv.first)), Logging::padding(short_col, kv.second.number), 
			Logging::padding(tiny_col, std::to_string(kv.second.townflag)), Logging::padding(short_col, std::to_string(kv.second.stops)),
			Logging::padding(short_col, std::to_string(kv.second.tariffmin)), Logging::padding(short_col, std::to_string(kv.second.code)),
			Logging::padding(norm_col, kv.second.name), Logging::padding(short_col, kv.second.tpscode));
	}
	log_msg(MSG_DEBUG, Logging::padding(total_col, "", '-') + "\n");
}

void NSIDatabase::show_cfg_params()
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	const int norm_col = 36;
	const int total_col = norm_col * 2 + 3;

	log_msg(MSG_DEBUG, " " + Logging::padding(total_col - 2, "", '_') + " \n");
	log_msg(MSG_DEBUG, "|" + Logging::padding(total_col - 2, " [ kCfg ] ", '_') + "|\n");

	for(const auto &kv : cfg_params_){
		log_msg(MSG_DEBUG, "|%s|%s|\n", 
			Logging::padding(norm_col, kv.first), Logging::padding(norm_col, kv.second));
	}

	log_msg(MSG_DEBUG, Logging::padding(total_col, "", '-') + "\n");
}

void NSIDatabase::show_frames()
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	const int big_col = 58;
	const int norm_col = 14;
	const int short_col = 6;
	const int tiny_col = 3;

	const int total_col = big_col * 1 + norm_col * 1 + short_col * 2 + tiny_col * 2 + 7;

	log_msg(MSG_DEBUG, " " + Logging::padding(total_col - 2, "", '_') + " \n");
	log_msg(MSG_DEBUG, "|" + Logging::padding(total_col - 2, " [ kFrames (id_route: " + to_s(get_current_route()) + ") ] ", '_') + "|\n");
	log_msg(MSG_DEBUG, "|%s|%s|%s|%s|%s|%s|\n",
		Logging::padding(short_col, "ID", '_'), Logging::padding(big_col, "ZONE", '_'), 
		Logging::padding(tiny_col, "MOD", '_'), Logging::padding(short_col, "NEXT", '_'), 
		Logging::padding(norm_col, "FILE", '_'), Logging::padding(tiny_col, "P", '_') );

	log_msg(MSG_DEBUG, "|" + Logging::padding(total_col - 2, " Main Frames ", '*') + "|\n");
	for(const auto &frame : frames_.first){
		log_msg(MSG_DEBUG, "|%s|%s|%s|%s|%s|%s|\n", 
			Logging::padding(short_col, std::to_string(frame.id)), Logging::padding(big_col, frame.zone->show()), 
			Logging::padding(tiny_col, std::to_string(frame.minfo.play_mode)), Logging::padding(short_col, std::to_string(frame.minfo.id_next)), 
			Logging::padding(norm_col, frame.minfo.filename), Logging::padding(tiny_col, std::to_string(frame.minfo.pause)) );
	}

	log_msg(MSG_DEBUG, "|" + Logging::padding(total_col - 2, " Child Frames ", '*') + "|\n");
	for(const auto &frame : frames_.second){
		log_msg(MSG_DEBUG, "|%s|%s|%s|%s|%s|%s|\n", 
			Logging::padding(short_col, std::to_string(frame.first)), Logging::padding(big_col, ""), 
			Logging::padding(tiny_col, std::to_string(frame.second.play_mode)), Logging::padding(short_col, std::to_string(frame.second.id_next)), 
			Logging::padding(norm_col, frame.second.filename), Logging::padding(tiny_col, std::to_string(frame.second.pause)) );
	}

	log_msg(MSG_DEBUG, Logging::padding(total_col, "", '-') + "\n");
}

std::string NSIDatabase::get_version()
{ 
	return get_cfg_param<std::string>("dataVersion", "unknown");
}

//
const kFrames::MediaInfo* NSIDatabase::find_media_info(
	const std::pair<double, double> &lat_lon, double course, int *frame_id)
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	// Для отслеживания попадания в один и тот же фрейм используем 
	// сохраненное значение предыдущего обработанного фрейма. Если мы
	// все еще в той же зоне, что и прежде - выходим не проверяя оставшиеся
	const size_t UNDEFINED = std::numeric_limits<size_t>::max();
	static size_t prev_frame_idx = UNDEFINED;

	const auto &m_frames = frames_.first;

	// Проверить попадание в зону ранее обработанного фрейма (курс не учитывается)
	if(prev_frame_idx != UNDEFINED){

		// Проверяем только координаты, без курса , чтобы не отслеживать 
		// повороты и не срабатывать ложно повторно
		if(m_frames[prev_frame_idx].zone && m_frames[prev_frame_idx].zone->contains(lat_lon, course)){
			// Мы по прежнему в зоне, которая уже была обработана
			return nullptr;
		}
	}

	// Проходимся по каждому фрейму в поисках вхождения переданых координат
	// в одну из зон. Помечаем фрейм как обработанный, возвращаем медиа-инфо.
	for(size_t i = 0; i < m_frames.size(); ++i){
		const auto &frame = m_frames[i];

		if(frame.zone && frame.zone->contains(lat_lon, course)){
			prev_frame_idx = i;
			log_info("Entering zone id: %d (lat: %lf, lon: %lf, course: %.2lf)\n", frame.id, lat_lon.first, lat_lon.second, course);
			if(frame_id){
				*frame_id = frame.id;
			}
			return &frame.minfo;
		}
	}

	// Нет попадания ни в одну из зон
	if(prev_frame_idx != UNDEFINED){
		log_info("Exiting zone id: %d (lat: %lf, lon: %lf, course: %.2lf)\n", m_frames[prev_frame_idx].id, lat_lon.first, lat_lon.second, course);
		if(frame_id){
			// Признак выхода из предыдущего фрейма
			*frame_id = -1;
		}
	}

	prev_frame_idx = UNDEFINED;
	return nullptr;
}

const kFrames::MediaInfo* NSIDatabase::get_media_info_of_child(int id)
{
	std::lock_guard<std::recursive_mutex> lck(db_file_mutex_);

	auto it = frames_.second.find(id);
	if(it == frames_.second.end()){
		log_warn("No child media_info found for id %d\n", id);
		return nullptr;
	}

	return &(it->second);
}


} //namespace avi

#ifdef _APP_DB_TEST

#include <cassert>

int main(int argc, char* argv[])
{
	logger.init(MSG_TRACE, "db.test.log", 0, KB_to_B(1));

	try {

		assert( sqlite3_libversion_number() == SQLITE_VERSION_NUMBER );

		log_msg(MSG_DEBUG, "SQLITE version: %s\n", sqlite3_version);
		log_msg(MSG_DEBUG, "SQLITE threadsafe mode: %s\n", sqlite3_threadsafe() ? "true" : "false");

		app::MainDatabase db;
		db.init("./app.db", DB_RW | DB_FMTX | DB_CREATE);

		// app::SystemStatus_table::record rec;
		// db.sys_status.put(rec);


		// db.sys_counters.add_column_if_not_exists("test_cnt", "INTEGER NOT NULL", "43");
		// db.sys_counters.add_column_if_not_exists("badaboom", "INTEGER", "-111");
		// db.sys_status.add_column_if_not_exists("pipka", "TEXT");
		// db.sys_status.add_column_if_not_exists("test_col", "TEXT", "'text'");

		db.f_data.set_fver(APP_FILE_NSI, "1.2.3");
		std::string ver = db.f_data.get_fver(APP_FILE_NSI);
		log_msg(MSG_DEBUG, "f_data(%s) " _GREEN  "OK" _RESET " - set_fver: '%s'\n", APP_FILE_NSI, ver);

		db.sys_counters.set(APP_CNT_TRIP_NUM, 5948390);
		db.sys_counters.set(APP_CNT_SYS_EV, 123456789);
		uint32_t trip_num = db.sys_counters.get(APP_CNT_TRIP_NUM);
		uint32_t sev_cnt = db.sys_counters.get(APP_CNT_SYS_EV);
		log_msg(MSG_DEBUG, "sys_counters(%s) " _GREEN  "OK" _RESET " - set: %" PRIu32 "\n", APP_CNT_TRIP_NUM, trip_num);
		log_msg(MSG_DEBUG, "sys_counters(%s) " _GREEN  "OK" _RESET " - set: %" PRIu32 "\n", APP_CNT_SYS_EV, sev_cnt);

		db.sys_flags.set_start_time(time(nullptr));
		time_t stime = db.sys_flags.get_start_time();
		log_msg(MSG_DEBUG, "sys_flags(start_time) " _GREEN  "OK" _RESET " - set: %llu\n", stime);

		db.deinit();
	}
	catch(const std::runtime_error& e){
		log_excp("%s\n", e.what());
		return 1;
	}

	return 0;
}
#endif
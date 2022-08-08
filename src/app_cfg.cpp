#include <memory>
#include <stdexcept>
#include <sstream>
#include <unordered_map>

// Используется C API
extern "C"{
#include <libconfig.h>
}

#define LOG_MODULE_NAME		"[ CFG ]"
#include "logger.hpp"

#include "app_cfg.hpp"

#define HEADER_PADDING		76

//
#define LOOKUP_AND_SET_BOOL(name, dest, dim) do{ 												\
	int tmp = 0;																				\
	if( config_lookup_bool(&cfg, name, &tmp) ){													\
		dest = tmp ? true : false;																\
		log_msg(this->log_level, "-- Set " _BOLD "%s" _RESET " to '%s'\n", name, dest); 		\
	}																							\
	else{																						\
		warn_to_log(std::string(_BOLD) + name + _RESET + " not found, using default (" + (dest ? "True" : "False") + ")"); \
	}																							\
}while(0)

//
#define LOOKUP_AND_SET_STR(name, dest, dim) do{ 												\
	const char *str = nullptr;																	\
	if( config_lookup_string(&cfg, name, &str) ){												\
		dest = str;																				\
		log_msg(this->log_level, "-- Set " _BOLD "%s" _RESET " to '%s' %s\n", name, dest, dim);	\
	}																							\
	else{																						\
		warn_to_log(std::string(_BOLD) + name + _RESET + " not found, using default (" + dest + ")"); \
	}																							\
}while(0)
		
//
#define LOOKUP_AND_SET_INT(name, dest, dim) do{ 												\
	if( config_lookup_int(&cfg, name, &(dest)) ){												\
		log_msg(this->log_level, "-- Set " _BOLD "%s" _RESET " to %d %s\n", name, dest, dim);	\
	}																							\
	else{																						\
		warn_to_log(std::string(_BOLD) + name + _RESET + " not found, using default (" + std::to_string(dest) + ")"); \
	}																							\
}while(0)
	
//
#define LOOKUP_AND_SET_INT64(name, dest, dim) do{ 												\
	if( config_lookup_int64(&cfg, name, &(dest)) ){												\
		log_msg(this->log_level, "-- Set " _BOLD "%s" _RESET " to %lld %s\n", name, dest, dim);	\
	}																							\
	else{																						\
		warn_to_log(std::string(_BOLD) + name + _RESET + " not found, using default (" + std::to_string(dest) + ")"); \
	}																							\
}while(0)

#define LOOKUP_AND_SET_DOUBLE(name, dest, dim) do{ 												\
	if( config_lookup_float(&cfg, name, &(dest)) ){												\
		log_msg(this->log_level, "-- Set " _BOLD "%s" _RESET " to %lf %s\n", name, dest, dim);	\
	}																							\
	else{																						\
		warn_to_log(std::string(_BOLD) + name + _RESET + " not found, using default (" + std::to_string(dest) + ")"); \
	}																							\
}while(0)

// Установка размера в МБ
#define LOOKUP_AND_SET_MB(name, dest) do{ 			\
	int tmp = utils::B_to_MB(dest);					\
	LOOKUP_AND_SET_INT(name, tmp, "[MB]");			\
	dest = utils::MB_to_B(tmp);						\
}while(0)

#define LOOKUP_AND_SET_KB(name, dest) do{ 			\
	int tmp = utils::B_to_KB(dest);					\
	LOOKUP_AND_SET_INT(name, tmp, "[KB]");			\
	dest = utils::KB_to_B(tmp);						\
}while(0)

namespace avi{

// Чтобы сохранить итерфейс класса независимым от используемого API библиотеки 
// не определяем хендлы внутри класса
static config_t cfg;              // Хэндл конфиг файла

static void warn_to_log(const std::string &msg)
{
	static bool first_message = true;

	if(first_message){
		log_msg(MSG_TO_FILE, "\n");
		first_message = false;
	}
	
	log_warn("%s\n", msg);
}

// Определение местоположения файла конфигурации и инициализация при нахождении
std::string Config::open(const std::string &file_name)
{
	config_init(&cfg);
	this->opened = true;

	std::unordered_map<std::string, std::string> errors;

	auto add_error_info = [&errors](const std::string &fname){
		errors[fname] = config_error_text(&cfg);

		if(config_error_type(&cfg) == CONFIG_ERR_PARSE){
			errors[fname] += ": " + std::to_string(config_error_line(&cfg));
		}
	};

	// Поиск системных файлов конфига (по умолчанию)
	for(size_t i = 0; i < default_paths.size(); ++i){
		if(config_read_file(&cfg, default_paths[i].c_str()) == CONFIG_TRUE){
			this->file_path = default_paths[i];
			return default_paths[i];
		}
		else{
			add_error_info(default_paths[i]);
		} 
	}

	// Использование пользовательского файла конфига
	if(config_read_file(&cfg, file_name.c_str()) == CONFIG_TRUE){
		this->file_path = file_name;
		return file_name;
	}

	add_error_info(file_name);

	warn_to_log("Couldn't read any config file:");
	for(const auto &fname : default_paths){
		warn_to_log("sys config (" + errors.at(fname) + ") - '" + fname + "'");
	}
	warn_to_log("usr config (" + errors.at(file_name) + ") - '" + file_name + "'");

	return "";
}

// Деинициализация хэндла
void Config::close()
{
	if(this->opened){
		config_destroy(&cfg);
		this->file_path.clear();
		this->opened = false;
	} 
}


std::string Config::read_app_settings(AVI::Settings &out, AVI::Directories &dirs)
{
	log_msg(this->log_level, "%s\n", Logging::padding(HEADER_PADDING, " [ System config ] ", '-'));

	// По умолчанию целые числа в конфиг файле воспринимаются как int \ uint. Поэтому
	// для чтения других типов целых используем промежуточную переменную для 
	// дальнейшего преобразования в нужные типы.
	// Если указанного поля в конфиге нет, используется значение по-умолчанию.
	int tmp = out.log_level;	
	LOOKUP_AND_SET_INT("log_level", tmp, "");
	if((tmp >= MSG_SILENT) && (tmp <= MSG_TRACE)) out.log_level = tmp;
	LOOKUP_AND_SET_KB("log_max_size", out.log_max_size);
	LOOKUP_AND_SET_KB("log_backup_max_size", out.log_backup_max_size);
	
	LOOKUP_AND_SET_INT("sys_events_level", out.sys_ev_level, "");

	LOOKUP_AND_SET_KB("main_db_max_size", out.main_db_max_size);

	// out.update_command = dirs.base_dir + "/update.sh &";
	// LOOKUP_AND_SET_STR("update_command", out.update_command, "");

	// LOOKUP_AND_SET_STR("i2c_dev", out.i2c_dev, "");
	// LOOKUP_AND_SET_STR("net_iface_name", out.net_iface_name, "");

	// Настройки директорий
	LOOKUP_AND_SET_STR("log_path", dirs.log_path, "");
	LOOKUP_AND_SET_STR("log_backup_dir", dirs.log_backup_dir, "");
	LOOKUP_AND_SET_STR("main_db_path", dirs.main_db_path, "");
	LOOKUP_AND_SET_STR("data_dir", dirs.data_dir, "");
	dirs.reset_data_subdirs();
	LOOKUP_AND_SET_STR("media_dir", dirs.media_dir, "");

	// Настройки подмодулей
	LOOKUP_AND_SET_INT("lc_poll_period", out.lc_poll_period, "[sec]");
	LOOKUP_AND_SET_INT("lc_download_period", out.lc_download_period, "[sec]");
	LOOKUP_AND_SET_INT("gps_poll_period_ms", out.gps_poll_period_ms, "[millisec]");
	LOOKUP_AND_SET_INT("gps_valid_threshold", out.gps_valid_threshold, "");
	LOOKUP_AND_SET_DOUBLE("gps_min_valid_speed", out.gps_min_valid_speed, "[km/h]");
	LOOKUP_AND_SET_STR("gps_gen_path", dirs.gps_gen_path, "");
	LOOKUP_AND_SET_STR("gps_track_path", dirs.gps_track_path, "");

	LOOKUP_AND_SET_INT("lcd_backlight_timeout", out.lcd_backlight_timeout, "[sec]");
	LOOKUP_AND_SET_DOUBLE("btn_long_press_sec", out.btn_long_press_sec, "[sec]");

	return this->file_path;	
}


std::string Config::read_lcc_settings(LC_client::settings &out, LC_client::directories &dirs)
{
	log_msg(this->log_level, "%s\n", Logging::padding(HEADER_PADDING, " [ LC_client config ] ", '-'));
	// Настройки библиотеки клиента
	int tmp = out.lsets.log_lvl;	
	LOOKUP_AND_SET_INT("lc_log_level", tmp, "");
	if((tmp >= MSG_SILENT) && (tmp <= MSG_TRACE)) out.lsets.log_lvl = tmp;
	LOOKUP_AND_SET_KB("lc_log_size", out.lsets.log_max_fsize);
	tmp = -1;
	LOOKUP_AND_SET_INT("lc_log_num", tmp, "");
	if( (tmp != -1) && (tmp < 100) ) out.lsets.max_files_num = static_cast<uint32_t>(tmp);
	LOOKUP_AND_SET_STR("lc_log_path", out.lsets.log_fname, "");
	LOOKUP_AND_SET_STR("lc_device_type", out.device_type, "");
	LOOKUP_AND_SET_STR("lc_server_url", out.lc_server_url, "");
	LOOKUP_AND_SET_INT("lc_server_timeout", out.server_tmout, "[sec]");
	LOOKUP_AND_SET_INT("lc_get_tries", out.get_data_tries, "");
	LOOKUP_AND_SET_INT("lc_send_tries", out.put_data_tries, "");
	LOOKUP_AND_SET_INT("lc_max_send_files_num", out.max_put_files_num, "");
	LOOKUP_AND_SET_MB("lc_backup_max_size", out.max_sent_data_size);
	LOOKUP_AND_SET_KB("lc_send_chunk_size", out.put_chunk_size);
	LOOKUP_AND_SET_BOOL("lc_ssl_check", out.ssl_check, "");
	// Разрешения
	LOOKUP_AND_SET_BOOL("lc_get_nsi", out.get["device_tgz"].enabled, "");
	LOOKUP_AND_SET_BOOL("lc_get_stoplist", out.get["device_tgz_stoplist"].enabled, "");
	LOOKUP_AND_SET_BOOL("lc_get_magiclist", out.get["device_tgz_magiclist"].enabled, "");
	LOOKUP_AND_SET_BOOL("lc_get_frmw_validator", out.get["firmware_usk04"].enabled, "");
	LOOKUP_AND_SET_BOOL("lc_get_frmw_suv", out.get["firmware_suv"].enabled, "");
	LOOKUP_AND_SET_BOOL("lc_get_unblock", out.perms.unblock, "");
	LOOKUP_AND_SET_BOOL("lc_send_data", out.perms.put_data, "");

	// Директории
	LOOKUP_AND_SET_STR("lc_put_data_dir", dirs.put_data_dir, "");
	dirs.put_data_tmp_dir = dirs.put_data_dir + "/tmp";
	dirs.put_data_isolation_dir = dirs.put_data_dir + "/broken";
	LOOKUP_AND_SET_STR("lc_backup_dir", dirs.sent_data_dir, "");

	// Приоритеты отправки файлов транзакций
	config_setting_t *pset = config_lookup(&cfg, "lc_sending_order");
	if( pset && (config_setting_is_array(pset) == CONFIG_TRUE) ){

		int len = config_setting_length(pset);
		std::vector<std::string> vec;

		for(int i = 0; i < len; ++i){
			std::string trans_name{config_setting_get_string_elem(pset, i)};
			vec.push_back(trans_name);
		}

		out.set_sending_order(vec);
	}

	return this->file_path;	
}

}//namespace avi

#ifdef _APP_CFG_TEST
int main(int argc, char *argv[])
{
	try{
		std::string path = "test.conf";

		if(argc >= 2){
			path = argv[1];
		} 

		log_lvl_t cfg_lvl = MSG_DEBUG;

		if((argc == 3) && (!strcmp(argv[2], "-v"))){
			cfg_lvl = MSG_VERBOSE;
		}

		logger.init(cfg_lvl, "conf.log", 0, KB_to_B(1));
		app::Config cfg{path};
		cfg.log_level = cfg_lvl;

		app::Settings sett;
		app::Directories dirs;

		log_msg(MSG_DEBUG, "Test starts. Using config file: %s\n", cfg.get_file_path());

		std::string conf_file = cfg.read_app_settings(sett, dirs);

		log_msg(MSG_DEBUG, "OK\n");
	}
	catch(const std::exception &e){	
		log_excp("%s\n", e.what());
	}

	return 0;
}
#endif
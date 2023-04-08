#include <stdexcept>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <cinttypes>
#include <thread>
#include <chrono>

#define LOG_MODULE_NAME		"[ APP ]"
#include "logger.hpp"

#include "platform.hpp"

#include "utils/fs.hpp"
#include "utils/datetime.hpp"
#include "utils/crypto.hpp"
#include "lc_trans.hpp"
#include "lc_sys_ev.hpp"
#include "lc_client.hpp"
#include "announ.hpp"
#include "app_cfg.hpp"
#include "app_db.hpp"
#include "app_lc.hpp"
#include "app.hpp"


namespace avi
{

void AVI::Directories::create() const
{
	// Локальные директории (рядом с исполняемым файлом)
	utils::make_new_dir(backup_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	
	// Внешние директории (путь может быть настроен в конфиге) - хранилища большого объема
	utils::make_new_dir(data_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	utils::make_new_dir(updates_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	utils::make_new_dir(tmp_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	utils::make_new_dir(media_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	utils::make_new_dir(log_backup_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	utils::make_new_dir(utils::get_dir_name(main_db_path), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	utils::make_new_dir(utils::get_dir_name(nsi_db_path), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

void AVI::Directories::reset_data_subdirs()
{
	updates_dir = data_dir + "/updates";
	nsi_db_path = data_dir + "/nsi.db";
	media_dir = data_dir + "/media";
	tmp_dir = data_dir + "/tmp";
	gps_track_path = data_dir + "/gps.track";
}

void AVI::DeviceId::generate_from(const std::string &in)
{
	uint32_t id = utils::crc32_wiki_inv(0, in.c_str(), in.length());
	this->set(id);
}

void AVI::create_sys_event(const std::string &ev_name, const std::string &ev_data) const
{
	// Поддериваемые Системные события
	const std::map<std::string, lc::sys_event::meta> sys_ev_map = {
		{"POWER_ON", {EV_LVL_BRIEF, "0B", EV_DEV_AVI} },			// Включение системы
		{"POWER_OFF", {EV_LVL_BRIEF, "0C", EV_DEV_AVI} },			// Выключение системы
		{"LC_OFFLINE", {EV_LVL_BRIEF, "16", EV_DEV_AVI} },			// Отсутствует связь с сервером личного кабинета (ЛЦ)
		{"LC_EXCH_ERR", {EV_LVL_BRIEF, "1D", EV_DEV_AVI} },			// Ошибка при загрузке/выгрузке данных из ЛЦ
		{"LC_GOT_" APP_FILE_NSI, {EV_LVL_BRIEF, "1F", EV_DEV_AVI} },// Получена база НСИ из ЛЦ
	};

	try{
		lc::sys_event::meta event_info = sys_ev_map.at(ev_name);

		// Проверяем необходимость заполнения структуры события заранее 
		if(lc::ProtoTransactions::sys_events.get_level() < event_info.level){
			return;
		}

		lc::sys_event sev(event_info);
		const Navigator::position pos = announ_task.get_position();
					
		sev.gps_valid = static_cast<uint32_t>(pos.valid);
		sev.gps_latitude = std::to_string(pos.latitude);
		sev.gps_longitude = std::to_string(pos.longitude);

		// Нумерация системных событий сквозная. Для синхронизации доступа из разных частей
		// приложения - инкремент, получение и сохранение счетчика в БД - одна целостная операция
		uint32_t psutrans = mdb.sys_counters.inc_and_get(APP_CNT_SYS_EV);

		lc::ProtoTransactions::sys_events.create(psutrans, sev, ev_data);

		log_info("Sys event %" PRIu32 " created: '%s', data: '%s'\n", psutrans, ev_name, ev_data);
	}
	catch(const std::out_of_range &e){
		log_excp("Unsupported system event key: '%s'\n", ev_name);
	}
	catch(const std::exception &e){
		log_err("'%s' - %s\n", ev_name, e.what());
	}
}

// ПРИМ: Не должен использовать запись логов в файл воизбежании рекурсии,
void AVI::backup_log_file(void *obj)
{
	const AVI *that = static_cast<AVI*>(obj); 

	if(utils::get_dir_size(that->dirs.log_backup_dir) >= that->settings.log_backup_max_size){
		// Очистка старых логов (половина старых файлов удаляется)
		uint32_t num = utils::get_files_num_in_dir(that->dirs.log_backup_dir) / 2;
		utils::remove_head_files_from_dir(that->dirs.log_backup_dir, num, false);
	}

	// Ротация текущего лог файла
	std::string fname = utils::get_fmt_local_datetime("%y%m%d_%H%M%S") + "_" APP_NAME "_" + 
		std::to_string(that->dev_id.get()) + ".rotated.log.gz";

	std::string cmd = "gzip -c " + that->dirs.log_path + " > " + that->dirs.log_backup_dir + "/" + fname;

	utils::exec(cmd);
}

// Загрузка данных НСИ в память
bool AVI::nsi_reload() const
{
	bool res = false;

	if(NSIDatabase::open()){
		NSIDatabase::read();
		res = true;

		// Обновляем содержимое меню выбора маршрута 
		std::vector<std::string> route_names = NSIDatabase::get_available_route_names();
		LCD_Interface::route_selection_menu.update_content(std::move(route_names));		
	}
	else{
		log_warn("Couldn't open NSI (%s)\n", this->dirs.nsi_db_path);
	}

	NSIDatabase::close();
	return res;
}

// Преинициализация приложения.
// Запуск подмолуей независимых от НСИ:
// - Логгер
// - Настройки из файла конфигурации
// - Директории
// - Периферия
// - БД внутреннего состояния
// - Идентификатор устройства
// - Загрузка НСИ в память
void AVI::preinit(bool conf_trace, bool first_time_called)
{
	try{
		// Уровень логирования процесса конфигурации
		log_lvl_t conf_log_level = conf_trace ? MSG_VERBOSE | MSG_TO_FILE : MSG_SILENT;
		// Инициализируем логирование чтобы видеть попытки поиска конфиг файла и не найденные настройки
		logger.init(conf_log_level, dirs.base_dir + "/config.warn", 0, KB_to_B(100));
		Config conf(dirs.config_path);
		conf.log_level = conf_log_level;
		this->dirs.config_path = conf.read_app_settings(this->settings, this->dirs);
		conf.read_lcc_settings(this->lc_task.sets, this->lc_task.dirs);
		conf.close();

		// Переинициализируем логирование с учетом конфига и назначаем обработчик ротации лог файла
		logger.init(settings.log_level, dirs.log_path, 0, settings.log_max_size);
		logger.set_rotation_callback(AVI::backup_log_file, this);

		if(first_time_called) log_msg(MSG_DEBUG | MSG_TO_FILE, _BOLD "~ %s log started (version: '%s', build-time: %s %s) ~\n" _RESET, APP_NAME, APP_VERSION, __DATE__, __TIME__);

		platform::init(this->settings.gps_poll_period_ms, dirs.gps_gen_path);
		std::string imei = platform::get_IMEI();

		if( !dirs.config_path.empty() ){ log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "Config file path:\t\t" _BOLD "'%s'\n" _RESET, dirs.config_path); } 
		else{ log_warn("No config file found, using default settings. See config.warn for details.\n"); }
		log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "Data dir:\t\t\t" _BOLD "'%s'\n" _RESET, dirs.data_dir);
		log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "Media data dir:\t\t" _BOLD "'%s'\n" _RESET, dirs.media_dir);
		log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "Main database path:\t" _BOLD "'%s'\n" _RESET, dirs.main_db_path);
		log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "NSI database path:\t\t" _BOLD "'%s'\n" _RESET, dirs.nsi_db_path);
		log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "System events directory:\t" _BOLD "'%s'\n" _RESET, lc_task.dirs.put_data_dir);
		log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "System events level:\t" _BOLD "%d (%s)\n" _RESET, settings.sys_ev_level, lc::event_level_as_str(settings.sys_ev_level));
		log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "LC-Client period:\t\t" _BOLD "%d sec\n" _RESET, settings.lc_poll_period);

		// Создание рабочих директорий
		this->dirs.create();

		// Создание (открытие) баз данных
		this->mdb.init(this->dirs.main_db_path, DB_RW | DB_FMTX | DB_CREATE);
		// check_maindb_size(settings.main_db_max_size);

		// Если идентификатор еще не был задан, генерируем. Иначе - используем сохраненное в БД значение 
		uint32_t id = this->mdb.dev_info.get_device_id();
		if(id == 0){
			this->dev_id.generate_from(imei);
			this->mdb.dev_info.set_device_id(dev_id.get());
			this->mdb.dev_info.set_device_id_source(imei);
			log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "Device id:\t\t\t" _BOLD "%" PRIu32 " (from IMEI: %s)\n" _RESET, dev_id.get(), imei);
		}
		else{
			this->dev_id.set(id);
			log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "Device id:\t\t\t" _BOLD "%" PRIu32 " (from DB)\n" _RESET, dev_id.get());
		}

		NSIDatabase::set_path(this->dirs.nsi_db_path);
		if(this->nsi_reload()){
			log_msg(MSG_DEBUG | MSG_TO_FILE, _GREEN "NSI version:\t\t" _BOLD "'%s'\n" _RESET, NSIDatabase::get_version());
		}
	}
	catch(const std::exception &e){
		throw std::runtime_error(excp_method(e.what()));
	}
}

// Инициализация работы с меню 
void AVI::interface_init()
{
	try{
		// Реакция на выбор маршрута (выполняется в потоке обработчике прерываний 
		// кнопки и не должна содержать "тяжелых" вызовов)
		LCD_Interface::route_selection_menu.on_select = [this](std::string s){ 
			const std::string &value = s.empty() ? "XXXX" : s;
			// NSIDatabase::select_route(std::stoi(s));
			NSIDatabase::select_route(s);
			// LCD_Interface::main_menu.update_content(0, "МАРШРУТ: " + value);
			LCD_Interface::main_menu.update_ticker_content(0, value);
			this->data_check();
		};

		// Заполняем информационное меню текущими значениями
		this->iface.update_info_menu = [this]() -> std::vector<std::string> 
		{
			std::vector<std::string> res;
			res.emplace_back( "версия: " APP_VERSION );
			res.emplace_back( "ид: " + std::to_string(this->dev_id.get()) );
			res.emplace_back( "громкость: " + std::to_string(platform::audio_get_gain_level()) );

			// TODO
			res.emplace_back( "маршрут: " + std::to_string(NSIDatabase::get_current_route()));
			res.emplace_back( "сервер: " );
			res.emplace_back( "порт: " );

			return res;
		};

		this->iface.prepare();
		this->iface.run();
	}
	catch(const std::exception &e){
		throw std::runtime_error(excp_method(e.what()));
	}
}

// Инициализация клиента Локального Центра
void AVI::lc_client_init()
{
	try{
		if(this->settings.lc_poll_period <= 0){
			log_warn("LC-Client is disabled\n");
			return;
		} 

		this->lc_task.global_init();
		this->lc_task.init();

		// Запускаться будет в режиме скачивания файлов приложения
		this->lc_task.enter_download_mode();

		// Реакция на получение обновления НСИ
		this->lc_task.on_nsi_update = [this](const std::string &path, const std::string &ver){
			log_info("Updating nsi on the disk\n");
			NSIDatabase::update(path);
			// Останавливаем задачи, зависимые от НСИ
			this->announ_task.stop();
			this->announ_task.wait();

			this->nsi_reload();
		};

		// Реакция на получение непустого медиа-листа
		this->lc_task.on_media_list_update = [this](const std::string &path, const std::string &ver){
			this->announ_task.stop();
			this->announ_task.wait();
		};

		// TODO: Реакция на обновление приложения
		this->lc_task.on_software_update = nullptr;

		// Реакция на окончание процедуры скачивания файлов приложения
		this->lc_task.on_download_finished = [this](){
			this->data_check();
		};

		// Запускаем поток клиента ЛЦ
		this->lc_task.set_period(this->settings.lc_poll_period);
		this->lc_task.start(true);
	}
	catch(const std::exception &e){
		throw std::runtime_error(excp_method(e.what()));
	}
}

void AVI::init(bool conf_trace)
{
	std::lock_guard<std::mutex> lock{this->init_phase_mutex};
	static bool first_call = true;

	this->preinit(conf_trace, first_call);
	
	this->interface_init();

	this->iface.display(&LCD_Interface::init_phase_menu, {{1, std::string("Версия: ") + APP_VERSION}});

	// Инициализация модуля транзакций и системных событий 
	lc::ProtoTransactions::init(this->lc_task.dirs.put_data_dir, this->dev_id.get(), this->dev_id.get(), this->lc_task.sets.device_type);
	lc::ProtoTransactions::check_and_clean();
	lc::ProtoTransactions::sys_events.init(this->settings.sys_ev_level);

	// Создаем Системное событие о запуске
	if(first_call){
		// FIXME: empty gps coordinates
		this->create_sys_event("POWER_ON", APP_VERSION);
	}

	this->lc_client_init();

	first_call = false;
}


// Определение готовности устройства к работе
void AVI::data_check()
{	
	// Проверка наличия данных БД НСИ и медиаконтента
	if( !NSIDatabase::ready() ){
		// Базы НСИ нет, не с чем работать
		log_warn("NSI database is not ready\n");
		this->wait_for_data("базы");
		return;
	}

	// База либо обновилась после скачивания с сервера, 
	// либо использовалась найденная на диске

	// Проверка наличия медиа файлов
	if( !utils::get_dir_size(dirs.media_dir) ){
		log_warn("No media-content found at '%s'\n", dirs.media_dir);
		this->wait_for_data("медиа");
		return;
	}

	// Проверка выбран ли маршрут
	if( !NSIDatabase::route_selected() ){
		log_warn("NSI route is not selected\n");
		this->wait_for_route_selection();
		return;
	}

	// 
	// 
	this->announ_task.stop();
	this->announ_task.wait();

	NSIDatabase::open();
	NSIDatabase::reload_route_frames();
	NSIDatabase::close();
	NSIDatabase::show_frames();

	log_info("Selected route: %d\n", NSIDatabase::get_current_route());

	// Проверка наличия необходимых медиа файлов с учетом выбранного маршрута
	if( !NSIDatabase::check_media_content_presence(dirs.media_dir) ){
		log_warn("Not all media-content for selected route found\n");
		this->wait_for_data("медиа");
		return;
	}
	
	// Переход в штатный режим работы
	this->regular_mode();
}

void AVI::wait_for_data(const std::string &data_type)
{
	log_msg(MSG_DEBUG | MSG_TO_FILE, _BOLD "~ %s waiting for data from LC server ~\n" _RESET, APP_NAME);

	this->iface.display(&LCD_Interface::error_menu, {{0, "нет " + data_type + " данных"}});

	// Перевести клиент ЛЦ в режим запроса файлов и ждать очередного вызова data_check()
	this->lc_task.enter_download_mode();

	this->dev_state.set("Ожидание " + data_type + " данных");
}

void AVI::wait_for_route_selection()
{
	log_msg(MSG_DEBUG | MSG_TO_FILE, _BOLD "~ %s waiting for route selection ~\n" _RESET, APP_NAME);

	// Ожидаем выбора через интерфейс
	this->iface.display(&LCD_Interface::route_selection_menu);

	// Ожидаем выбора через push-сообщение
	this->lc_task.enter_regular_mode();

	this->dev_state.set("Ожидание выбора маршрута");
}

void AVI::regular_mode()
{
	log_msg(MSG_DEBUG | MSG_TO_FILE, _BOLD "~ %s entering regular mode ~\n" _RESET, APP_NAME);

	this->iface.display(&LCD_Interface::main_menu);
	LCD_Interface::route_selection_menu.setup_timeout(LCD_Interface::menu_timeout, [this](){ iface.display(&LCD_Interface::main_menu); });

	this->lc_task.enter_regular_mode();

	// Инициализация задачи информационных объявлений
	if(this->settings.gps_poll_period_ms <= 0){
		log_warn("Announcement_task is disabled\n");
	} 
	else{

		if(this->settings.gps_poll_period_ms < 250){
			this->announ_task.set_period(std::chrono::milliseconds(this->settings.gps_poll_period_ms), 
				std::chrono::milliseconds(this->settings.gps_poll_period_ms / 2));
		}
		else{
			this->announ_task.set_period(std::chrono::milliseconds(this->settings.gps_poll_period_ms));
		}
		
		this->announ_task.init(&this->dirs.media_dir);
		this->announ_task.start(true);
	}

	this->dev_state.set("Штатный режим работы (маршрут: " + 
		std::to_string(NSIDatabase::get_current_route()) + ")");
}

void AVI::deinit()
{
	std::lock_guard<std::mutex> lock{this->init_phase_mutex};

	// this->startup_scheduler.stop();
	this->iface.stop();

	this->lc_task.stop();
	this->announ_task.stop();

	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	lc::ProtoTransactions::deinit();
	this->lc_task.cancel();
	this->announ_task.cancel();

	platform::deinit();
	this->mdb.deinit();
	this->lc_task.global_cleanup();
}

// void AVI::reinit()
// {
// 	log_msg(MSG_DEBUG | MSG_TO_FILE, _BOLD "~ %s reinit phase ~\n" _RESET, APP_NAME);

// 	this->lc_task.stop();
// 	this->announ_task.stop();

// 	this->lc_task.wait();
// 	this->announ_task.wait();

// 	// platform::deinit();
// 	// this->mdb.deinit();

// 	this->init();
// }


	// TMP 
	// 55.753633, lon: 37.759800  lat: 55.747267, lon: 37.761383
	// Entering zone id: 3209 (lat: 55.748717, lon: 37.756533, course: 327.50)
	//
	// lat: 55.752350, long: 37.759667
	// lat: 55.752350, long: 37.759683,
	// std::unique_ptr<NSIDatabase::kFrames_table::Zone> zone(
	// 	new NSIDatabase::kFrames_table::CircleZone{55.753633, 37.759800, 29, 1000});

	// zone->contains({55.752350, 37.759683}); 

	//

} // namespace avi

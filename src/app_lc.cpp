#include <cinttypes>
#include <algorithm>
#include <future>

#define LOG_MODULE_NAME		"[ ALC ]"
#include "logger.hpp"

#include "utils/fs.hpp"
#include "utils/crypto.hpp"
// #include "lc_protocol.hpp"
#include "app_db.hpp"
#include "app.hpp"
#include "app_lc.hpp"

namespace avi{

// Инициализация статических членов класса
// LC_client::settings LC_client_task::sets;
// LC_client::directories LC_client_task::dirs;
// LC_client LC_client_task::lcc{&LC_client_task::dirs, &LC_client_task::sets};

// Сопоставление типов файлов протокола приложения и протокола локального центра
const std::unordered_map<std::string, const std::string> LC_client_task::types_map = {
	{APP_FILE_NSI, "device_tgz"},
};
bool LC_client_task::global_inited{false};

// Информация об устройстве для периодической передачи на сервер ЛЦ 
AVI_Info::AVI_Info(const std::string &version, const std::string &state): 
	LC_base_device_info(pb::RAW) // TODO:: add type
{
	this->info.mutable_base()->set_version(version);
	this->info.mutable_base()->set_state(state);
}

std::string AVI_Info::serialize_to_proto() const
{
	std::string out;
	this->info.SerializeToString(&out);
	return out;
}

//

void LC_client_task::global_init() 
{ 
	LC_client::global_init(); 
	global_inited = true;
}

void LC_client_task::global_cleanup() 
{ 
	if(global_inited){
		LC_client::global_cleanup(); 
		global_inited = false;
	}
}

//
void LC_client_task::init()
{
	if( !this->app ){
		throw std::invalid_argument(excp_method("app was not set (nullptr)"));
	}

	// Выставляем общие разрешения (разблокировка запрещена, отправка транзакций разрешена всегда)
	sets.perms.unblock = false;
	sets.perms.put_data = true;

	sets.device_id = app->dev_id.get();
	sets.system_id = app->dev_id.get();

	// Заполнения локальных медиа листов
	const_media_list.fill(app->dirs.media_dir, false);
	tmp_media_list.fill(app->dirs.media_dir);
	
	log_msg(MSG_VERBOSE /*| MSG_TO_FILE*/, "const_media_list (ver. %s):\n%s\n", const_media_list.version, const_media_list.data_to_str(29));
	log_msg(MSG_VERBOSE /*| MSG_TO_FILE*/, "tmp_media_list (ver. %s):\n%s\n", tmp_media_list.version, tmp_media_list.data_to_str(29));

	// Назначаем колбеки на сохранение файлов и их локальные версии 
	this->setup_download_callbacks();
	sets.get["device_tgz"].curr_ver = NSIDatabase::get_version();

	std::lock_guard<std::recursive_mutex> lock(this->lcc_mutex);
	this->lcc.init();
	this->lcc.rotate_sent_data();
}

void LC_client_task::deinit()
{
	std::lock_guard<std::recursive_mutex> lock(this->lcc_mutex);
	this->lcc.deinit();
	this->on_download_finished = nullptr;
	this->mode = Mode::suspend;
}


void LC_client_task::save_as_b64(const std::string &type, const std::string &content, const std::string &ver)
{
	// FilesController::write(type, content, ver);
	
	std::string ev_name = std::string("LC_GOT_") + type;
	app->create_sys_event(ev_name, ver); // TODO

	// Обновляем текущую версиюю файла в структуре запроса
	sets.get[ types_map.at(type) ].curr_ver = ver;
}

std::string LC_client_task::save_as_bin(const std::string &type, const std::string &dest_dir, const uint8_t *content, size_t size, const std::string &ver)
{
	std::string path = dest_dir + "/" + type + "_" + ver + LC_FILE_EXT;
		
	utils::write_bin_file(path, content, size);

	bool no_fileinfo_is_error = (type == APP_FILE_MEDIA) ? false : true;
	std::string content_name = lc::utils::unpack_tar(path, dest_dir, no_fileinfo_is_error);

	remove(path.c_str());

	if( !content_name.empty() ){
		std::string dest_name = dest_dir + "/" + content_name;
		utils::change_mod(dest_name, 0666);
		log_msg(MSG_DEBUG | MSG_TO_FILE, "'%s' tar has been unpacked -> '%s'\n", type, dest_name);
	}
	else{
		log_msg(MSG_DEBUG | MSG_TO_FILE, "'%s' tar has been unpacked to '%s/'\n", type, dest_dir);
	}

	// Обновляем текущую версиюю файла в структуре запроса и создаем системное событие
	sets.get[ types_map.at(type) ].curr_ver = ver;
	std::string ev_name = std::string("LC_GOT_") + type;
	app->create_sys_event(ev_name, ver);

	return dest_dir + "/" + content_name;
}

void LC_client_task::save_media(const std::string &dest_dir, const std::string &name, const uint8_t *content, size_t size)
{
	std::string file_path = dest_dir + "/" + name;

	utils::write_bin_file(file_path, content, size);
	utils::change_mod(file_path, 0666);

	log_info("Media '%s' has been saved\n", file_path);
}

// Проверяет есть ли элемент elem с заданным именем и md5 хешем в указанном списке
static std::pair<bool, std::string> is_in_list(const lc_media_list &list, const lc::list_elem &elem)
{
	std::pair<bool, std::string> res{false, ""};

	// Листы отсортированы в алфавитном (лексикографическом порядке) по имени файлов
	auto comp = [](const lc::list_elem &lhs, const lc::list_elem &rhs) -> bool{
		return lhs.name < rhs.name;
	};

	// Returns an iterator pointing to the first element in the range [begin, end) 
	// such that comp(element, value)) is false (i.e. element greater or equal to value)
	auto it = std::lower_bound(list.cbegin(), list.cend(), elem, comp);

	bool found = (it != list.cend()) && (it->name == elem.name);

	if(found){
		res = {true, it->md5};
	}

	return res;

	// return std::binary_search(list.cbegin(), list.cend(), elem, comp);
}

// Возвращает массив имен+версий файлов отсутствующих в local листе 
// для последующих запросов скачивания
std::vector<std::string> LC_client_task::compare_media_lists(const lc_media_list &local, const lc_media_list &remote) const
{
	std::vector<std::string> res;

	// Определение какие именно файлы не совпадают
	for(const auto &elem : remote){

		std::pair<bool, std::string> ret = is_in_list(local, elem);

		// Если такого имени файла нет в локальном листе или его хеш не совпадает
		if( !ret.first || (ret.second != elem.md5) ){
			res.push_back(elem.name + ";" + elem.md5);
		}
	}

	return res;
}

void LC_client_task::clear_unused_media(const lc_media_list &local, const lc_media_list &remote) const
{
	// Проверка нужно ли что-нибудь удалить с диска
	for(const auto &elem : local){
		std::pair<bool, std::string> ret = is_in_list(local, elem);

		if( !ret.first ){
			std::string path = app->dirs.media_dir + "/" + elem.name;
			log_msg(MSG_DEBUG | MSG_TO_FILE, "Removing media not in remote list '%s'\n", path);
			remove(path.c_str());
		}
	}
}

void LC_client_task::Media_list::fill(const std::string &media_dir, bool tmp_media)
{
	const std::string files_extension = tmp_media ? ".tmp.mp3" : ".const.mp3";
	std::vector<std::string> files = utils::get_file_names_in_dir(media_dir, files_extension);

	// В качестве version используется md5 от строки из конкатенации md5(file[i]).
	// Файлы должны быть отсортированы в алфавитном порядке.
	std::string md5_str;
	std::sort(files.begin(), files.end());

	this->data.clear();
	this->version = "undefined";

	for(const auto &file : files){
		std::string md5 = utils::file_md5(media_dir + "/" + file);
		this->data.emplace_back(file, md5);
		md5_str += md5;
	}

	if( !md5_str.empty() ){
		this->version = utils::md5sum(md5_str.c_str(), md5_str.length());
	}
}

std::string LC_client_task::Media_list::data_to_str(int pad) const
{
	std::string out;

	for(const auto &elem : data){
		out += std::string(pad, ' ') + elem.name + " : " + elem.md5 + "\n";
	}

	if( !out.empty() ){
		out.pop_back();
	}

	return out;
}

void LC_client_task::get_media(bool list_is_tmp, const lc_media_list &remote_list)
{
	const Media_list * const local_list = list_is_tmp ? &this->tmp_media_list : &this->const_media_list;
	std::exception error;
	bool error_occured = false;
	uint files_cnt = 0;

	bool was_enabled = this->sets.get.at("media").enabled;
	this->sets.get.at("media").enabled = true;

	try{
		std::vector<std::string> versions = this->compare_media_lists(local_list->data, remote_list);

		for(const auto &ver : versions){
			this->sets.get.at("media").curr_ver = ver;
			this->lcc.get_file("media", this->sets.get.at("media"));
			++files_cnt;
		}

		if(versions.empty()){
			log_warn("%s_media_lists are equal, no update needed\n", list_is_tmp ? "tmp" : "const");
		}	
		else{
			log_info("Successfully downloaded %u %s_media file(s)\n", files_cnt, list_is_tmp ? "tmp" : "const");
		}
		
	}
	catch(const std::exception &e){
		// Во время скачивания может пропасть связь или возникнуть
		// критическая ошибка. Сохраняем объект исключения.
		error = e;
		error_occured = true;
	}

	// Возвращаем разрешение на запрос
	this->sets.get.at("media").enabled = was_enabled;

	// Если что-то было скачано
	if(files_cnt){
		// Обновляем соответствующий локальный медиа-лист 
		if(list_is_tmp){
			tmp_media_list.fill(app->dirs.media_dir);
		}
		else{
			const_media_list.fill(app->dirs.media_dir, false);
		}
		
		// Очищаем устаревший медиа контент
		this->clear_unused_media(local_list->data, remote_list);
	}

	if(error_occured){
		throw error;
	}
}


void LC_client_task::setup_download_callbacks()
{
	using namespace std;

	// Убираем заведенные по-умолчанию неиспользуемые типы файлов из запросов
	this->sets.get.clear();

	// НСИ
	this->sets.get["device_tgz"].dec_save = [this](const string &name, const uint8_t *content, size_t size, const string &version){ 
		std::string db_path = this->save_as_bin(APP_FILE_NSI, app->dirs.data_dir, content, size, version); 
		
		if(this->on_nsi_update){
			this->on_nsi_update(db_path, version);
		}
	};

	// Медиа листы
	this->lcc.on_media_list_get = [this](lc_media_list &list, bool list_is_tmp){
		if(this->on_media_list_update){
			this->on_media_list_update("", "");
		}

		this->get_media(list_is_tmp, list);
	};

	// Медиа контент
	this->sets.get["media"].dec_save = [this](const string &name, const uint8_t *content, size_t size, const string &version){
		this->save_media(app->dirs.media_dir, name, content, size);
	};

	// Пока не получен медиа-лист, ничего не запрашиваем
	this->sets.get["media"].enabled = false;  
}

void LC_client_task::enter_suspend_mode()
{
	std::lock_guard<std::recursive_mutex> lock(this->lcc_mutex);
	this->mode = Mode::suspend;
}

// Режим приема только PUSH сообщений
void LC_client_task::enter_regular_mode()
{
	std::lock_guard<std::recursive_mutex> lock(this->lcc_mutex);
	this->mode = Mode::regular;

	// Отключаем все разрешения на скачивания.
	// В процессе работа клиента, может придти push на скачивание
	// какого-то определенного набора данных
	for(auto &kv : this->sets.get){
		kv.second.enabled = false;
	}
	this->sets.perms.get_media_list = false;
}

// Режим отправки запросов на скачивание файлов приложения
void LC_client_task::enter_download_mode()
{
	std::lock_guard<std::recursive_mutex> lock(this->lcc_mutex);
	this->mode = Mode::download;

	// Разрешаем скачивать все необходимые данные
	this->sets.perms.get_media_list = true;
	this->sets.get["device_tgz"].enabled = true;

	this->reset_download_period_cnt();
}

void LC_client_task::reset_download_period_cnt()
{
	this->download_period_cnt = 1;
}

bool LC_client_task::download_period_elapsed()
{
	--this->download_period_cnt;

	if(this->download_period_cnt <= 0){
		const int sec_to_cnt = app->settings.lc_download_period / Background_task::get_period_sec().count();
		this->download_period_cnt = sec_to_cnt;
		return true;
	}

	return false;
}

// Скачивание данных приложения
void LC_client_task::download()
{
	this->lcc.get_files();
	this->lcc.get_media_lists(tmp_media_list.version, const_media_list.version);
}

Background_task::signal LC_client_task::main_func(void)
{
	// Проверка состояния задачи
	if(this->get_current_state() == signal::STOP){
		return signal::STOP;
	}

	// Актуализация идентификаторов системы
	sets.device_id = app->dev_id.get();
	sets.system_id = app->dev_id.get();

	session_finished_cb cb = nullptr;

	std::unique_lock<std::recursive_mutex> lock(this->lcc_mutex);
	if(this->mode == Mode::suspend){
		return signal::SLEEP;
	}

	this->lcc.show_start();
	
	try{
		
		// В любом режиме отправляется статус устройства с возможным получением PUSH сообщения
		// TODO:
		// this->lcc.send_info();

		if(this->mode == Mode::regular){

			// Ответный PUSH может вызвать обновление каких-либо файлов приложения.
			if(this->download_pushed){
				cb = this->on_download_finished;
				this->download();
				// выставить флаг, что пуш обработан
			}
		}

		if(this->mode == Mode::download){

			app->iface.display(&LCD_Interface::download_phase_menu);

			if(this->download_period_elapsed()){
				cb = this->on_download_finished;
				this->download();

				if(this->download_pushed){
				// выставить флаг, что пуш обработан
				}
			}
		}

		// В любом режиме отправляются транзакции системных событий 
		// if(this->upload_period_elapsed()){
		// 	this->lcc.put_data();
		// } 
	}
	catch(const LC_no_connection &e){
		// Если соединения было, но пропало - генерируем системное событие
		if(this->connection) app->create_sys_event("LC_OFFLINE");
	}
	catch(const LC_error &e){
		app->create_sys_event("LC_EXCH_ERR", e.what());
	}
	catch(const std::exception &e){
		log_err("%s\n", e.what());
	}

	this->connection = this->lcc.show_results();

	// Разлочим клиент перед вызовов колбека
	lock.unlock();	
	if(cb){
		// auto fut = std::async(std::launch::async, cb);
		cb();
	}

	return signal::SLEEP;
}

// bool LC_client_task::download()
// {
// 	bool ret = false;
// 	std::lock_guard<std::recursive_mutex> lock(this->lcc_mutex);

// 	try{
// 		this->lcc.show_start();
// 		this->lcc.get_files();
// 	}
// 	catch(const LC_no_connection &e){
// 		// Если соединения было, но пропало - генерируем системное событие
// 		if(this->connection){
// 			app->create_sys_event("LC_OFFLINE");
// 		} 
// 	}
// 	catch(const LC_error &e){
// 		app->create_sys_event("LC_EXCH_ERR", e.what());
// 	}
// 	catch(const std::exception &e){
// 		log_err("%s\n", e.what());
// 	}

// 	this->connection = this->lcc.show_results();
// 	return ret;
// }

} // namespace

#ifdef _APP_LC_TEST

#include <thread>
#include <chrono>

using namespace std;

int main(int argc, char *argv[])
{
	logger.init(MSG_TRACE, "", 1, 1000);

	app->LC_client_task lc_task;
	lc_task.period_sec = 20;
	lc_task.sets.psu = 3487366287;
	lc_task.init();
	lc_task.start();

	log_msg(MSG_DEBUG, "[M] going to sleep\n");
	this_thread::sleep_for(chrono::seconds(30));

	log_msg(MSG_DEBUG, "[M] lc_task sending stop\n");
	lc_task.stop();
	lc_task.wait();
	log_msg(MSG_DEBUG, "[M] lc_task stopped\n");

	return 0;
}

#endif

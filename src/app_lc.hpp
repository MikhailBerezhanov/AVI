/*============================================================================== 
Описание: 	Модуль фонового процесса запуска клиента Локального Центра.

Автор: 		berezhanov.m@gmail.com
Дата:		22.04.2022
Версия: 	1.0
==============================================================================*/

#pragma once

#include <functional>
#include <unordered_map>
#include <mutex>

#include "lc_client.hpp"
#include "bg_task.hpp"

namespace avi{

class AVI;

// Информация об устройстве для периодической отправки на сервер
class AVI_Status final : public LC_device_status
{
public:
	AVI_Status(const std::string &version = "undefined", const std::string &state = "undefined"); 

	std::string serialize_to_proto() const override;

	void set_state(const std::string &new_state)
	{
		std::lock_guard<std::mutex> lck(this->mtx);
		this->status.mutable_base()->set_state(new_state);
	}

	void set_prev_push_info(const std::string &uid, const std::string &date)
	{
		std::lock_guard<std::mutex> lck(this->mtx);
		this->status.mutable_base()->set_prev_push_uid(uid);
		this->status.mutable_base()->set_prev_push_date(date);
	}

	void set_sd_free_space(uint32_t megabytes)
	{
		std::lock_guard<std::mutex> lck(this->mtx);
		this->status.set_sd_free_space(megabytes);
	}

	void set_nsi(const std::string &version, const std::string &date = "")
	{
		std::lock_guard<std::mutex> lck(this->mtx);
		this->status.set_nsi_version(version);
		if( !date.empty() ){
			this->status.set_nsi_update_date(date);
		}
	}

	void set_latitude_longitude(const std::string &lat, const std::string &lon)
	{
		std::lock_guard<std::mutex> lck(this->mtx);
		this->status.set_latitude(lat);
		this->status.set_longitude(lon);
	}

private:
	mutable std::mutex mtx;
	pb::AviStatus status;
};


// 
class LC_client_task final : public Background_task
{
	using session_finished_cb = std::function<void()>;
	using update_cb = std::function<void(const std::string &path, const std::string &ver)>;

public:
	LC_client_task(const AVI *application = nullptr, const std::string &task_name = "LCC_task"): 
		Background_task(task_name), app(application) {}

	static void global_init();
	static void global_cleanup();

	void init();

	void wait() override { 
		Background_task::wait();
		this->deinit();
	}

	void cancel() override{
		Background_task::cancel();
		this->deinit();
	}

	enum class Mode{
		suspend = 0,
		download= 1,
		regular,
	};

	void enter_suspend_mode();
	void enter_regular_mode();
	void enter_download_mode();

	LC_client::settings sets;
	LC_client::directories dirs;

	// Колбеки при получении обновлений файлов приложения
	update_cb on_nsi_update = nullptr;
	update_cb on_media_list_update = nullptr;
	update_cb on_route_selected = nullptr;	// TODO
	update_cb on_software_update = nullptr;
	session_finished_cb on_download_finished = nullptr;

	AVI_Status avi_status{APP_VERSION};

private:
	const AVI *const app = nullptr; 	// Указатель на общие данные приложения
	Mode mode = Mode::suspend;

	mutable std::recursive_mutex lcc_mutex;
	LC_client lcc{&this->dirs, &this->sets};
	static const std::unordered_map<std::string, const std::string> types_map;
	static bool global_inited;
	bool connection = true;		// Считаем что изначально соединение есть 

	void deinit();

	// Обновление файлов может быть вызвона принудительно PUSH сообщением
	bool download_pushed = false;
	void download();
	
	void setup_download_callbacks();
	int download_period_cnt = 1;
	void reset_download_period_cnt();
	bool download_period_elapsed();

	// Отправка статуса устройства
	void send_device_status();

	Background_task::signal main_func(void) override;

	void save_as_b64(const std::string &type, const std::string &content, const std::string &ver);
	std::string save_as_bin(const std::string &type, const std::string &dest_dir, const uint8_t *content, size_t size, const std::string &ver);
	
	struct Media_list
	{
		std::string version = "undefined";
		lc_media_list data;

		void fill(const std::string &media_dir, bool tmp_media = true);
		std::string data_to_str(int pad = 0) const;
	};

	Media_list tmp_media_list;
	Media_list const_media_list;

	std::vector<std::string> compare_media_lists(const lc_media_list &local, const lc_media_list &remote) const;
	void clear_unused_media(const lc_media_list &local, const lc_media_list &remote) const;
	void get_media(bool tmp_media, const lc_media_list &list);
	void save_media(const std::string &dest_dir, const std::string &name, const uint8_t *content, size_t size);
};

} // namespace avi


/*============================================================================== 
Описание: 	Модуль фонового процесса обновления навигационных данных 
			аудио информирования.

			Задача получает текущие координаты местоположения устройства,
			обновляет внутреннюю структуру и следит за порядком воспроизведения 
			медиа-файлов.

Автор: 		berezhanov.m@gmail.com
Дата:		16.05.2022
Версия: 	1.0
==============================================================================*/

#pragma once

#include <string>
#include <mutex>
#include <tuple>
#include <utility>
#include <queue>

#include "bg_task.hpp"
#include "platform.hpp"
#include "app_db.hpp"
#include "logger.hpp"

namespace avi{

class AVI;

// Класс получения GPS данных и анализа текущих координат на
// попадание в загруженные из БД НСИ фреймы
class Navigator
{
public:

	void init(const std::string &log_file_name = "");

	std::tuple<bool, double, double> get_position() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return std::make_tuple(data_.valid, data_.lat_lon.first, data_.lat_lon.second);
	}

	platform::GPS_data get_gps_data() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return data_;
	}

	platform::GPS_data update_gps_data(){
		std::lock_guard<std::mutex> lock(mutex_);
		data_ = platform::get_GPS_data();
		return data_;
	}

	bool position_is_valid() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return data_.valid;
	}

	void log_position(const platform::GPS_data &data) const;

private:
	mutable std::mutex mutex_;
	platform::GPS_data data_; 
	Logging track_logger_{MSG_TO_FILE, ""};		// Лог текущих координат
};

// Проигрыватель медиа-контента
class MediaPlayer
{
	using info = const NSIDatabase::kFrames_table::MediaInfo*;

public:

	void init(const std::string *media_dir);
	void deinit();

	// Режимы проигрывания
	enum class mode: uint8_t{
		// Воспроизведение FRM1 без прерывания (в случае если появилась необходимость 
		// воспроизведения другого фрейма FRM2, воспроизведение текущего фрейма НЕ 
		// ОСТАНАВЛИВАЕТСЯ, FRM2 не воспроизводится)
		UNINTERRUPTED = 0,

		// Воспроизведение FRM1 без прерывания с последующим воспроизведением FRM2 (в 
		// случае если определена необходимость воспроизведения другого фрейма FRM2,
		// воспроизведение текущего фрейма НЕ ОСТАНАВЛИВАЕТСЯ, FRM2 воспроизводится при 
		// завершении воспроизведения FRM1)
		QUEUED = 1,

		// Воспроизведение FRM1 с прерыванием и воспроизведением FRM2 (в случае если 
		// появилась необходимость воспроизведения другого фрейма FRM2, воспроизведение 
		// фрейма FRM1 ОСТАНАВЛИВАЕТСЯ, воспроизводится фрейм FRM2)
		INTERRUPTED = 2,

		// Принудительно воспроизвести следующий фрейм FRM_CHLD по окончании 
		// воспроизведения текущего FRM1 с возможностью прерывания при необходимости 
		// воспроизведения другого FRM2
		INTERRUPTED_PARENT = 3,

		// Принудительно воспроизвести следующий фрейм FRM_CHLD по окончании 
		// воспроизведения текущего FRM1  без возможности прерывания при необходимости 
		// воспроизведения другого FRM2
		UNINTERRUPTED_PARENT = 4,
	};

	static const char* mode_as_str(uint8_t mode);

	void play(info media);

	void after_play_finished();

private:
	mutable std::recursive_mutex mutex_;
	const std::string *media_dir_ = nullptr;

	// Данные о текущем воспроизведении
	info playing_media_ = nullptr;

	// Медиа-данные обновляются только при старте приложения. Во время выполнения задачи
	// считаем, что таблица медиа-данных уже загружена в память и не изменяется - храним 
	// указатели на активные меда-фрагменты
	std::queue<info> media_queue_;

	// Проверить потомка: если есть и задан режим - добавить в очередь
	void enqueue_child_media(info parent_media);
	void clear_media_queue();
	void start_playing(info media, bool after_stop = false);
};


//
class Announcement_task final : public Background_task
{
public:
	Announcement_task(const AVI *app = nullptr, const std::string &task_name = "Announcement_task"): 
		Background_task(task_name), app_(app) {}

	void init(const std::string *media_dir);

	void wait() override { 
		mplayer_.deinit();
		Background_task::wait();
	}

	void cancel() override{
		mplayer_.deinit();
		Background_task::cancel();
	}

	std::tuple<bool, double, double> get_position() const { return navi_.get_position(); }

private:
	const AVI *const app_ = nullptr;
	Navigator navi_;
	MediaPlayer mplayer_;

	int gps_validity_counter_ = 0;

	Background_task::signal main_func(void) override;
	bool gps_data_is_valid(const platform::GPS_data &data) noexcept;

	// Обновить отображение текущего фрейма
	void update_interface(int frame_id) const;
};

} // namespace avi

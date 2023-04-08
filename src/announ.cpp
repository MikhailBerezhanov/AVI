#include <limits>
#include <sstream>

#define LOG_MODULE_NAME		"[ ANN ]"
#include "logger.hpp"

#include "app.hpp"
#include "announ.hpp"

namespace avi{

void Navigator::init(const std::string &log_file_name)
{
	int route = NSIDatabase::get_current_route();

	// перед расширением добавляем идентификатор маршрута
	std::stringstream ss{log_file_name};
	std::string name;
	std::string extension;
	std::string path{log_file_name};

	if(getline(ss, name, '.') && getline(ss, extension)){
		path = name + std::to_string(route) + "." + extension;
	}

	track_logger_.init(MSG_TO_FILE, path, 0, MB_to_B(50));
}

void Navigator::log_position(const platform::GPS_data &data) const
{	
	track_logger_.msg(MSG_TO_FILE, "%s\n", data.to_string());
}



void MediaPlayer::enqueue_child_media(info parent_media)
{
	// Проверить потомка: если есть и задан подходящий режим - добавить в очередь
	if( (parent_media->id_next > -1) && 
		((static_cast<mode>(parent_media->play_mode) == mode::INTERRUPTED_PARENT) || 
		(static_cast<mode>(parent_media->play_mode) == mode::UNINTERRUPTED_PARENT)) ){

		info child_media = NSIDatabase::get_media_info_of_child(parent_media->id_next);
		if(child_media){
			media_queue_.push(child_media);
			log_msg(MSG_DEBUG, "Enqueuing child media (id: %d): '%s', mode: %s\n", parent_media->id_next, child_media->filename, mode_as_str(child_media->play_mode));
		}
	}
}

void MediaPlayer::init(const std::string *media_dir)
{
	media_dir_ = media_dir;
	platform::audio_setup_stop_callback([this](){ this->after_play_finished(); });
}

void MediaPlayer::deinit()
{
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		this->clear_media_queue();
		playing_media_ = nullptr;
	}

	platform::audio_setup_stop_callback(nullptr);
	platform::audio_stop();	
}

void MediaPlayer::clear_media_queue()
{
	std::queue<info> empty;
	std::swap(media_queue_, empty);
}

const char* MediaPlayer::mode_as_str(uint8_t play_mode)
{
	switch(static_cast<mode>(play_mode)){
		case mode::UNINTERRUPTED: return "UNINTERRUPTED";
		case mode::UNINTERRUPTED_PARENT: return "UNINTERRUPTED_PARENT";
		case mode::QUEUED: return "QUEUED";
		case mode::INTERRUPTED: return "INTERRUPTED";
		case mode::INTERRUPTED_PARENT: return "INTERRUPTED_PARENT";
		default: return "UNSUPPORTED";
	}
}

void MediaPlayer::start_playing(info media, bool after_stop)
{
	// Добавляем медиа-данные потомка если есть
	this->enqueue_child_media(media);

	// Воспроизвести аудио-файл
	auto play = [this, media, after_stop](){

		try{
			std::string filepath = *media_dir_ + "/" + media->filename;

			// Если задано делаем паузу
			if(media->pause){
				log_msg(MSG_VERBOSE, "Waiting %d sec pause before playing\n", media->pause);
				std::this_thread::sleep_for(std::chrono::seconds(media->pause));
			}

			// Ждем окончательной готовности к новому проигрыванию
			while(platform::audio_is_playing()){
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			log_msg(MSG_DEBUG | MSG_TO_FILE, "Playing audio %s '%s' (mode: %s)\n", 
				after_stop ? "from media_queue" : "", filepath, mode_as_str(media->play_mode));
			platform::audio_play(filepath);

			// Обновить данные о текущем воспроизведении
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			playing_media_ = media;
		}
		catch(const std::exception &e){
			log_excp("%s\n", e.what());
		}
	};

	// Колбек after_play_finished() должен отработать прежде чем произойдет очередной
	// вызов platform::audio_play() т.к. SIMCOM API не позволяет вызывать audio_play() из 
	// обработчика audio_stop(). Поэтому отсоединяем поток запуска аудио-файла.
	std::thread tplay(play);
	tplay.detach();
}

void MediaPlayer::after_play_finished()
{
	try{
		log_msg(MSG_DEBUG | MSG_TO_FILE, "Audio stopped\n");

		std::lock_guard<std::recursive_mutex> lock(mutex_);
		playing_media_ = nullptr;

		if(media_queue_.empty()){
			// Больше нет активных воспроизведений
			log_msg(MSG_DEBUG, "Media queue is empty\n");
			return;
		}

		// Достаем медиа-данные из начала очереди (порядок FIFO)
		info media = media_queue_.front();
		media_queue_.pop();

		this->start_playing(media, true);
	}
	catch(const std::exception &e){
		log_excp("%s\n", e.what());
	}
}

void MediaPlayer::play(info media)
{
	if( !media || !media_dir_ ){
		return;
	}

	try{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		// Проверка воспроизводится ли в текущий момент что-то еще
		if(platform::audio_is_playing() && playing_media_){

			switch(static_cast<mode>(playing_media_->play_mode)){
				case mode::UNINTERRUPTED:
				case mode::UNINTERRUPTED_PARENT:
					// Прервать текущее воспроиздевение нельзя - игнорируем пришедшие медиа-данные
					log_warn("Uninterrupted media is playing now. Ignoring '%s'\n", media->filename);
					return;

				case mode::QUEUED:
					// Добавляем в очередь на воспроизведение
					media_queue_.push(media);
					log_msg(MSG_DEBUG | MSG_TO_FILE, "Queued media is playing. Enqueuing '%s'\n", media->filename);
					return;

				case mode::INTERRUPTED:
				case mode::INTERRUPTED_PARENT:
					log_msg(MSG_DEBUG | MSG_TO_FILE, "Interrupted media is playing. Stopping\n");
					
					// Очищаем текущую очередь
					this->clear_media_queue();

					// Добавляем текущую медиа для проигрывания после отработки прерывания
					media_queue_.push(media);

					// Прерываем текущее воспроизведение
					platform::audio_stop();	// this->after_play_finished() will be called 
					return;

				default:
					log_warn("Unsupported play_mode: %d. Ignoring\n", playing_media_->play_mode);
					return;
			}
		}
		else{
			this->start_playing(media);
		}
	}
	catch(const std::exception &e){
		log_excp("%s\n", e.what());
	}
}



void Announcement_task::init(const std::string *media_dir)
{
	if( !app_ ){
		throw std::invalid_argument(excp_method("app was not set (nullptr)"));
	}

	if( !platform::ready() ){
		throw std::runtime_error(excp_method("platform not ready"));
	}

	navi_.init(app_->dirs.gps_track_path);
	mplayer_.init(&(app_->dirs.media_dir));
}

bool Announcement_task::gps_data_ready_for_processing(const platform::GPS_data &data) noexcept
{
	if( !data.valid || (data.speed_kmh < app_->settings.gps_min_valid_speed) ){
		gps_validity_counter_ = 0;
		return false;
	}

	++gps_validity_counter_;

	if(gps_validity_counter_ >= app_->settings.gps_valid_threshold){
		gps_validity_counter_ = 0;
		return true;
	}

	return false;
}

void Announcement_task::update_interface(int frame_id) const
{
	std::string value = (frame_id > -1) ? std::to_string(frame_id) : "XXXX";

	// LCD_Interface::main_menu.update_content(1, "ФРЕЙМ: " + value);
	LCD_Interface::main_menu.update_ticker_content(1, value);

	// Перерисовываем главное меню только если оно уже на экране
	if(app_->iface.curr_menu() == &LCD_Interface::main_menu){
		app_->iface.display(&LCD_Interface::main_menu);
	}
}

Background_task::signal Announcement_task::main_func(void)
{
	// Признак валидности предыдущих GPS координат
	static bool was_valid = false;

	// Проверка состояния задачи
	if(this->get_current_state() == Background_task::signal::STOP){
		return Background_task::signal::STOP;
	}

	// Задача получает текущие координаты местоположения,
	// обновляет внутреннюю структуру и следит за порядком
	// воспроизведения медиа-файлов				

	try{

		platform::GPS_data gps_data = navi_.update_gps_data();

		if( !gps_data.valid && was_valid ){
			// Логируем пропадание валидных координат один раз
			navi_.log_position(gps_data);
			was_valid = false;
		}

		// Проверка готовности данных к обработке 
		if( !gps_data_ready_for_processing(gps_data) ){
			return Background_task::signal::SLEEP;
		}

		was_valid = true;
		int frame_id = std::numeric_limits<int>::min();
		const NSIDatabase::kFrames_table::MediaInfo *minfo = NSIDatabase::find_media_info(gps_data.lat_lon, gps_data.course, &frame_id);

		if(frame_id != std::numeric_limits<int>::min()){
			this->update_interface(frame_id);
		}

		if(minfo){
			mplayer_.play(minfo);
		}

		navi_.log_position(gps_data);
	}
	catch(const std::exception &e){
		log_err("%s\n", e.what());
	}

	return Background_task::signal::SLEEP;
}

} // namespace
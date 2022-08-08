#include <stdexcept>
#include <thread>
#include <chrono>

extern "C" {
#include <simcom_common.h>
#include <ALSAControl.h>
}

#include "audio.hpp"

namespace hw{

// static members init
// std::atomic<bool> Audio::stopped{false};
// std::recursive_mutex Audio::mutex_;
std::function<void(void)> Audio::finished_callback = nullptr;

void Audio::stop()
{
	// std::lock_guard<std::recursive_mutex> lock(mutex_);
	audio_play_stop();
}

// Play audio MP3 file
void Audio::play(char *file_name, int repeats)
{
	if(is_playing()){
		return;
	}

	// std::lock_guard<std::recursive_mutex> lock(mutex_);
	int ret = audio_play_start(file_name, repeats, 0);

	if(ret < 0){
		throw std::runtime_error(std::string("audio_play_start(") + file_name + ") failed");
	}   

	// while( stopped.load() != true ){
	// 	// usleep(500);
	// 	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	// }

	// stopped.store(false);
	// Audio::stop();
}

// Play audio MP3 file from FS
void Audio::play(const std::string &file_path, int repeats)
{
	if(is_playing()){
		return;
	}

	std::string dest_name = "/data/media/tmp.mp3";
	std::string cmd = "cp " + file_path + " " + dest_name;

	if( int sys_res = system(cmd.c_str()) ){
		throw std::runtime_error(std::string("Audio::play failed - ") + cmd + " error (" + std::to_string(WEXITSTATUS(sys_res)) + ")");
	}

	char internal_name[32];
	strcpy(internal_name, "e:/tmp.mp3");

	// std::lock_guard<std::recursive_mutex> lock(mutex_);
	int ret = audio_play_start(internal_name, repeats, 0);

	if(ret < 0) {
		throw std::runtime_error(std::string("audio_play_start(" + file_path + ") failed"));
	} 

	// remove(dest_name.c_str());
	// while( stopped.load() != true ){
	// 	// printf("Audio state: %d\n",  Audio::is_playing());
	// 	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	// }

	// stopped.store(false);
	// Audio::stop();
}


bool Audio::is_playing()
{
	int state = audio_get_state();

	if(state < 0){
		throw std::runtime_error("audio_get_state() failed");
	}

	return static_cast<bool>(state);
}

int Audio::get_mic_gain_level()
{
	int res = get_micgain_value();
	if(res < 0){
		throw std::runtime_error("get_micgain_value() failed");
	}

	return res;
}

// level limits: 0 .. 8
void Audio::set_mic_gain_level(int value)
{
	if(value > 8){
		value = 8;
	}

	if(set_micgain_value(value) < 0){
		throw std::runtime_error(std::string("set_micgain_value(") + std::to_string(value) + ") failed");
	}
}

int Audio::get_audio_gain_level()
{
	int res = get_clvl_value();
	if(res < 0){
		throw std::runtime_error("get_clvl_value() failed");
	}

	return res;
}
	
// level limits: 0 .. 5
void Audio::set_audio_gain_level(int value)
{
	if(value > 5){
		value = 5;
	}

	if(set_clvl_value(value) < 0){
		throw std::runtime_error(std::string("set_clvl_value(") + std::to_string(value) + ") failed");
	}
}
	
} // namespace hardware


// Events handler (audio finished)
void process_simcom_ind_message(simcom_event_e event, void *cb_usr_data)
{
	switch(event){
		case SIMCOM_EVENT_AUDIO:
			// printf("Audio state: %d\n",  Audio::is_playing());
			// Audio::stopped.store(true);
			if(hw::Audio::finished_callback){
				hw::Audio::finished_callback();
			}

			break; 
	}
}



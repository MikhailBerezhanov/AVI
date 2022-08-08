#pragma once

#include <string>
#include <functional>
// #include <atomic>
// #include <mutex>

namespace hw{

class Audio
{
public:
	Audio() = default;
	~Audio()
	{ 
		if(this->is_playing()){
			this->stop();
		}
	}

	int get_mic_gain_level();

	// level limits: 0 .. 8
	void set_mic_gain_level(int value);

	int get_audio_gain_level();

	// level limits: 0 .. 5
	void set_audio_gain_level(int value);

	// Play audio MP3 file from virtual filesystem (i.e 'E:/file_path.mp3')
	void play(char *file_name, int repeats = 0);

	// Play audio MP3 file from real filesystem (i.e '/sdcard/file_path.mp3')
	void play(const std::string &file_path, int repeats = 0);

	void stop();

	bool is_playing();

	// 
	// static std::atomic<bool> stopped;
	// Audio finished playing callback function
	static std::function<void(void)> finished_callback;

private:
	// static std::recursive_mutex mutex_;
};

} // namespace hardware

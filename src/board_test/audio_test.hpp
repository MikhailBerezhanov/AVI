#pragma once

#include "tests.hpp"

class AudioTest: public BaseTest
{
public:
	AudioTest(const std::string &type = "AudioTest"): BaseTest(type){
		table_.insert({"settings", {get_settings, "- get current ALSA settings"}});
		table_.insert({"micgain", {set_mic_level, "<0..8> - set mic_in gain level"}});
		table_.insert({"gain", {set_audio_level, "<0..5> - set audio_out gain level"}});
		table_.insert({"play", {play, "<fname> [rpts] - play .mp3 file repeats times from FS (/sdcard)"}});
		table_.insert({"play_e", {play_e, "<fname> [rpts] - play (E:/file.mp3) repeats times"}});
		table_.insert({"stop", {stop, "- stop active file playing"}});
		table_.insert({"life", {life, "[fname] - enable life playing using buttons 1, 3"}});
	}

	void init() const override;

private:
	static void get_settings(const std::vector<std::string> &params);
	static void set_mic_level(const std::vector<std::string> &params);
	static void set_audio_level(const std::vector<std::string> &params);
	static void play(const std::vector<std::string> &params);
	static void play_e(const std::vector<std::string> &params);
	static void stop(const std::vector<std::string> &params);
	static void life(const std::vector<std::string> &params);
	static void noise(const std::vector<std::string> &params);
};
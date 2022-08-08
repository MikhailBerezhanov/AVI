#include "logger.hpp"
#include "utils/fs.hpp"
#include "drivers/hardware.hpp"
#include "audio_test.hpp"

// ----- AUDIO TESTS -----
using namespace hw;

void AudioTest::init() const
{
	Hardware::enable_audio();

	// Destructor has this call already
	// interrupt_hook = [](){ Hardware::audio->stop(); };
}

void AudioTest::play(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::runtime_error(excp_method("no file_path provided"));
	}

	int repeats = (params.size() > 1) ? std::stoi(params[1]) : 0;

	Hardware::audio->play(params[0], repeats);

	while(Hardware::audio->is_playing()){
		wait(1);
	}
	
}

void AudioTest::play_e(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::runtime_error(excp_method("no file_name provided"));
	}

	const char* file_name = params[0].c_str();
	int repeats = (params.size() > 1) ? std::stoi(params[1]) : 0;

	Hardware::audio->play(const_cast<char*>(file_name), repeats);
	
	while(Hardware::audio->is_playing()){
		wait(1);
	}
}

void AudioTest::get_settings(const std::vector<std::string> &params)
{
	int mic_level = Hardware::audio->get_mic_gain_level();
	int audio_level = Hardware::audio->get_audio_gain_level();

	logger.msg(MSG_DEBUG, "audio gain level (0..5): %d\n", audio_level);
	logger.msg(MSG_DEBUG, "mic gain level (0..8): %d\n", mic_level);
}

void AudioTest::set_audio_level(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::runtime_error(excp_method("no level value provided"));
	}

	int value = std::stoi(params[0]);
	Hardware::audio->set_audio_gain_level(value);
}

void AudioTest::set_mic_level(const std::vector<std::string> &params)
{
	if(params.empty()){
		throw std::runtime_error(excp_method("no level value provided"));
	}

	int value = std::stoi(params[0]);
	Hardware::audio->set_mic_gain_level(value);
}


void AudioTest::stop(const std::vector<std::string> &params)
{
	Hardware::audio->stop();
}


void AudioTest::life(const std::vector<std::string> &params)
{
	std::string file_name = params.empty() ? "/sdcard/test2.mp3" : params[0];

	Hardware::enable_buttons();

	logger.msg(MSG_DEBUG, "Button 1: is for play\n"); 
	logger.msg(MSG_DEBUG, "Button 3: is for stop\n");

	Hardware::buttons[0].set_callbacks([file_name](){ 
		
		try{
			logger.msg(MSG_DEBUG, "playing: %s\n", file_name);
			Hardware::audio->play(file_name);
			logger.msg(MSG_DEBUG, "play command sent\n");
		}
		catch(const std::exception &e){
			logging_excp(logger, e.what());
		}
	});

	Hardware::buttons[2].set_callbacks([](){ 
		Hardware::audio->stop();
		logger.msg(MSG_DEBUG, "stop command sent\n");
	});

	Hardware::audio->finished_callback = [](){ logger.msg(MSG_DEBUG, "finished_callback\n"); };

	wait(-1);
}


void AudioTest::noise(const std::vector<std::string> &params)
{
    // if( !audio_play(file_name) ) return;

    // std::string cmd = "wget -O /data/autoinformer/wget.zip.tmp http://ftp.gnu.org/gnu/wget/wget-1.5.3.tar.gz";

    // while(int ret = audio_get_state()){

    //     if(system(cmd.c_str())){
    //         logging_err(logger, "'%s' failed\n", cmd);
    //         audio_play_stop();
    //         break;
    //     } 

    //     if(ret == -1){
    //         logging_err(logger, "audio_get_state() failed\n");
    //         audio_play_stop();
    //         break;
    //     } 

    //     sleep(5);
    // }
}
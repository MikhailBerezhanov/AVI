#include <cstdio>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <cerrno>

#include <string>
#include <iostream>
#include <fstream>
#include <functional>
#include <chrono>
#include <array>
#include <atomic>

#include "logger.hpp"

#include "i2c.hpp"
#include "nau8810.hpp"
#include "lcd1602.hpp"

#include "lc_client.hpp"

extern "C" {

#include <simcom_common.h>

#include <ATControl.h>
#include <ALSAControl.h>

// #include <sqlite3.h>
// #include <libconfig.h>

}

#ifndef APP_VERSION
#define APP_VERSION 	"1.3"
#endif

#define I2C_DEV			"/dev/i2c-5"

#define LOG_FILE_PATH   "/data/autoinformer/tests.log"

static Logging logger(MSG_DEBUG, "");
static std::atomic<bool> audio_finished{false};

static void SD_copy_test(int files_num);
static void SD_write_test(int files_num, int file_size_kb = 100);
static void SD_check_files_cs();
static void AT_cmd_test();
static void audio_play(char *file_name, int repeats = 0);
static void audio_play2(const std::string &file_path, int repeats = 0);
static void audio_stop();
static void audio_noise_test(const char* file_name);
static void ALSA_test();
static void LCD_test(int argc, char **argv);
static void NAU_test(int argc, char **argv);
static void LCC_test();


static void print_list_of_tests()
{
	printf("Autoinformer board functionality test utility (ver.%s)\n", APP_VERSION);
	printf("Supported list of tests:\n\n");
	printf("-V, --version\t\t- show util's version\n");
	printf("sd_copy <num>\t\t- num: number of copy operations to perform\n");
	printf("sd_write <num>\t\t- num: number of write operations to perform\n");
	printf("at\t\t\t- perform AT commands\n");
	printf("audio_play <file_name> [repeats]\t- play (E:/file.mp3) repeats times\n");
	printf("audio_play2 <file_name> [repeats]\t- play .mp3 file repeats times from FS (/sdcard)\n");
	printf("audio_stop\t\t- stop playing\n");
	printf("alsa\t\t\t- show current audio setting\n");
	printf("audio_gain <0..5>\t- set DAC-out level\n");
	printf("mic_gain <0..8>\t\t- set microphone-in level\n");
	// logger.msg(MSG_DEBUG, "5. i2c_test <rd | wr>\n");
	printf("lcd [addr] <dec_addr>\t- (optional) provide i2c chip address\n");
	printf("\\_ init\t\t\t- first time init LCD\n");
	printf("\\_ test\t\t\t- init and print test messages (default cmd)\n");
	printf("\\_ bl <0 | 1>\t\t- disable backlight\n");
	printf("\\_ c <0 | 1>\t\t- disable cursor\n");
	printf("\\_ b <0 | 1>\t\t- disable blinking cursor\n");
	printf("\\_ clear\t\t- clear screen\n");
	printf("\\_ home\t\t\t- return cursor to 0,0 position\n");
	printf("\\_ scroll <l | r>\t- scroll screen (left of right)\n");
	printf("\\_ ltr <0 | 1>\t\t- enable left to right printing\n");
	printf("\\_ autoscroll <0 | 1>\t- enable screen autoscroll\n");
	printf("\\_ set_c <row | col>\t- set cursor position\n");
	printf("\\_ puts <str>\t\t- print string\n");
	printf("\\_ putwc <unicode>\t- print unicode character\n");

	printf("nau\t\t\t- audio codec chip control\n");
	printf("\\_ reset\t\t- soft chip reset\n");
	printf("\\_ read\t\t\t- read chip state\n");
	printf("\\_ en_micbias\t\t- enable mic power\n");
	printf("\\_ dis_micbias\t\t- disable mic power\n");
	printf("\\_ sidetone\t\t- connect mic in with audio out\n");
	printf("\\_ en_mout_boost\t- enable audio out boost\n");
	printf("\\_ dis_mout_boost\t- disable audio out boost\n");
	printf("\\_ en_pga_boost\t\t- enable mic in boost\n");
	printf("\\_ dis_pga_boost\t- disable mic in boost\n");
	printf("\\_ pga_gain <0..63>\t- set microphone gain\n");
	printf("\\_ dac_gain <0..255>\t- set audio gain\n");

	printf("lcc\t\t\t- perform lc_client connection\n");

	printf("\n");
}



int main(int argc, char* argv[]) 
{

    if(argc < 2) {
    	print_list_of_tests();
    	return 1;
    }

    std::string arg = argv[1];

    if((arg == "-V") || (arg == "--version")){
    	printf("%s build: %s %s\n", APP_VERSION, __DATE__, __TIME__);
    	return 0;
    }

    i2c_init(I2C_DEV);

    try{

		if(arg == "sd_copy"){
			(argc == 3) ? SD_copy_test(atoi(argv[2])) : SD_copy_test(100);
		}
		else if(arg == "sd_write"){
			(argc == 3) ? SD_write_test(atoi(argv[2])) : SD_write_test(30);
		}
		else if(arg == "sd_write_check"){
			SD_check_files_cs();
		}
		else if(arg == "at"){
			AT_cmd_test();
		}
		else if(arg == "audio_play"){
			if(argc == 3){
				audio_play(argv[2]);
			}
			else if(argc > 3){
				audio_play(argv[2], atoi(argv[3]));
			}
		}
		else if(arg == "audio_play2"){
			if(argc == 3){
				audio_play2(argv[2]);
			}
			else if(argc > 3){
				audio_play2(argv[2], atoi(argv[3]));
			}
		}
		else if(arg == "audio_stop"){
		    audio_stop();
		}
		else if(arg == "audio_noise"){
		    (argc == 3) ? audio_noise_test(argv[2]) : audio_noise_test("e:/test.mp3");
		}
		else if(arg == "alsa"){
		    ALSA_test();
		}
		else if(arg == "lcd"){
			LCD_test(argc, argv);
		}
		else if(arg == "nau"){
		    NAU_test(argc, argv);
		}
		else if(arg == "mic_gain"){
		    if(argc != 3) goto error;

		    int val = atoi(argv[2]);

		    if (set_micgain_value(val) < 0){
		        logging_err(logger, "set_micgain_value(%d) failed\n", val);
		        return 1;
		    }

		    // logger.msg(MSG_DEBUG, "mic gain set to %d\n", val);
		}
		else if(arg == "audio_gain"){

		    if(argc != 3) goto error;

		    int val = atoi(argv[2]);

		    if (set_clvl_value(val) < 0){
		        logging_err(logger, "set_clvl_value(%d) failed\n", val);
		        return 1;
		    }

		    // logger.msg(MSG_DEBUG, "audio gain set to %d\n", val);
		}
		else if(arg == "lcc"){
		    LCC_test();
		}
		else{
	error:
	        print_list_of_tests();
	        return 1;
	    }
    }
    catch(const std::exception &e){
    	logging_excp(logger, "%s\n", e.what());
    }
    

    return 0;
}



// Сохранение БД Нормативно-Справочной-Информации в раскодированном бинарном виде (в виде tar архива)
static void save_nsi_bin(const uint8_t *file_content, size_t file_size, const std::string &file_ver) 
{
    std::string dest_dir = ".";
    std::string name = dest_dir + "/nsi." + file_ver + LC_FILE_EXT;
    lc::utils::write_bin_file(name, file_content, file_size);

    logger.msg(MSG_DEBUG, "Decoded nsi (ver.'%s') of size: %zu [B] has been saved\n", file_ver, file_size);

    // Распаковка при необходимости
    std::string db_name = lc::utils::unpack_tar(name, dest_dir);
    lc::utils::change_mod(dest_dir + "/" + db_name, 0666);
    remove(name.c_str());
    sync();
    logger.msg(MSG_DEBUG, "'%s' has been unpacked\n", db_name);
}

//
static void LCC_test()
{
    // Определяем настройки клиента
    LC_client::settings sets;
    LC_client::directories dirs;        // Будем использовать директории по умолчанию
    sets.device_id = 3487366287;        // Идентификатор устройства - Тестовый ID
    sets.system_id = sets.device_id;    // Идентификатор системы, в которой находится утройства (может совпадать) 

    sets.get["device_tgz"].dec_save = save_nsi_bin;

    LC_client lcc(&dirs, &sets);

    try{
        lcc.init();             // Инициализация клиентской части
        lcc.show_start();       // Информационное сообщение о запуске клиента

        lcc.rotate_sent_data(); // Проверка объема бэкаппа отправленных данных. Очистка при необходимости
        lcc.get_files();        // Запрос файлов, для которых выставлены колбеки, с сервера 
        lcc.put_files();        // Отправки накопленных файлов транзакций (sqlite)
        lcc.put_data();         // Отправки накопленных транзакций (protobuf)
    }
    catch(LC_no_connection &e){
        // фиксируем что связь отсутствует. сообщение об этом появится в show_results()
    }
    catch(const std::exception &e){
        logging_excp(logger, "%s\n", e.what());
    }

    // Возникшие ошибки в процессе работы клиента фиксируются и могут быть выведны методом show_results()
    lcc.show_results();    
    lcc.deinit(); 
}

static double calc_exec_time(std::function<void(void)> test_func)
{
	using namespace std::chrono;

	auto time_start = high_resolution_clock::now();
	test_func();
	auto time_finish = high_resolution_clock::now();
	auto time_span = duration_cast<duration<double>>(time_finish - time_start);

	return time_span.count();
}

//
static void SD_copy_test(int num)
{
	logger.msg(MSG_DEBUG, "Start %s\n", __func__);
	std::string cmd = "cp /sdcard/test.speed /data/media/";

    auto mass_copy = [cmd, num]()
    {
    	for(int i = 0; i < num; ++i){
			logger.msg(MSG_DEBUG, "Copying %d ..\n", i+1);
			if(system(cmd.c_str())){
				logging_err(logger, "'%s' failed\n", cmd);
				return;
			}
			remove("/data/media/test.speed");
		}
    };

    double sec = calc_exec_time(mass_copy);

	logger.msg(MSG_DEBUG, "Finished. Took: %lf sec\n", sec);
}


uint32_t crc32(uint32_t initial, const char *block, uint64_t size) 
{
	const uint32_t table[] = {
		0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
		0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
		0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
		0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
		0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
		0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
		0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
		0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
		0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
		0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
		0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
		0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
		0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
		0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
		0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
		0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
		0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
		0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
		0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
		0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
		0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
		0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
		0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
		0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
		0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
		0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
		0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
		0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
		0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
		0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
		0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
		0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
		0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
		0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
		0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
		0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
		0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
		0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
		0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
		0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
		0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
		0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
		0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
		0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
		0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
		0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
		0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
		0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
		0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
		0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
		0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
		0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
		0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
		0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
		0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
		0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
		0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
		0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
		0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
		0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
		0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
		0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
		0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
		0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
	};

	uint32_t crc = initial;

	while(size--) {
		crc = (crc >> 8) ^ table[(crc ^ *block++) & 0xFF];
	}

	return ~crc;
}

// check files control sum
static void SD_check_files_cs()
{
	std::string cmd = "md5sum -c /data/autoinformer/sd_write_check.md5";

	if(system(cmd.c_str())){
		logging_err(logger, "'%s' failed\n", cmd);
		return;
	}
}

static uint32_t SD_write_test_file(const std::string &name, int file_size = 100*1024, char content_symb = 'a')
{
	errno = 0;
	std::ofstream out_file(name, std::ios::out | std::ios::trunc | std::ios::binary);

	std::vector<char> test_content(file_size, content_symb);

	if( !out_file.is_open() ){
		throw std::runtime_error(std::string("could not open file: ") + strerror(errno));
	}

	char *content = &test_content[0];

	uint32_t content_cs = crc32(0, content, test_content.size());

	if( !out_file.write(content, test_content.size()) ){
		throw std::runtime_error(std::string("write error: ") + strerror(errno));
	}

	out_file.flush();
	out_file.close();

	//remove(name.c_str());

	return content_cs;
}

static void SD_write_test(int files_num, int file_size_kb)
{
	logger.msg(MSG_DEBUG, "Start %s\n", __func__);

	std::string dir_name = "/sdcard/write_tests";
	std::string cmd = "mkdir -p " + dir_name;
	std::string checksum_file = "/data/autoinformer/sd_write_check.md5";

	if(system(cmd.c_str())){
		logging_err(logger, "'%s' failed\n", cmd);
		return;
	}

	int err = 0;

	// remove old check-sums file
	remove(checksum_file.c_str());

	auto mass_write = [&err, files_num, file_size_kb, &dir_name, &checksum_file](){
		for(int i = 0; i < files_num; ++i){

			std::string file_name = dir_name + "/file" + std::to_string(i) + ".test";

			try{
				char symb = 'A' + i % 58;
				uint32_t cs = SD_write_test_file(file_name, file_size_kb * 1024, symb);
				// logger.msg(MSG_DEBUG, "Wrote %s (cs: %" PRIu32 ")\n", file_name, cs);

				std::string cmd = "md5sum " + file_name + " >> " + checksum_file;
				if(system(cmd.c_str())){
					logging_err(logger, "'%s' failed\n", cmd);
					return;
				}

			}
			catch(const std::exception &e){
				logging_err(logger, "%s failed: %s\n", file_name, e.what());
				++err;
			}
		}
	};

	double sec = calc_exec_time(mass_write);

	logger.msg(MSG_DEBUG, "Finished. Took: %lf sec, Errors: %d\n", sec, err);
}

static void AT_cmd_test()
{
    long timeout_ms = 5000;
    std::array<char, ARRAY_SIZE> buf{};

    logger.msg(MSG_DEBUG, "[ AT ] SIMCOM AT port: %s\n", SIMCOM_AT_PORT);

    if(int ret = atctrl_init() < 0) {
        logging_perr(logger, "[ AT ] atctrl_init() failed (%d)", ret);
        return ;
    }

    getModuleRevision(buf.data(), buf.size());
    logger.msg(MSG_DEBUG, "[ AT ] getModuleRevision(): %s\n", buf.data());

    std::fill(begin(buf), end(buf), 0);

    getIMEI(buf.data(), buf.size());
    logger.msg(MSG_DEBUG, "[ AT ] getIMEI(): %s\n", buf.data());

    std::fill(begin(buf), end(buf), 0);

    char resp[100] = {0};
    sendATCmd((char*)"AT+CICCID", resp, buf.data(), buf.size(), timeout_ms);
    logger.msg(MSG_DEBUG, "[ AT ] Read ICCID from SIM card: %s %s\n", resp, buf.data());
    std::fill(begin(buf), end(buf), 0);
    memset(resp, 0, sizeof resp);

    sendATCmd((char*)"AT+GMR", resp, buf.data(), buf.size(), timeout_ms);
    logger.msg(MSG_DEBUG, "[ AT ] Request TA revision identification of software release: %s %s\n", resp, buf.data());
    std::fill(begin(buf), end(buf), 0);
    memset(resp, 0, sizeof resp);

    sendATCmd((char*)"AT+CGPS?", resp, buf.data(), buf.size(), timeout_ms);
    logger.msg(MSG_DEBUG, "[ AT ] Start/Stop GPS status: %s %s\n", resp, buf.data());
    std::fill(begin(buf), end(buf), 0);
    memset(resp, 0, sizeof resp);

    sendATCmd((char*)"AT+CGPSINFO", resp, buf.data(), buf.size(), timeout_ms);
    logger.msg(MSG_DEBUG, "[ AT ] GPS fixed position information: %s %s\n", resp, buf.data());
    std::fill(begin(buf), end(buf), 0);
    memset(resp, 0, sizeof resp);

    atctrl_deinit();
}

// Play audio MP3 file
static void audio_play(char *file_name, int repeats)
{
	int ret = 0;

	ret = audio_play_start(file_name, repeats, 0);

	if(ret < 0) {
		logging_err(logger, "audio_play_start('%s') failed: %d\n", file_name, ret);
		return;
	}   
}

static void audio_stop()
{
	audio_play_stop();
}

static std::string get_file_name(const std::string &path)
{
	std::string res{path};
	size_t pos = path.find_last_of('/');
	if( (pos != std::string::npos) && ((pos + 1) < path.length()) ){
		res = path.substr(pos + 1);
	} 

	return res;
}

// Play audio MP3 file from FS
static void audio_play2(const std::string &file_path, int repeats)
{
	int ret = 0;

	std::string dest_name = "/data/media/tmp.mp3";
	std::string cmd = "cp " + file_path + " " + dest_name;
	if(system(cmd.c_str())){
		logging_err(logger, "cp failed\n");
		return;
	}

	sync();

	char internal_name[32];
	strcpy(internal_name, "e:/tmp.mp3");

	ret = audio_play_start(internal_name, repeats, 0);

	if(ret < 0) {
		logging_err(logger, "audio_play_start('%s') failed: %d\n", internal_name, ret);
		return;
	} 

	// remove(dest_name.c_str());
	while( audio_finished.load() != true ){
		usleep(500);
	}

	audio_finished.store(false);
	audio_play_stop();
}

//
static void ALSA_test()
{
    int ret = get_clvl_value();
    if(ret < 0) logging_err(logger, "get_clvl_value() failed: %d\n", ret);
    else logger.msg(MSG_DEBUG, "internal loudspeaker volume value: %d\n", ret);

    ret = get_micgain_value();
    if(ret < 0) logging_err(logger, "get_micgain_value() failed: %d\n", ret);
    else logger.msg(MSG_DEBUG, "micgain value: %d\n", ret);

    ret = get_csdvc_value();
    if(ret < 0) logging_err(logger, "get_csdvc_value() failed: %d\n", ret);
    else logger.msg(MSG_DEBUG, "voice channel value: %d\n", ret);

    ret = get_codec_ctrl();
    if(ret < 0) logging_err(logger, "get_codec_ctrl() failed: %d\n", ret);
    else logger.msg(MSG_DEBUG, "codec mode value: %d\n", ret);
}

//
static void LCD_test(int argc, char **argv)
{
    uint8_t lcd_addr = PCF8574A_ADDR;	// by default

	size_t cmd_idx = 1;

	if(argc >= 3){

		if(std::string(argv[2]) == "addr"){
			lcd_addr = (uint8_t)atoi(argv[3]);
			cmd_idx = 4;
		}
		else{
			cmd_idx = 2;
		}
	}

	// Создаем объект управдения дисплеем с обозначение адреса i2c
	WH1602B_CTK wlcd(lcd_addr);

	std::string cmd = argv[cmd_idx];

	// logger.msg(MSG_DEBUG, "cmd: %s, argc: %d, cmd_idx: %d\n", cmd, argc, cmd_idx);

	if((argc < 3) || (cmd == "test")){
		
		wlcd.init(lcd_addr);

		logger.msg(MSG_DEBUG, "Printing test messages...\n");

		for(int i = 0; i < 10; ++i){
			wlcd.print("АБВГДЕЖЗИЙКЛМНОП");
			wlcd.set_cursor(1, 0);
			wlcd.print("РСТУФХЦЧШЩЪЫЬЭЮЯ");
			sleep(5);
			wlcd.clear();

			wlcd.print("абвгдеёжзиклмноп");
			wlcd.set_cursor(1, 0);
			wlcd.print("рстуфхцчшщъыьэюя");
			sleep(5);
			wlcd.clear();
		}

		return;
	}

	// Инициализация необходима только вначале работы с дисплеем 1 раз
	if(cmd == "init"){
		logger.msg(MSG_DEBUG, "Trying lcd addr: 0x%02x\n", lcd_addr);
		wlcd.init(lcd_addr);
	}
	else if(cmd == "bl"){			// Backlight
		bool on = false;
		if(argc > (cmd_idx + 1)){
			on = (atoi(argv[cmd_idx + 1])) ? true : false;
		}

		logger.msg(MSG_DEBUG, "Setting lcd backlight: %s\n", on);
		wlcd.control(on);
	} 
	else if(cmd == "c"){	// Cursor
		bool on = false;

		if(argc > (cmd_idx + 1)){
			on = (atoi(argv[cmd_idx + 1])) ? true : false;
		}

		logger.msg(MSG_DEBUG, "Highlighting lcd cursor: %s\n", on);
        wlcd.control(true, on);
    }
    else if(cmd == "b"){	// Blinking cursor
		bool on = false;

		if(argc > (cmd_idx + 1)){
			on = (atoi(argv[cmd_idx + 1])) ? true : false;
		}

		logger.msg(MSG_DEBUG, "Setting lcd blink: %s\n", on);
        wlcd.control(true, true, on);
    }
	else if(cmd == "clear"){
		wlcd.clear();
	}
	else if(cmd == "home"){
		wlcd.return_home();
	}
	else if(cmd == "scroll"){
		bool left = true;
		if(argc > (cmd_idx + 1)){
			left = (std::string(argv[cmd_idx + 1]) == "l") ? true : false;
		}

		if(left){
			wlcd.scroll_left();
		}
		else{
			wlcd.scroll_right();
		}
	}
	else if(cmd == "ltr"){
		bool on = false;

		if(argc > (cmd_idx + 1)){
			on = (atoi(argv[cmd_idx + 1])) ? true : false;
		}

	    wlcd.left_to_right(on);
	}
	else if(cmd == "autoscroll"){
		bool on = false;

		if(argc > (cmd_idx + 1)){
			on = (atoi(argv[cmd_idx + 1])) ? true : false;
		}

		wlcd.autoscroll(on);
	}
	else if(cmd == "set_c"){
		uint8_t row = 0;
		uint8_t col = 0;

		if(argc > (cmd_idx + 2)){
			row = (uint8_t)atoi(argv[cmd_idx + 1]);
			col = (uint8_t)atoi(argv[cmd_idx + 2]);
		}

		wlcd.set_cursor(row, col);
	}
	else if(cmd == "puts"){
		if(argc <= (cmd_idx + 1)){
			logging_warn(logger, "No text string provided.\n");
			return;
		}

	    wlcd.print(argv[3]);
	}
	else if(cmd == "putwc"){
		if(argc <= (cmd_idx + 1)){
			logging_warn(logger, "No char code provided.\n");
			return;
		}

		wchar_t wc = atoi(argv[3]); // symbol unicode
		wlcd.print_ru(wc);
	}
    else{
    	logging_warn(logger, "Unsupported cmd: %s\n", cmd);
		return;
    }




    // lcd.print_str_ru("Hello bear!!");
    // for(int i = 0; i < 100; ++i){
    //     lcd.set_cursor(0, 0);
    //     lcd.print_str_ru("АБВГДЕЁЖ");
    //     usleep(300000);
    //     lcd.clear();

    //     lcd.set_cursor(1, 0);
    //     lcd.print_str_ru("ЗИКЛМОПР");
    //     usleep(300000);
    //     lcd.clear();
    // }

    // uint8_t bell[8]  = {0x4,0xe,0xe,0xe,0x1f,0x0,0x4};
    // uint8_t note[8]  = {0x2,0x3,0x2,0xe,0x1e,0xc,0x0};
    // uint8_t clock[8] = {0x0,0xe,0x15,0x17,0x11,0xe,0x0};
    // uint8_t heart[8] = {0x0,0xa,0x1f,0x1f,0xe,0x4,0x0};
    // uint8_t duck[8]  = {0x0,0xc,0x1d,0xf,0xf,0x6,0x0};
    // uint8_t check[8] = {0x0,0x1,0x3,0x16,0x1c,0x8,0x0};
    // uint8_t cross[8] = {0x0,0x1b,0xe,0x4,0xe,0x1b,0x0};
    // uint8_t retarrow[8] = {0x1,0x1,0x5,0x9,0x1f,0x8,0x4};

    // uint8_t rus_y[8] = {0b00000,0b00000,0b10001,0b10001,0b11101,0b10011,0b11101,0b00000};//ы

    // lcd.create_char(0, bell);
    // lcd.create_char(1, note);
    // lcd.create_char(2, clock);
    // lcd.create_char(3, heart);
    // lcd.create_char(4, duck);
    // lcd.create_char(5, check);
    // lcd.create_char(6, cross);
    // lcd.create_char(7, rus_y);
    // sleep(5);
  
    
    //lcd.print_str_ru("зиклмопр");

    // lcd.print_str("over I2C bus");

    // for(int i = 0; i < 8; ++i){
    //     lcd.print_char(i);
    // }

    //lcd.print_str_ru("Превед медвед!!");
}


static uint16_t read_nau_reg(const NAU8810 *chip, uint16_t reg, const char *reg_name)
{
    uint16_t value = chip->read_reg(reg);

    logger.msg(MSG_DEBUG, "[ NAU88U10 ] (0x%02X) '%s': 0x%04X\n", reg, reg_name, value);

    return value;
}

static void read_nau_registers(const NAU8810 *chip)
{
	logger.msg(MSG_DEBUG, "------------ POWER MANAGEMENT -----------\n");
	read_nau_reg(chip, 0x01, "Power Management 1"); //
	read_nau_reg(chip, 0x02, "Power Management 2"); //
	read_nau_reg(chip, 0x03, "Power Management 3"); //

	logger.msg(MSG_DEBUG, "------------- AUDIO CONTROL -------------\n");
	read_nau_reg(chip, 0x04, "Audio Interface"); //
	read_nau_reg(chip, 0x05, "Companding"); //
	read_nau_reg(chip, 0x06, "Clock Control 1"); //
	read_nau_reg(chip, 0x07, "Clock Control 2"); //
	read_nau_reg(chip, 0x0A, "DAC CTRL"); // 
	read_nau_reg(chip, 0x0B, "DAC Volume"); // 
	read_nau_reg(chip, 0x0E, "ADC CTRL"); // 
	read_nau_reg(chip, 0x0F, "ADC Volume"); // 

	logger.msg(MSG_DEBUG, "-------------- DAC LIMITER --------------\n");
	read_nau_reg(chip, 0x18, "DAC Limiter 1"); // 
	read_nau_reg(chip, 0x19, "DAC Limiter 2"); // 

	// logger.msg(MSG_DEBUG, "-------------- ALC CONTROL --------------\n");
	// read_nau_reg(chip, 0x20, "ALC CTRL 1");
	// read_nau_reg(chip, 0x21, "ALC CTRL 2");
	// read_nau_reg(chip, 0x22, "ALC CTRL 3");
	// read_nau_reg(chip, 0x23, "Noise Gate");

	logger.msg(MSG_DEBUG, "-------------- PLL CONTROL --------------\n");
	read_nau_reg(chip, 0x24, "PLL N CTRL"); // 
	read_nau_reg(chip, 0x25, "PLL K 1"); //
	read_nau_reg(chip, 0x26, "PLL K 2"); //
	read_nau_reg(chip, 0x27, "PLL K 3"); // 

	logger.msg(MSG_DEBUG, "----- INPUT, OUTPUT & MIXER CONTROL -----\n");
	read_nau_reg(chip, 0x28, "Attenuation CTRL");
	read_nau_reg(chip, 0x2C, "Input CTRL"); // 
	read_nau_reg(chip, 0x2D, "PGA Gain");
	read_nau_reg(chip, 0x2F, "ADC Boost");
	read_nau_reg(chip, 0x31, "Output CTRL"); // 
	read_nau_reg(chip, 0x32, "Mixer CTRL");
	read_nau_reg(chip, 0x36, "SPKOUT Volume"); // 
	read_nau_reg(chip, 0x38, "MONO Mixer Control"); // 

	logger.msg(MSG_DEBUG, "-------------- Register ID --------------\n");
	// read_nau_reg(chip, 0x3E, "Silicon Revision"); // 
	// read_nau_reg(chip, 0x3F, "2-Wire ID"); //         (0x001A)
	// read_nau_reg(chip, 0x40, "Additional ID"); //     (0x00CA)
	// read_nau_reg(chip, 0x41, "Reserved"); //          (0x0124)
	read_nau_reg(chip, 0x4E, "Control and Status"); //
	read_nau_reg(chip, 0x4F, "Output tie-off CTRL"); //
}

//
static void NAU_test(int argc, char **argv)
{
	NAU8810 nau;

	try{
		std::string cmd = argv[2];

		if(cmd == "reset") {nau.soft_reset();}
		else if(cmd == "read") {read_nau_registers(&nau);}
		else if(cmd == "sidetone") {nau.MIC_sidetone();}
		else if(cmd == "en_micbias") {nau.MIC_bias(true);}
		else if(cmd == "dis_micbias") {nau.MIC_bias(false);}

		else if(cmd == "en_mout_att") {nau.MOUT_attenuate(true);}
		else if(cmd == "dis_mout_att") {nau.MOUT_attenuate(false);}
		else if(cmd == "en_mout_mute") {nau.MOUT_mute(true);}
		else if(cmd == "dis_mout_mute") {nau.MOUT_mute(false);}
		else if(cmd == "en_mout_boost") {nau.MOUT_boost(true);}
		else if(cmd == "dis_mout_boost") {nau.MOUT_boost(false);}
		else if(cmd == "en_mout_res") {nau.MOUT_add_resistance(true);}
		else if(cmd == "dis_mout_res") {nau.MOUT_add_resistance(false);}

		else if(cmd == "dis_pga_boost") { nau.PGA_boost(false); }
		else if(cmd == "en_pga_boost") { nau.PGA_boost(true); }
		else if(cmd == "pga_gain") { nau.PGA_set_gain((uint8_t)atoi(argv[3])); }

		else if(cmd == "dac_gain") { nau.DAC_set_volume((uint8_t)atoi(argv[3])); }
		else if(cmd == "en_dac_mout") {nau.DAC_to_MOUT(true);}
		else if(cmd == "dis_dac_mout") {nau.DAC_to_MOUT(false);}
		else if(cmd == "en_dac_snr") {nau.DAC_best_SNR(true);}
		else if(cmd == "dis_dac_snr") {nau.DAC_best_SNR(false);}
		else if(cmd == "en_dac_smute") {nau.DAC_soft_mute(true);}
		else if(cmd == "dis_dac_smute") {nau.DAC_soft_mute(false);}
		else if(cmd == "en_dac_amute") {nau.DAC_automute(true);}
		else if(cmd == "dis_dac_amute") {nau.DAC_automute(false);}
		else if(cmd == "en_dac_deemp") {nau.DAC_de_emphasis((uint8_t)atoi(argv[3]));}
		else if(cmd == "dis_dac_deemp") {nau.DAC_de_emphasis(0);}

		else if(cmd == "en_dac_lim") {nau.DAC_enable_limiter(true);}
		else if(cmd == "dis_dac_lim") {nau.DAC_enable_limiter(false);}
		else if(cmd == "dac_lim_thr") {nau.DAC_set_limiter_threshold((int8_t)atoi(argv[3]));}
		else if(cmd == "dac_lim_boost") {nau.DAC_set_limiter_boost((int16_t)atoi(argv[3]));}
	}
	catch (const std::runtime_error& e){
		logging_err(logger, "%s\n", e.what());
	}

}


static void audio_noise_test(const char* file_name)
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



// Dummy for ALSAControl.c
void process_simcom_ind_message(simcom_event_e event, void *cb_usr_data)
{
	switch(event){
		case SIMCOM_EVENT_AUDIO:
            //printf("%d\n",*(char *)cb_usr_data);
            printf("SIMCOM_EVENT_AUDIO catched\n");

            audio_finished.store(true);

            break; 
	}
}
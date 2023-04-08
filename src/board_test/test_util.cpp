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
#include <memory>

#include "logger.hpp"

#include "at_test.hpp"
#include "lcd_test.hpp"
#include "nau_test.hpp"
#include "audio_test.hpp"
#include "sd_test.hpp"
#include "gpio_test.hpp"
#include "uart_test.hpp"

#define VERSION 		"1.7"

#ifndef _SHARED_LOG
static Logging logger;
#endif

static void LCC_test();


static void print_list_of_tests()
{
	printf("Autoinformer board functionality test utility (ver.%s)\n", APP_VERSION);
	printf("Supported list of tests:\n\n");
	printf("-V, --version\t- show util's version\n");

	printf("sd\t\t- SD card tests\n");
	printf("at\t\t- AT commands tests\n");
	printf("audio\t\t- Audio control tests\n");
	printf("lcd\t\t- LCD tests \n");
	printf("nau\t\t- Audio codec NAU88U10 tests\n");
	printf("gpio\t\t- GPIO tests\n");
	printf("uart\t\t- UART tests\n");

	printf("\n");
}

int main(int argc, char* argv[]) 
{
	logger.init(MSG_DEBUG, "");
	logger.set_module_name("");

	if(argc < 2) {
		print_list_of_tests();
		return 1;
	}

	std::string arg = argv[1];

	if((arg == "-V") || (arg == "--version")){
		printf("%s build: %s %s\n", VERSION, __DATE__, __TIME__);
		return 0;
	}

	// Parse input args
	std::string test_name;
	size_t param_start_idx = 2;

	if(argc >= 3){
		test_name = argv[2];
		param_start_idx = 3;
	}

	std::vector<std::string> params;
	for(int i = param_start_idx; i < argc; ++i){
		params.push_back(argv[i]);
	}

	std::unique_ptr<BaseTest> ptest;

	try{
		if(arg == "audio"){
			ptest = std::unique_ptr<BaseTest>(new AudioTest);
		}
		else if(arg == "sd"){
			ptest = std::unique_ptr<BaseTest>(new SDcardTest);
		}
		else if(arg == "gpio"){
			ptest = std::unique_ptr<BaseTest>(new GPIOTest);
		}
		else if(arg == "at"){
			ptest = std::unique_ptr<BaseTest>(new ATTest);
		}
		else if(arg == "lcd"){
			ptest = std::unique_ptr<BaseTest>(new LCDTest);
		}
		else if(arg == "nau"){
			ptest = std::unique_ptr<BaseTest>(new NAUTest);
		}
		else if(arg == "uart"){
			ptest = std::unique_ptr<BaseTest>(new UARTTest);
		}
		else{
			print_list_of_tests();
			return 0;
		}

		// Perform selected test
		ptest->run(test_name, params);
    }
    catch(const std::out_of_range &e){
    	// unsupported test command
    	if(ptest){
    		ptest->show_tests_info();
    	}
    }
    catch(const std::exception &e){
    	logging_excp(logger, "%s\n", e.what());
    }
    
    return 0;
}






// Сохранение БД Нормативно-Справочной-Информации в раскодированном бинарном виде (в виде tar архива)
// static void save_nsi_bin(const uint8_t *file_content, size_t file_size, const std::string &file_ver) 
// {
//     std::string dest_dir = ".";
//     std::string name = dest_dir + "/nsi." + file_ver + LC_FILE_EXT;
//     lc::utils::write_bin_file(name, file_content, file_size);

//     logger.msg(MSG_DEBUG, "Decoded nsi (ver.'%s') of size: %zu [B] has been saved\n", file_ver, file_size);

//     // Распаковка при необходимости
//     std::string db_name = lc::utils::unpack_tar(name, dest_dir);
//     lc::utils::change_mod(dest_dir + "/" + db_name, 0666);
//     remove(name.c_str());
//     sync();
//     logger.msg(MSG_DEBUG, "'%s' has been unpacked\n", db_name);
// }

// //
// static void LCC_test()
// {
//     // Определяем настройки клиента
//     LC_client::settings sets;
//     LC_client::directories dirs;        // Будем использовать директории по умолчанию
//     sets.device_id = 3487366287;        // Идентификатор устройства - Тестовый ID
//     sets.system_id = sets.device_id;    // Идентификатор системы, в которой находится утройства (может совпадать) 

//     sets.get["device_tgz"].dec_save = save_nsi_bin;

//     LC_client lcc(&dirs, &sets);

//     try{
//         lcc.init();             // Инициализация клиентской части
//         lcc.show_start();       // Информационное сообщение о запуске клиента

//         lcc.rotate_sent_data(); // Проверка объема бэкаппа отправленных данных. Очистка при необходимости
//         lcc.get_files();        // Запрос файлов, для которых выставлены колбеки, с сервера 
//         lcc.put_files();        // Отправки накопленных файлов транзакций (sqlite)
//         lcc.put_data();         // Отправки накопленных транзакций (protobuf)
//     }
//     catch(LC_no_connection &e){
//         // фиксируем что связь отсутствует. сообщение об этом появится в show_results()
//     }
//     catch(const std::exception &e){
//         logging_excp(logger, "%s\n", e.what());
//     }

//     // Возникшие ошибки в процессе работы клиента фиксируются и могут быть выведны методом show_results()
//     lcc.show_results();    
//     lcc.deinit(); 
// }


#include <csignal>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <memory>

extern "C"{
#include <unistd.h>
}

#define LOG_MODULE_NAME   "[  M  ]"
#include "logger.hpp"

#include "app.hpp"

static std::atomic<int> stop_main;

static inline void signal_handler_init(std::initializer_list<int> signals)
{
	stop_main.store(0);

	auto signal_handler = [](int sig_num){ stop_main.store(sig_num); };

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;        
	sigemptyset(&act.sa_mask);     

	for (const auto sig : signals){
		sigaddset(&act.sa_mask, sig);
		sigaction(sig, &act, nullptr);
	}                                        
}

int main(int argc, char *argv[]) 
{
	// Обработка переданных ключей
	bool conf_trace = false;

	if(argc > 1){
		std::string key{argv[1]};
		if( (key == "--version") || (key == "-V") ){
			printf("Version: %s (build: %s %s)\n", APP_VERSION, __DATE__, __TIME__);
			return 0;
		}
		else if(key == "--conf-trace"){
			conf_trace = true;
		}
		else{
			printf("Unsupported key. List of available keys:\n");
			printf("\t--version, -V\tshow application version\n");
			printf("\t--conf-trace \tenable application configuration log\n");
			return 0;
		}
	}

	// Запуск приложения
	try{
		signal_handler_init({SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGPIPE});

		std::unique_ptr<avi::AVI> avi{new avi::AVI};
		// app::AVI avi;
		avi->init(conf_trace);
		// app::init(conf_trace);

		for(;;){
			sleep(10); 	// Ожидание сигналов от ОС (сигналы прерывают сон от sleep())

			int sig_num = stop_main.load();
			if( !sig_num ){
				continue;
			} 

			log_info("Catched signal: %d\n", sig_num);

			// Выбор реакции на пришедший сигнал
			switch(sig_num){

				case SIGHUP: 	// Перезагрузка конфигурации приложения
					//app::reload();
					stop_main.store(0);
					break;

				// Cигнал SIGPIPE может возникать в процессе работы библиотек (curl, fastcgi) при 
				// запоздании ответа на запрос. Это не критично -> добавляем игнорирование сигнала:
				case SIGPIPE:	// Игнорирование
					log_warn("SIGPIPE\n");
					stop_main.store(0);
					break;

				default: 		// Остановка приложения
					avi->create_sys_event("POWER_OFF", APP_VERSION);
					avi->deinit();
					// app::create_sys_event("POWER_OFF", APP_VERSION);
					// app::deinit();

					log_msg(MSG_DEBUG | MSG_TO_FILE, _BOLD "~ %s %s (%d) ~" _RESET "\n", APP_NAME,
						(sig_num == SIGINT ? "interrupted" : ( sig_num == SIGTERM ? "terminated" : 
						(sig_num == SIGQUIT ? "quited" : "killed"))), sig_num);
					return 0;
			}
		}

		return 0;      
	}
	catch(const std::exception &e){
		log_excp("%s\n", e.what());
		return 1;
	}
	catch(...){
		log_err("Unknown exception occured\n");
		return 2;
	}
}

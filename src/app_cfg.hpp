/*==============================================================================
Описание: 	Модуль работы с файлом конфигурации и задания настроек приложения.

Автор: 		berezhanov.m@gmail.com
Дата:		22.04.2022
Версия: 	1.0
==============================================================================*/

#pragma once

#include <cstdint>
#include <string> 
#include <vector> 

#include "lc_client.hpp"
#include "logger.hpp"
#include "app.hpp"

namespace avi{

class Config
{
public:
	Config() = default;
	Config(const std::string &fname) { this->open(fname); }
	~Config(){ this->close(); } 

	// Определение местоположения файла конфигурации и инициализация при нахождении
	std::string open(const std::string &file_name);
	void close();

	std::string read_app_settings(AVI::Settings &out, AVI::Directories &dirs);

	std::string read_lcc_settings(LC_client::settings &out, LC_client::directories &dirs);

	std::string get_file_path() const { return file_path; }

	// Уровень логирования при конфигурации 
	log_lvl_t log_level = MSG_SILENT; 

private:
	// Выбранный путь к конфиг файлу с учетом пользовательского пожелания и default_paths
	std::string file_path;
	bool opened = false;

	// Расположения конфиг-файла по умолчанию (имеют более высокий приоритет над пользовательским)
	const std::vector<std::string> default_paths = {
		"/etc/ainf.conf",
	};
};

}//namespace

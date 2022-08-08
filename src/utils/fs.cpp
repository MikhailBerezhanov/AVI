
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <array>

extern "C"{
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
}

#include "utility.hpp"
#include "fs.hpp"

namespace utils
{

// Получение текущего абсолютного пути рабочей директории 
std::string get_cwd()
{
	int cnt = 0;
	char buf[512] = {0};
	cnt = readlink("/proc/self/exe", buf, sizeof buf);

	if(cnt <= 0){
		throw std::runtime_error(excp_func((std::string)"readlink '/proc/self/exe' failed: " + strerror(errno)));
	} 

	char *sc = strrchr(buf, '/');
	if(sc){
		*sc = '\0'; // исключить собственное имя исполняемого файла
	} 
	return std::string(buf);
}

//  Создание новой директории если таковой еще не существует
void make_new_dir(const std::string &path, mode_t mode)
{
	if(path.empty()){
		return;
	}

	std::string cmd_ret = exec_piped("mkdir -p " + path + " 2>&1 | tr -d '\n'");

	if( !cmd_ret.empty() ){
		 throw std::runtime_error(excp_func("of '" + path + "' failed: " + cmd_ret));
	}

	change_mod(path, mode);
}

// Изменение прав доступа к файлу 
void change_mod(const std::string &path, mode_t mode)
{
	if(chmod(path.c_str(), mode) == -1){
		std::runtime_error(excp_func("of '" + path + "' failed: " + strerror(errno)));
	} 
}

// Определение размера директории включая вложенные поддиректории
uint64_t get_dir_size(const std::string &dir_name, bool nesting)
{
	uint64_t total = 0;
	std::unique_ptr<DIR, int(*)(__dirstream*)> dirp (opendir(dir_name.c_str()), closedir);
	struct dirent *entry = nullptr;

	if( !dirp ){
		throw std::runtime_error(excp_func("opendir '" + dir_name + "' failed: " + strerror(errno)));
	} 

	while( (entry = readdir(dirp.get())) != nullptr ){

		std::string entry_name = dir_name + "/" + entry->d_name;

		// Если разрешена вложенность, считаем размер поддиректорий
		if( nesting && (entry->d_type == DT_DIR) && strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") ){
			total += get_dir_size(entry_name, nesting);;
		}
		// Считаем размер файлов
		else if(entry->d_type == DT_REG){
			total += get_file_size(entry_name);
		}
	}

	return total;
}

// Определение количества файлов и поддиректорий в директории
uint32_t get_entries_num_in_dir(const std::string &dir_name)
{
	uint32_t count = 0;
	struct dirent *entry = nullptr;
	std::unique_ptr<DIR, int(*)(__dirstream*)> dirp (opendir(dir_name.c_str()), closedir); 

	if(!dirp) throw std::runtime_error(excp_func("opendir '" + dir_name + "' failed: " + strerror(errno)));

	while( (entry = readdir(dirp.get())) != nullptr ) {

		if( (entry->d_type == DT_REG) || ((entry->d_type == DT_DIR) && strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ){
			++count;
		}
	}

	return count;
}

//  Получение кол-ва файлов в директории
uint32_t get_files_num_in_dir(const std::string &dir_name, const std::string &ext, bool nesting)
{
	uint32_t file_count = 0;
	struct dirent *entry = nullptr;
	std::unique_ptr<DIR, int(*)(__dirstream*)> dirp (opendir(dir_name.c_str()), closedir); 

	if( !dirp ){
		throw std::runtime_error(excp_func("opendir '" + dir_name + "' failed: " + strerror(errno)));
	} 

	while( (entry = readdir(dirp.get())) != nullptr ) {

		if( nesting && (entry->d_type == DT_DIR) && strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") ){
			std::string entry_name = dir_name + "/" + entry->d_name;
			file_count += get_files_num_in_dir(entry_name, ext, nesting);
		}
		else if(entry->d_type == DT_REG){
			// Поиск любых файлов
	        if( ext.empty() ){
	            ++file_count;
	        }
	        // Поиск файлов с заданным расширением
	        else{
	            std::string fn{entry->d_name};
	            size_t pos = fn.find_last_of('.');
	            if( (pos != std::string::npos) && (fn.substr(pos) == ext) ){
	                ++file_count;
	            }
	        }
		}
	}

	return file_count;
}

//  Получение имен файлов в директории
std::vector<std::string> get_file_names_in_dir(
	const std::string &dir_name, 
	const std::string &ext,
	uint32_t max_num,
	uint64_t max_size)
{
	uint64_t total_size = 0;

	std::unique_ptr<DIR, int(*)(__dirstream*)> dirp (opendir(dir_name.c_str()), closedir);
	struct dirent *entry = nullptr;
	std::vector<std::string> vec;

	if(!dirp) throw std::runtime_error("opendir '" + dir_name + "' failed: " + strerror(errno));

	while( (entry = readdir(dirp.get())) != nullptr )
	{
		bool to_add = false;

		// Проверка достижения максимального числа обрабатываемых файлов
		if(max_num && (vec.size() >= max_num)) break;

		// Проверка достижения максимального размера обрабатываемых файлов [Б]
		if(max_size && (total_size > max_size)) break;

		// Исключить из поиска относительные переходы
		if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

		// Игнорировать все кроме файлов
		if( entry->d_type != DT_REG ) continue;

		// Поиск любых файлов
		if( ext.empty() ){
			to_add = true;
		}
		// Поиск файлов с заданным расширением
		else{
			std::string fn{entry->d_name};
			size_t pos = fn.find_first_of('.');
			if( (pos != std::string::npos) && (fn.substr(pos) == ext) ){
				to_add = true;
			}
	    }

		if(to_add){
			uint64_t fsize = get_file_size(dir_name + "/" + entry->d_name);
			if( max_size && (fsize > (max_size - total_size)) ) continue;
			vec.push_back(entry->d_name);
			total_size += fsize;
		}
	}

	return vec;
}

// Удаление верхних файлов (в том числе поддиректорий) из заданной директории
void remove_head_files_from_dir(const std::string &dir_name, uint32_t files_num, bool async)
{
	std::string cmd = "for i in $(ls " + dir_name + " | head -" + std::to_string(files_num) + " ); do rm -rf " + dir_name + "/$i; done";
	if(async) cmd += " &";

	exec(cmd.c_str()); 
}

// Получение размера файла
uint64_t get_file_size (const std::string &fname)
{
	errno = 0;
	struct stat st;
	if(0 != stat(fname.c_str(), &st)){
		if(errno == ENOENT){
			throw(file_not_found("File '" + fname + "' not found"));
		} 

		throw std::runtime_error(excp_func("File '" + fname + "' stat failed: " + strerror(errno)));
	}

	return static_cast<uint64_t>(st.st_size);
}

// Получение имени файла из полного пути
std::string file_short_name(const std::string &path)
{
	std::string res{path};
	size_t pos = path.find_last_of('/');
	if( (pos != std::string::npos) && ((pos + 1) < path.length()) ){
		res = path.substr(pos + 1);
	} 

	return res;
}

// Получение расширения файла из полного пути
std::string file_extension(const std::string &path)
{
	std::string extension;
	size_t pos = path.find_last_of('.');

	if( pos != std::string::npos ){
		extension = path.substr(pos);
	}

	return extension;
}

// Проверка существования файла
bool file_exists(const std::string &fname)
{
	try{
		get_file_size(fname);
	}
	catch(const utils::file_not_found &e){
		return false;
	}

	return true;
}

// Получение имени файла из полного пути
std::string get_file_name(const std::string &path)
{
	std::string res;
	size_t pos = path.find_last_of('/');
	if( (pos != std::string::npos) && ((pos + 1) < path.length()) ){
		res = path.substr(pos + 1);
	} 

	return res;
}

// Получение имени директории из полного пути
std::string get_dir_name(const std::string &path)
{
	std::string directory = path;

	const size_t last_slash_idx = path.rfind('/');
	if(std::string::npos != last_slash_idx){
	    directory = path.substr(0, last_slash_idx);
	}

	return directory;
}

// Запись в бинарный файл
void write_bin_file(const std::string &fname, const uint8_t *buf, size_t buf_len)
{
	std::ofstream file(fname, std::ofstream::binary);

	if( !file.is_open() ){
		throw std::runtime_error(excp_func("File '" + fname + "' open for writing failed"));
	} 

	if( !file.write(reinterpret_cast<const char*>(buf), buf_len) ){
		throw std::runtime_error(excp_func("File '" + fname + "' write failed"));
	} 

	file.flush();
	file.close();
}

// Чтение бинарного файла
std::unique_ptr<uint8_t[]> read_bin_file(const std::string &fname, uint64_t *buf_len)
{
	std::ifstream file(fname, std::ifstream::binary);
	if( !file.is_open() ){
		throw std::runtime_error(excp_func("File '" + fname + "' open for reading failed"));
	} 

	uint64_t size = get_file_size(fname);
	std::unique_ptr<uint8_t[]> buf_ptr(new (std::nothrow) uint8_t[size]); 
	if( !buf_ptr ){
		throw std::runtime_error(excp_func("Failed to alloc buf for '" + fname + "' (" + std::to_string(size) + " bytes)"));
	} 

	if( !file.read(reinterpret_cast<char*>(buf_ptr.get()), size) ){
		throw std::runtime_error(excp_func("File '" + fname + "' read failed"));
	} 
	file.close();
	if(buf_len){
		*buf_len = size;
	} 
	return buf_ptr;
}

//Запись в текстовый файл потока символов
void write_text_file(const std::string &fname, const char *buf, size_t buf_len)
{
	std::ofstream file(fname);

	if( !file.is_open() ){
		throw std::runtime_error(excp_func("File '" + fname + "' open for writing failed"));
	} 

	if( !file.write(buf, buf_len) ){
		throw std::runtime_error(excp_func("File '" + fname + "' write failed"));
	} 

	file.flush();
	file.close();
}

void write_text_file(const std::string &fname, const std::string &data)
{
	write_text_file(fname, data.c_str(), data.length());
}

// Чтение текстового файла в вектор строк
void read_text_file(const std::string &fname, std::vector<std::string> &vec)
{
	errno = 0;
	std::ifstream file(fname);
	if(!file.is_open()){
		if(errno == ENOENT){
			throw(file_not_found("File '" + fname + "' not found"));
		} 

		throw std::runtime_error(excp_func("File '" + fname + "' open failed: " + strerror(errno)));
	}

	std::string str;

	while(getline(file, str)) vec.push_back(str);

	file.close();
}

// Чтение текстового файла в динамическую память
std::unique_ptr<char[]> read_text_file(const std::string &fname)
{
	errno = 0;
	std::ifstream file(fname);
	if( !file.is_open() ) {
		if(errno == ENOENT){
			throw(file_not_found("File '" + fname + "' not found"));
		} 

		throw std::runtime_error(excp_func("File '" + fname + "' open failed: " + strerror(errno)));
	}
	uint64_t buf_len = get_file_size(fname);

	std::unique_ptr<char[]> buf_ptr(new (std::nothrow) char[buf_len+1]);

	if( !buf_ptr ){
		throw std::runtime_error(excp_func("Failed to alloc buf for '" + fname + "' (" + std::to_string(buf_len+1) + " bytes)"));
	}

	if( !file.read(buf_ptr.get(), buf_len) ){
		throw std::runtime_error(excp_func("File '" + fname + "' read failed"));
	} 
	(buf_ptr.get())[buf_len] = 0;  // конец С-строки
	file.close();

	return buf_ptr;
}

// Чтение текстового файла в строку
std::string read_text_file_to_str(const std::string &fname)
{
	std::ifstream file(fname);
	if(!file.is_open()){
		throw std::runtime_error(excp_func("File '" + fname + "' open failed: " + strerror(errno)));
	} 

	std::stringstream ss;
	if( !(ss << file.rdbuf()) ){
		throw std::runtime_error(excp_func("File '" + fname + "' rdbuf failed"));
	} 
	return ss.str();
}


} // namespace utils


#ifdef _UTILS_TEST

#include <cinttypes>

#define LOG_MODULE_NAME   "[ UTILS ]"
#include "logger.hpp"

int main(int argc, char *argv[])
{
	logger.init(MSG_TRACE, "utils_test.log", 1, 1000);
	log_msg(MSG_VERBOSE | MSG_TO_FILE, "UTILS module unit test\n");

	std::string iface_name = "enp0s3";

	try{

		log_msg(MSG_VERBOSE, "get_file_size(): %" PRIu64 "\n", utils::get_file_size("utils_test.log"));

		// utils::make_new_dir("./new_dir_test");

		// utils::file_base64_encode("app.db", "app.db.b64");

		void *mem_ptr;
		std::string s;
		std::string s2{"1234567890"};

		log_msg(MSG_TRACE, "Ptr size: %zu, Empty string size: %zu, String size: %zu\n", 
			sizeof mem_ptr, sizeof s, sizeof s2);

	}
	catch(const std::runtime_error& e){
	    log_excp("%s\n", e.what());
	}

	return 0;
}

#endif
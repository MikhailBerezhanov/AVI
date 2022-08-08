/*============================================================================== 
Описание: 	Модуль с набором утилит для работы с файловой системой, 
			командной оболочкой и системой в целом.
			
Автор: 		berezhanov.m@gmail.com
Дата:		01.11.2021
Версия: 	1.0
==============================================================================*/

#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <sys/stat.h>

namespace utils{

// -------------------- Операции с файловой системой  --------------------
/**
  * @описание   Получение текущего абсолютного пути рабочей директории.
  * @примечание (где расположен исполняемый файл)
  * @возвращает путь к исполняемому файлу (не включая его имя).
  * @исключения std::runtime_error
 */
std::string get_cwd();

/**
  * @описание   Создание новой директории если таковой еще не существует.
  * @параметры
  *     Входные:
  *         path - строка содержащая полный путь создаваемой директории
  *         mode - флаги прав доступа к новой директории (например 0666)
  * @исключения std::runtime_error
 */
void make_new_dir(const std::string &path, mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

/**
  * @описание   Изменение прав доступа к файлу 
  * @параметры
  *     Входные:
  *         path - строка содержащая полный к файлу
  *         mode - новые флаги доступа (например 0666)
  * @исключения std::runtime_error
 */
void change_mod(const std::string &path, mode_t mode);

/**
  * @описание   Определение размера директории (включая вложенные поддиректории).
  * @параметры
  *     Входные:
  *         dir_name - имя директории
  * 		nesting - разрешение рекурсивного обхода поддиректорий
  * @возвращает Размер директории
  * @исключения std::runtime_error
 */
uint64_t get_dir_size(const std::string &dir_name, bool nesting = false);

/**
  * @описание   Определение количества файлов и поддиректорий в директории.
  * @параметры
  *     Входные:
  *         dir_name - имя директории
  * @возвращает кол-во файлов и поддиректорий в директории.
  * @исключения std::runtime_error
 */
uint32_t get_entries_num_in_dir(const std::string &dir_name);

/**
  * @описание   Определение количества файлов в директории и поддиректориях (рекурсивно).
  * @параметры
  *     Входные:
  *         dir_name - имя директории
  *  		    ext 	   - расширение файлов
  *         nesting  - разрешение рекурсивного обхода поддиректорий
  * @возвращает кол-во файлов
  * @исключения std::runtime_error
 */
uint32_t get_files_num_in_dir(const std::string &dir_name, const std::string &ext = "", bool nesting = false);

/**
  * @описание   Определение имен файлов в директории.
  * @параметры
  *     Входные:
  * 		dir_name - имя директории (С-строка)
  *			ext - расширение файлов для поиска ("" для любого расширения)
  *			max_num - максимально допустимое кол-во файлов для поиска
  *			max_size - максимальный суммарный размер файлов для поиска
  * @возвращает вектор с именами файлов в директории.
  * @исключения std::runtime_error
 */
std::vector<std::string> get_file_names_in_dir(
	const std::string &dir_name, 
	const std::string &ext = "",
	uint32_t max_num = 0,
	uint64_t max_size = 0);

/**
  * @описание   Удаление files_num верхних файлов (в том числе поддиректорий) из заданной директории.
  * @параметры
  *     Входные:
  *         dir_name - имя директории
  *         files_num - количество удаляемых файлов\поддиректорий
  *         async - разрешение фонового режима выполнения
 */
void remove_head_files_from_dir(const std::string &dir_name, uint32_t files_num, bool async = false);

/**
  * @описание   Определение размера файла в байтах.
  * @параметры
  *     Входные:
  * 		fname - абсолютное имя файла
  * @возвращает размер файла.
  * @исключения std::runtime_error
 */
uint64_t get_file_size(const std::string &fname);

/**
  * @описание   Получение имени файла из полного пути
  * @параметры
  *     Входные:
  *         path - строка содержащая полный путь к файлу
  * @возвращает имя файла
 */
std::string file_short_name(const std::string &path);

/**
  * @описание   Получение расширения файла из полного пути
  * @параметры
  *     Входные:
  *         path - строка содержащая полный путь к файлу
  * @возвращает расширение файла ".ext"
 */
std::string file_extension(const std::string &path);

// Проверка существования файла
bool file_exists(const std::string &fname);

/**
  * @описание   Получение имени файла из полного пути
  * @параметры
  *     Входные:
  *         path - строка содержащая полный путь к файлу
  * @возвращает имя файла
 */
std::string get_file_name(const std::string &path);

/**
  * @описание   Получение имени директории из полного пути
  * @параметры
  *     Входные:
  *         path - строка содержащая полный путь к файлу
  * @возвращает имя файла
 */
std::string get_dir_name(const std::string &path);

/**
  * @описание   Запись массива байт в файл.
  * @параметры
  *     Входные:
  * 		fp - файловый дескриптор заранее открытого файла
  *			buf - указатель на массив байт для записи в файл
  *			buf_len - размер массива
  * @исключения std::runtime_error
 */
void write_bin_file(const std::string &fname, const uint8_t *buf, size_t buf_len);

// Чтение бинарного файла
std::unique_ptr<uint8_t[]> read_bin_file(const std::string &fname, uint64_t *buf_len);

// Запись в текстовый файл потока символов
void write_text_file(const std::string &fname, const char *buf, size_t buf_len);

// Запись в текстовый файл строки
void write_text_file(const std::string &fname, const std::string &data);

// Чтение текстового файла в вектор строк
void read_text_file(const std::string &fname, std::vector<std::string> &vec);

// Чтение текстового файла в массив char динамической памяти
std::unique_ptr<char[]> read_text_file(const std::string &fname);

// Чтение такстового файла в cтроку
std::string read_text_file_to_str(const std::string &fname);


} // namespace utils



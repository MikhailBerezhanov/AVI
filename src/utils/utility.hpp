#pragma once

#include <cerrno>
#include <cstring>
#include <string>
#include <stdexcept>
#include <functional>

#define excp_func(str) ( (std::string)__func__ + "():" + std::to_string(__LINE__) + " " + (str) )

namespace utils{
	
struct file_not_found: public std::runtime_error
{
  file_not_found() = default;
  file_not_found(const std::string &s): std::runtime_error(s) {}
};

constexpr uint64_t B_to_MB(uint64_t x) { return ((x) / 1048576); }
constexpr uint64_t B_to_KB(uint64_t x) { return ((x) / 1024); }
// ГБайты -> Байты
constexpr uint64_t GB_to_B(uint64_t x) { return ((x) * 1073741824); }
// МБайты -> Байты
constexpr uint64_t MB_to_B(uint64_t x) { return ((x) * 1048576); }
// КБайты -> Байты
constexpr uint64_t KB_to_B(uint64_t x) { return ((x) * 1024); }

/**
  * @описание   Выполнить команду в оболочке и считать вывод через пайп в строку
  * @параметры
  *     Входные:
  *         cmd - строка, содержащая команду оболочки среды
  *         is_empty_error - выкидывать ли исключение если вывод пустой (true - выкидывать)
  * @возвращает результат выполнения команды.
  * @исключения std::runtime_error в случае ошибки открытия пайпа или пустого вывода
 */
std::string exec_piped(const std::string &cmd, bool empty_is_error = false);

/**
  * @описание   Выполнить команду в оболочке
  * @параметры
  *     Входные:
  *         cmd - строка, содержащая команду оболочки среды
  * @исключения std::runtime_error в случае ошибки выполнения команды
 */
bool exec(const std::string &cmd, bool no_throw = false);

// Получение идентификатора текущего потока
std::string get_thread_id(void);

double calc_exec_time(std::function<void(void)> test_func);

std::string hex_to_string(const unsigned char *in, size_t len);

} //namespace utils
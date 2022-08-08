#pragma once

#include <cstring>
#include <ctime>

namespace utils{

// -------------------- Операции с системным временем --------------------
// Получить текущее локальное время системы в виде строки (формат 210215203031_543)
std::string get_local_datetime(bool with_ms = true);
// Получить текущее локальное время системы в виде структуры
void get_local_datetime(struct tm *result);
// Получить текущее локальное время системы в виде строки (формат 19.03.2021 20:30:31.543)
std::string get_fmt_local_datetime(const char *fmt = "%d.%m.%Y %T", bool with_ms = false, const time_t *sec = nullptr);
// Преобразовать строку времени в структуру
struct tm parse_time_str(const std::string &str, const char *fmt = "%d.%m.%Y %T");
// Определение является ли время last из текущего дня
bool is_same_day(time_t last);

// Установка системного времени
void set_time(const std::string &hour, const std::string &min, const std::string &sec);
// Установка системной даты
void set_date(const std::string &mday, const std::string &month, const std::string &year);
void set_date_time(const std::string &str, const char *fmt = "%Y-%m-%d %H:%M:%S");


} // namespace utils
extern "C"{
#include <sys/time.h>
}

#include "utility.hpp"
#include "datetime.hpp"

namespace utils{

// Получить текущее локальное время системы в виде строки (формат 210215203031_543)
std::string get_local_datetime(bool with_ms)
{
	char str[32] = {0};
	time_t seconds = time(nullptr);
	struct tm timeinfo;
	struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

	if( !localtime_r(&seconds, &timeinfo) ){
		return "";
	} 

	char buf[16] = {0};
	strftime(buf, sizeof buf, "%y%m%d%H%M%S", &timeinfo);
	if( !with_ms ){
		return std::string(buf);
	} 

	snprintf(str, sizeof str, "%s_%03lu", buf, spec.tv_nsec / 1000000L);
	return std::string(str);
}

// Получить текущее время системы в виде структуры
void get_local_datetime(struct tm *result)
{
	time_t seconds = time(nullptr);

	if(!localtime_r(&seconds, result)){
		throw std::runtime_error(excp_func((std::string)"localtime_r failed: " + strerror(errno)));
	} 
	// Конвертирование в общеупотрябляемый формат
	++result->tm_mon;
	result->tm_year += 1900;
}

// Получить текущее локальное время системы в виде строки (формат 19.03.2021 20:30:31.543)
std::string get_fmt_local_datetime(const char *fmt, bool with_ms, const time_t *sec)
{
	char str[64] = {0};
	time_t seconds = sec ? *sec : time(nullptr);
	struct tm timeinfo;
	struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

	if( !localtime_r(&seconds, &timeinfo) ){
		return "";
	} 

	char buf[32] = {0};
	strftime(buf, sizeof buf, fmt, &timeinfo);
	if( !with_ms ){
		return std::string(buf);
	} 

	snprintf(str, sizeof str, "%s.%03lu", buf, spec.tv_nsec / 1000000L);
	return std::string(str);
}

// Преобразовать строку времени в структуру
struct tm parse_time_str(const std::string &str, const char *fmt)
{
	struct tm res;

	if( !strptime(str.c_str(), fmt, &res) ){
		throw std::runtime_error(excp_func("strptime failed (str: '" + str + "', fmt: '" + fmt + "')"));
	}

	res.tm_year += 1900;
	res.tm_mon += 1;

	return res;
}

// Проверка изменились ли сутки относительно заданного времени
bool is_same_day(time_t last)
{
	time_t now = time(nullptr);
	struct tm *now_tm = gmtime(&now);
	int now_yday = now_tm->tm_yday;
	struct tm *last_tm = gmtime(&last);

	if(now_yday == last_tm->tm_yday){
		return true;
	} 

	return false;
}

// Установка системного времени
void set_time(const std::string &hour, const std::string &min, const std::string &sec)
{
	std::string cmd_ret = exec_piped("date +\"\" --set \"" + hour + ":" + min + ":" + sec + "\" 2>&1 | tr -d '\n'");

	if( !cmd_ret.empty() ){
		throw std::runtime_error(excp_func("failed: " + cmd_ret));
	} 
}

// Установка системной даты
void set_date(const std::string &mday, const std::string &month, const std::string &year)
{
	std::string cmd_ret = exec_piped("date +\"\" -s \"" + year + month + mday + "\" 2>&1 | tr -d '\n'");

	if( !cmd_ret.empty() ){
		throw std::runtime_error(excp_func("failed: " + cmd_ret));
	} 
}

void set_date_time(const std::string &str, const char *fmt)
{
	struct tm tms = parse_time_str(str, fmt);

	char date[16] = {0};
	char time[16] = {0};

	sprintf(date, "%04u%02u%02u", tms.tm_year, tms.tm_mon, tms.tm_mday);
	sprintf(time, "%02u:%02u:%02u", tms.tm_hour, tms.tm_min, tms.tm_sec);

	std::string cmd_ret = exec_piped(std::string("date +\"\" -s \"") + date + " " + time + "\" 2>&1 | tr -d '\n'");

	if( !cmd_ret.empty() ){
		throw std::runtime_error(excp_func("failed: " + cmd_ret));
	} 
}

} // namespace utils
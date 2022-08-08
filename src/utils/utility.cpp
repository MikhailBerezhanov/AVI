#include <memory>
#include <thread>
#include <sstream>
#include <chrono>

extern "C"{
#include <fcntl.h>
}

#include "utility.hpp"

namespace utils{

// Выполнить команду в оболочке и считать вывод через пайп в строку
std::string exec_piped(const std::string &cmd, bool empty_is_error)
{
	std::array<char, 128> buf{};
	std::string result;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

	if( !pipe ){
		throw std::runtime_error(excp_func((std::string)"popen() failed: " + strerror(errno)));
	} 

	while(fgets(buf.data(), buf.size(), pipe.get()) != nullptr){
		result += buf.data();
		std::fill(begin(buf), end(buf), 0);
	}

	if(empty_is_error && result.empty()){
		throw std::runtime_error(excp_func("'" + cmd + "' failed: stdout is empty"));
	} 

	return result;
}

// Выполнить команду в оболочке
bool exec(const std::string &cmd, bool no_throw)
{
	if(cmd.empty()){
		if( no_throw ){
			return false;
		}

		throw std::invalid_argument(excp_func("cmd is empty"));
	}

	int res = system(cmd.c_str());

	if(res){
		if( no_throw ){
			return false;
		}

		throw std::runtime_error(excp_func("cmd '" + cmd + "' failed (" + std::to_string(WEXITSTATUS(res)) + ")"));
	} 

	return true;
}

// Получение идентификатора текущего потока
std::string get_thread_id(void)
{
	std::ostringstream id;
	id << std::this_thread::get_id();
	return id.str();
}

double calc_exec_time(std::function<void(void)> test_func)
{
	using namespace std::chrono;

	auto time_start = high_resolution_clock::now();
	test_func();
	auto time_finish = high_resolution_clock::now();
	auto time_span = duration_cast<duration<double>>(time_finish - time_start);

	return time_span.count();
}

std::string hex_to_string(const unsigned char *in, size_t len)
{
	std::string res;
	char buf[3] = {0};

	while(len){
		snprintf(buf, sizeof(buf), "%02X", *in++);
		res += buf;
		--len;
	}

	return res;
}

} //namespace utils
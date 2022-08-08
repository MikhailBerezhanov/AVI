#pragma once

#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <atomic>

#include "logger.hpp"

class BaseTest
{
public:
	using func = std::function< void(const std::vector<std::string> &) >;

	BaseTest(const std::string &type  = "BaseTest");
	virtual ~BaseTest() = default;

	virtual void init() const {}	// test init phase (called before each run)
	virtual void deinit() const {}	// test deinit phase

	void run(const std::string &name, const std::vector<std::string> &params) const
	{
		auto it = table_.find(name);

		if(it == table_.end()){
			throw std::out_of_range("unsupported test name: " + name);
		}

		// Execute test body
		init();
		it->second.callback(params);

		// Stop program execution
		if(it->second.exit_after_callback){
			deinit();
			exit(0);
		}
		else{
			// Wait forever till signal comes
			wait(-1);
			deinit();
			exit(0);
		}
	}

	static void wait(int sec);

	void show_tests_info() const
	{
		for(const auto &kv : table_){
			printf("%s%s\n", Logging::padding(16, kv.first).c_str(), kv.second.description.c_str());
		}
	}

#ifndef _SHARED_LOG
	static Logging logger;
#endif

	// Information about test
	struct meta{
		meta(func cb, const std::string &desc = "", bool exit = true): 
			callback(cb), description(desc), exit_after_callback(exit) {}

		func callback = nullptr;
		std::string description;
		bool exit_after_callback = true;
	};

protected:
	// name -> {function, exit_flag, description}
	std::unordered_map<std::string, meta> table_;
	std::string test_type_ = "Base";
	static std::string cwd_;

	// when wait() call is interrupted, hook will be executed before exit 
	static std::function<void(void)> interrupt_hook;

private:
	static std::atomic<int> sig_num;
	void signal_handlers_init(std::initializer_list<int> signals);
};





// 
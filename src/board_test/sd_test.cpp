#include "logger.hpp"
#include "utils/fs.hpp"
#include "utils/utility.hpp"
#include "sd_test.hpp"

// ----- SD TESTS -----

void SDcardTest::copy(const std::vector<std::string> &params)
{
	std::string file_name = "/sdcard/test.speed";

	if( !params.empty() ){
		file_name = params[0];
	}

	std::string cmd = "cp " + file_name + " /data/media/sd_copy.test";

	int num = (params.size() > 1) ? std::stoi(params[1]) : 10;

	uint64_t size = utils::get_file_size(file_name);
	logger.msg(MSG_DEBUG, "Copying '%s' of %llu Bytes %d times ...\n", file_name, size, num);

    auto mass_copy = [cmd, num]()
    {
    	for(int i = 0; i < num; ++i){
			// logger.msg(MSG_DEBUG, "Copying %d ..\n", i+1);
			utils::exec(cmd);
			remove("/data/media/sd_copy.test");
		}
    };

    double sec = utils::calc_exec_time(mass_copy);
	logger.msg(MSG_DEBUG, "Took: %lf sec\n", sec);
	logger.msg(MSG_DEBUG, _GREEN "OK" _RESET "\n");
}

void SDcardTest::write(const std::vector<std::string> &params)
{
	std::string dir_name = "/sdcard/write_tests";
	std::string cmd = "mkdir -p " + dir_name;
	std::string checksum_file = cwd_ + "/sd_write_check.md5";

	utils::exec(cmd);

	int err = 0;
	int files_num = params.empty() ? 10 : std::stoi(params[0]);
	int file_size_kb = (params.size() > 1) ? std::stoi(params[1]) : 10;
	file_size_kb *= 1024;

	// remove old check-sums file
	remove(checksum_file.c_str());

	logger.msg(MSG_DEBUG, "Writing %d files of size %d KB to %s ...\n", files_num, file_size_kb, dir_name);

	auto mass_write = [&err, files_num, file_size_kb, &dir_name, &checksum_file](){
		for(int i = 0; i < files_num; ++i){

			std::string file_name = dir_name + "/file" + std::to_string(i) + ".test";

			try{
				char symb = 'A' + i % 58;
				std::string content(file_size_kb, symb);
				utils::write_text_file(file_name, content);

				std::string cmd = "md5sum " + file_name + " >> " + checksum_file;
				utils::exec(cmd);
			}
			catch(const std::exception &e){
				logging_err(logger, "%s failed: %s\n", file_name, e.what());
				++err;
			}
		}
	};

	double sec = utils::calc_exec_time(mass_write);

	logger.msg(MSG_DEBUG, "Took: %lf sec, Errors: %d\n", sec, err);
	if( !err ){
		logger.msg(MSG_DEBUG, _GREEN "OK" _RESET "\n");
	}
}


void SDcardTest::check_written_files(const std::vector<std::string> &params)
{
	std::string cmd = "md5sum -c " + cwd_ + "/sd_write_check.md5";

	utils::exec(cmd);
}
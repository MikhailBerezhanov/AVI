#include <fstream>
#include <cstring>
#include <cerrno>
#include <iostream>

#include "gps_gen.hpp"

using namespace std;


void GPS_Generator::load_route(const std::string &file_path)
{
	errno = 0;
	std::ifstream in{file_path};

	if( !in.is_open() ){
		throw std::runtime_error("could not open '" + file_path + "' - " + strerror(errno));
	}

	std::string tmp;

	this->refresh();

	while(std::getline(in, tmp)){

		try{
			if(tmp.substr(0, 6) == "$GPRMC"){
				RMC_data data = parser_.parse_RMC(tmp);
				cache_.push_back(std::move(data));
			}
		}
		catch(const std::exception &e){
			cerr << "WARN: " << e.what() << endl;
		}
	}

	// printf("GPS_Generator: loaded %zu RMC sentences\n", cache_.size());
}

void GPS_Generator::refresh()
{
	cache_.clear();
	cache_index_ = 0;
	cache_step_ = 1;
}

void GPS_Generator::reset(int index, int increment_step)
{
	cache_index_ = index;
	cache_step_ = increment_step;
}


RMC_data GPS_Generator::get_route_point(int *index)
{
	RMC_data res;

	if(index){
		*index = cache_index_;
	}

	if( cache_.empty() || (cache_index_ >= cache_.size()) || (cache_index_ < 0) ){
		return res;
	}

	res = cache_[cache_index_];
	cache_index_ += cache_step_;

	return res;
}


void GPS_Generator::reverse_route()
{
	// Change points course
	for(auto &elem : cache_){
		elem.course += 180.0;
		if(elem.course > 360.0){
			elem.course -= 360.0;
		}
	}

	cache_step_ *= -1;
	cache_index_ += cache_step_;
}


#ifdef _GPS_GEN_TEST

int main(int argc, char* argv[])
{
	if(argc < 2){
		cout << "No file provided" << endl;
		return 1;
	}

	try{
		const std::string file_path = argv[1];
		GPS_Generator generator;

		generator.load_cache(file_path);
		generator.reset(0, 2);

		for(size_t i = 0; i < 2 * generator.cache_size(); ++i){
			cout << i << " - " << generator.generate() << endl;
		}

	}
	catch(const std::exception &e){
		cerr << e.what() << endl;
		return 1;
	}

	return EXIT_SUCCESS;
}

#endif
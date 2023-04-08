
// Платформо-зависимые модули
#ifndef _HOST_BUILD
#include "drivers/hardware.hpp"
#else
#include <iostream>
#include <clocale>

extern "C"{
#include <ncurses.h>
#include <termios.h>
#include <unistd.h>
}

#include "utils/utility.hpp"
#endif

#include <cstring>
#include <cerrno>
#include <fstream>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

#include "logger.hpp"
#include "utils/fs.hpp"
#include "platform.hpp"


namespace platform
{

// ------------- COMMON PART -------------

static std::mutex lcd_mutex;

// Custom Format parser:
//
// [ 13.07.22 12:12:39 ] vld: 1, lat: 55.750317, long: 37.770050, crs: 147.60, spd: 17.96
//
// lat, long in dec degrees, speed in kmh
class GPS_Track_Loader
{
public:

	void load(const std::string &file_path)
	{
		std::ifstream in_file(file_path);

		if( !in_file.is_open() ){
			log_warn("%s couldn't open '%s': %s\n", excp_method(""), file_path, strerror(errno));
			return;
		}

		std::string line;

		while(std::getline(in_file, line)){

			// Ignore time stamp
			auto pos = line.find_last_of(']');

			if(pos == std::string::npos){
				pos = -2;
			}

			// printf("line: '%s'\n", line.substr(pos + 2).c_str());
			try{
				GPS_data data{line.substr(pos + 2)};
			
				cache_.push_back(std::move(data));

				// printf("added: %s, cache_size: %zu \n", data.to_string().c_str(), cache_.size());
			}
			catch(const std::exception &e){
				log_excp("%s\n", e.what());
			}
		}
	}

	GPS_data get()
	{
		GPS_data res;

		if(cache_.empty()){
			return res;
		}

		if(curr_idx_ >= cache_.size()){
			log_warn("%s track finished. Starting from the beginning\n", excp_method(""));
			curr_idx_ = 0;
		}

		res = cache_.at(curr_idx_);
		// printf("GPS_data::get(%zu) : %s\n", curr_idx_, res.to_string().c_str());
		++curr_idx_;
		return res;
	}

private:
	std::vector<GPS_data> cache_;
	size_t curr_idx_ = 0;
};

static std::unique_ptr<GPS_Generator> gps_nmea_gen = nullptr;
static std::unique_ptr<GPS_Track_Loader> gps_track_gen = nullptr;

static void init_gps_generator(const std::string &file_path)
{
	if(gps_nmea_gen){
		// already exists
		return;
	}

	if(file_path == "dummy_path"){
		return;
	}

	std::string extension = utils::file_extension(file_path);

	if(extension == ".rmc")
	{
		// NMEA RMC is input generator format

		try{
			std::unique_ptr<GPS_Generator> gen{new GPS_Generator};
			gps_nmea_gen = std::move(gen);
			gps_nmea_gen->load_route(file_path);

			log_info("Using RMC GPS generator (%s)\n", file_path);
		}
		catch(const std::exception &e){
			gps_nmea_gen.reset(nullptr);
			log_excp("%s\n", e.what());
		}
	}
	else if(extension == ".track"){

		// track is input generator format

		std::unique_ptr<GPS_Track_Loader> gen{new GPS_Track_Loader};
		gps_track_gen = std::move(gen);
		gps_track_gen->load(file_path);

		log_info("Using Track GPS generator (%s)\n", file_path);
	}
	
}

static GPS_data get_gps_generator_data()
{
	GPS_data res;

	if(gps_nmea_gen){
		int curr_index = 0;

		/*RMC_data*/ res = gps_nmea_gen->get_route_point(&curr_index);

		if((curr_index >= gps_nmea_gen->route_size()) || (curr_index < 0)){
			log_info("RMC GPS generator route finished. Reversing.\n");
			gps_nmea_gen->reverse_route();
		}

		//return res;
	}
	
	else if(gps_track_gen){
		res = gps_track_gen->get();
	}

	return res;
}

// ------------- TARGET PART -------------
#ifndef _HOST_BUILD

void init(int gps_poll_period_ms, const std::string &gps_generator_file)
{
	Hardware::enable_AT();
	Hardware::AT->init();

	if(gps_generator_file.empty()){
		bool increased_rate = gps_poll_period_ms < 1000;
		Hardware::AT->enable_GPS(increased_rate);
		log_info("GPS is enabled with '%s' rate mode\n", increased_rate ? "100ms" : "1sec");
	}
	else{
		init_gps_generator(gps_generator_file);
	}

	Hardware::enable_audio();
	Hardware::audio->set_audio_gain_level(3);

	Hardware::enable_leds();
	Hardware::enable_buttons();

	Hardware::enable_lcd();
	Hardware::lcd_init();
}

bool ready()
{
	// Проверка валидности указателей

	if( !Hardware::AT ){
		log_err("Hardware::AT hasn't been enabled\n");
		return false;
	}

	if( !Hardware::audio ){
		log_err("Hardware::audio hasn't been enabled\n");
		return false;
	}

	if( !Hardware::lcd ){
		log_err("Hardware::lcd hasn't been enabled\n");
		return false;
	}

	if(Hardware::buttons.empty()){
		log_err("Hardware::buttons hasn't been enabled\n");
		return false;
	}

	return true;
}

std::string get_IMEI()
{
	return Hardware::AT->get_IMEI();
}

GPS_data get_GPS_data()
{
	static bool was_valid = false;
	// счетчик невалдиных данных (для защиты от дребезга моргания светодиодом)
	static uint8_t valid_cnt = 3;	

	try{

		if(gps_nmea_gen || gps_track_gen){
			return get_gps_generator_data();
		}
	
		hw::GPSinfo data = Hardware::AT->get_GPSinfo();

		if(data.is_valid()){

			if( !was_valid ){
				// Включаем светодиод - индикацию наличия валидных GPS данных
				platform::set_LED(true);
				was_valid = true;
			}

			valid_cnt = 3;

			std::pair<double, double> lat_lon = data.get_decimal_coordinates();
			return GPS_data(data.get_utc_time(), lat_lon, data.get_speed(), data.get_course(), true);
		}
		else{
			--valid_cnt;

			if( !valid_cnt ){
				// Отключаем светодиод
				platform::set_LED(false);
			}

			was_valid = false;
		}
	}
	catch(const std::exception &e){
		log_excp("%s\n", e.what());
	}

	return GPS_data();
}

void set_LED(bool enable)
{
	// Управление светодиодом HL2
	Hardware::leds[1].set_value(enable);
}

void audio_play(const std::string &mp3_path)
{
	Hardware::audio->play(mp3_path);
}

bool audio_is_playing()
{
	return Hardware::audio->is_playing();
}

void audio_setup_stop_callback(std::function<void(void)> func)
{
	Hardware::audio->finished_callback = func;
}

void audio_set_gain_level(int level)
{
	Hardware::audio->set_audio_gain_level(level);
	log_info("Audio gain is set to: %d\n", level);
}

int audio_get_gain_level()
{
	int res = 0;

	try{
		res = Hardware::audio->get_audio_gain_level();
	}
	catch(const std::exception &e){
		log_excp("%s\n", e.what());
	}

	return res;
}

void audio_stop()
{
	Hardware::audio->stop();
}

void deinit()
{
	Hardware::AT->deinit();
	platform::set_LED(false);
}

// Buttons
void set_button_cb(button_t id, button_callback short_press, button_callback long_press)
{
	if(id >= Hardware::buttons.size()){
		return;
	} 

	Hardware::buttons[id].set_callbacks(short_press, long_press);
}

void set_button_long_press_threshold(button_t id, double sec)
{
	if(id >= Hardware::buttons.size()){
		return;
	} 

	Hardware::buttons[id].set_long_press_threshold(sec);
}

void set_buttons_long_press_threshold(double sec)
{
	for(auto &button : Hardware::buttons){
		button.set_long_press_threshold(sec);
	}
}

// LCD methods
void lcd_backlight(bool on_off)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);

	std::tuple<bool, bool, bool> curr_ctrl = Hardware::lcd->get_control();
	Hardware::lcd->control(on_off, std::get<1>(curr_ctrl), std::get<2>(curr_ctrl));
}

bool lcd_get_backlight_state()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	return Hardware::lcd->get_backlight_state();
}

void lcd_print(const std::string &str, LCD1602::Alignment align)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->print(str, align);
}

void lcd_print(char ch)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->print_char(ch);
}

void lcd_print_with_padding(const std::string &str, char symb)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->print_with_padding(str, symb);
}

uint8_t lcd_get_num_rows()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	return Hardware::lcd->get_num_rows();
}

uint8_t lcd_get_num_cols()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	return Hardware::lcd->get_num_cols();
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->set_cursor(row, col);
}

void lcd_clear()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->clear();
}

void lcd_home()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->return_home();
}

void lcd_add_custom_char(uint8_t location, const uint8_t *charmap)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->user_char_create(location, charmap);
}

void lcd_print_custom_char(uint8_t location)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	Hardware::lcd->user_char_print(location);
}


// ------------- HOST PART -------------
#else

class AudioSimulator
{
public:
	using stop_callback = std::function<void(void)>;

	AudioSimulator(stop_callback func = nullptr): thandle_([this](){ simulation(); }), stop_cb_(func) {}

	~AudioSimulator()
	{
		cancel();
	}

	void cancel()
	{
		if(thandle_.joinable()){
			pthread_cancel( thandle_.native_handle() );
			thandle_.join();
		}
	}

	void set_stop_callback(stop_callback func)
	{
		stop_cb_ = func;
	}

	void simulation()
	{
		for(;;){
			try{
				std::unique_lock<std::mutex> lock(mutex_);
				// Use wait_for to join() after pthread_cancel() works properly
				if( !cv_play_.wait_for(lock, std::chrono::milliseconds(100), [this](){ return wake_up_flag_; }) ){
					continue;
				}

				// After wakeup we own the lock - drop flag
				wake_up_flag_ = false;

				// "Start playing" simulation
				is_playing_.store(true);
				stopped_.store(false);

				// Duration = 20 * 250ms = 5 sec 
				for(int i = 0; i < 40; ++i){

					if(stopped_.load() == true){
						break;
					}

					// printf("LA %d\n", i);
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
				}

				is_playing_.store(false);

				// Force mutex unlock before callback calling
				lock.unlock();

				// Audio "Stopped"
				if(stop_cb_){
					stop_cb_();
				}
			}
			catch(const std::exception &e){
				log_excp("%s\n", e.what());
			}
		}
	}

	bool is_playing() const
	{
		return is_playing_.load();
	}

	void play(const std::string &file_path)
	{
		if(this->is_playing()){
			log_warn("AudioSimulator::play ignoring - already playing\n");
			return;
		}

		{
			std::lock_guard<std::mutex> lock(mutex_);
			wake_up_flag_ = true;
		}
		
		log_msg(MSG_TRACE, "AudioSimulator::play(%s)\n", file_path);
		cv_play_.notify_one();
	}

	void set_gain_level(int lvl) 
	{ 
		gain_level_ = lvl;
		log_msg(MSG_VERBOSE, "AudioSimulator::set_gain_level to %d\n", gain_level_); 
	}
	int get_gain_level() { return gain_level_;}

	void stop()
	{
		stopped_.store(true);
	}

private:
	std::thread thandle_;
	std::mutex mutex_;
	std::condition_variable cv_play_;
	bool wake_up_flag_ = false;
	std::atomic<bool> is_playing_{false};
	std::atomic<bool> stopped_{false};
	stop_callback stop_cb_ = nullptr;
	int gain_level_ = 0;
};


// #ifndef CTRL
// #define CTRL(x) ((x) & 0x1F)
// #endif

class InterfaceSimulator
{
public:

	InterfaceSimulator() = default;

	~InterfaceSimulator(){
		if(thandle_.joinable()){
			pthread_cancel( thandle_.native_handle() );
			thandle_.join();
		}

		this->cleanup_ncurses();

		if( !term_num.empty() ){
			utils::exec("pkill -t " + term_num, true);
		}
	}

	void init()
	{
		open_new_terminal();
		setlocale(LC_CTYPE, "ru_RU.UTF8");

		std::string term_dev = "/dev/" + this->term_num;

		this->term_fp = fopen(term_dev.c_str(), "r+");

		this->screen = newterm(nullptr, this->term_fp, this->term_fp);

		// wait terminal shows it's input to clear it
		std::this_thread::sleep_for(std::chrono::milliseconds(250));

		raw(); 			// disable line buffering (CTRL-Z, CTRL-C are directly passed to the program without generating a signal)
		// nonl();
		noecho();		// switches off echoing
		cbreak();		// disable line buffering (CTRL-Z, CTRL-C works as signals)
		clear();		// clear the screen
		curs_set(0);	// hide cursor
		start_color();	// enable color
		init_pair(1, COLOR_BLACK, COLOR_GREEN);  // define color pair (pair_number, foreground, background)

		int height = window_rows + 2 * window_padding_y; // padding top = 1, padding bot = 1
		int width = window_cols + 2 * window_padding_x;  // padding left = 2, padding right = 2;
		int starty = 2;
		int startx = 10;

		// Interface controls
		attron(A_BOLD);
		mvprintw(7, 10, "^");
		mvprintw(7, 20, "v");
		mvprintw(7, 29, "f");
		attroff(A_BOLD);

		const char *prompt = "ALT+f = long press";
		int len = screen_width - strlen(prompt);
		int prompt_start_x =  len >= 0 ? len / 2 : 0;
		mvprintw(9, prompt_start_x, prompt);

		refresh();

		this->window = create_window(height, width, starty, startx);
		wbkgd(this->window, COLOR_PAIR(1));

		keypad(/*stdscr*/this->window, TRUE);
		home_cursor();

		thandle_ = std::thread([this](){ tfunc(); }); 
	}

	int get_rows_num() const { return window_rows; }
	int get_cols_num() const { return window_cols; }

	void set_backlight(bool on_off)
	{
		this->backlight = on_off;

		if( !on_off ){
			// werase(this->window);
			wbkgd(this->window, COLOR_PAIR(0));
			wrefresh(this->window);
		}
		else{
			wbkgd(this->window, COLOR_PAIR(1));
			wrefresh(this->window);
		}
	}

	bool get_backlight() const { return this->backlight; }

	void print(const std::string &str)
	{
		wprintw(this->window, "%s", str.c_str());
		wrefresh(this->window);
	}

	void print(char ch)
	{
		wprintw(this->window, "%c", ch);
		wrefresh(this->window);
	}

	void print_with_padding(const std::string &str, char ch)
	{
		std::string padding(window_cols - number_of_symbols(str.c_str()), ch);

		this->print(str + padding);
	}

	void print_custom_char(size_t id)
	{
		if(id >= custom_chars_table.size()){
			return;
		}

		this->print(custom_chars_table.at(id));
	}

	void home_cursor()
	{
		wmove(this->window, window_padding_y, window_padding_x);
	}

	void move_cursor(int y, int x)
	{
		wmove(this->window, y + window_padding_y, x + window_padding_x);
		// wrefresh(this->window);
	}

	void clear()
	{
		this->home_cursor();
		this->print( std::string(window_cols, ' ') );
		this->move_cursor(1, 0);
		this->print( std::string(window_cols, ' ') );
	}

	// matches button_id with < short_press_cb, long_press_cb >
	std::unordered_map<button_t, std::pair<button_callback, button_callback>> cbs = {
		{S1, {nullptr, nullptr}},
		{S2, {nullptr, nullptr}},
		{S3, {nullptr, nullptr}},
	};

private:
	const int window_rows = 2;
	const int window_cols = 16;
	const int window_padding_y = 1;
	const int window_padding_x = 2;

	const int screen_width = 40;
	const int screen_height = 10;

	// simple simulation of suctom charactes
	const std::array<char, 3> custom_chars_table = {
		';',	// 0 - arrows
		'^',	// 1 - up_arrow
		'v',	// 2 - down_arrow
	};

	bool backlight = false;

	std::thread thandle_;
	std::string term_num;
	FILE *term_fp = nullptr;
	SCREEN *screen = nullptr;
	WINDOW *window = nullptr;

	void execute_cb(button_t id, bool short_press = true){
		
		if(short_press){
			if(cbs.at(id).first){
				cbs.at(id).first();
			}
		}
		else{
			if(cbs.at(id).second){
				cbs.at(id).second();
			}
		}
	}

	void cleanup_ncurses()
	{
		if(this->window){
			destroy_window(this->window);
		}
		
		if(this->screen){
			delscreen(this->screen);
		}

		if(this->term_fp){
			fclose(this->term_fp);
		}

		endwin();	// End curses mode
	}

	void open_new_terminal()
	{
		utils::exec("gnome-terminal --geometry=" + std::to_string(screen_width) + "x" + std::to_string(screen_height));
		std::this_thread::sleep_for(std::chrono::milliseconds(250));

		std::string num = utils::exec_piped("ls /dev/pts/ | tail -2 | head -1 | tr -d '\n'");
		this->term_num = "pts/" + num;
	}

	WINDOW* create_window(int height, int width, int starty, int startx)
	{	
		WINDOW *win = newwin(height, width, starty, startx);

		// 0, 0 gives default characters for
		// the vertical and horizontal lines
		box(win, 0, 0);

		// Show that box 
		wrefresh(win);

		return win;
	}

	void destroy_window(WINDOW *win)
	{	
		/* box(local_win, ' ', ' '); : This won't produce the desired
		 * result of erasing the window. It will leave it's four corners 
		 * and so an ugly remnant of window. 
		 */
		wborder(win, ' ', ' ', ' ',' ',' ',' ',' ',' ');

		wrefresh(win);
		delwin(win);
	}

	void tfunc()
	{
		int prev_ch = -1;

		for(;;){

			int ch = wgetch(this->window);			/* If raw() hadn't been called
						 * we have to press enter before it
						 * gets to the program 		*/
			switch(ch){

				case -1:
					return;

				case KEY_UP:
					if(prev_ch == 27){
						// ALT + KEY_UP
						this->execute_cb(S3, false);
					}
					else{
						this->execute_cb(S3);
					}

					break;

				case KEY_DOWN:
					if(prev_ch == 27){
						// ALT + KEY_DOWN
						this->execute_cb(S2, false);
					}
					else{
						this->execute_cb(S2);
					}

					
					break;

				case 523:
					printf("ALT+KEY_DOWN\n");
					break;

				case 564:
					printf("ALT+KEY_UP\n");
					break;

				// case 10:
				case 102: // f
					// ALT + ENTER = long press
					if(prev_ch == 27){
						// printf("ALT+ENTER\n");
						this->execute_cb(S1, false);
					}
					else{
						this->execute_cb(S1);
					}

					break;

				default:
					// printf("pressed code: %d\n", ch);
					break;
			}
			
			prev_ch = ch;
			// home_cursor();
		}
	}
};

// Simulators
static AudioSimulator audio_sim;
static InterfaceSimulator iface_sim;

void init(int gps_poll_period_ms, const std::string &gps_generator_file)
{
	if( !gps_generator_file.empty() ){
		init_gps_generator(gps_generator_file);
	}

	try{
		iface_sim.init();
	}
	catch(const std::exception &e){
		log_excp("%s\n", e.what());
	}
}

bool ready() { return true; }

std::string get_IMEI()
{
	return "mahachkala";
}

GPS_data get_GPS_data()
{
	if(gps_nmea_gen || gps_track_gen){
		return get_gps_generator_data();
	}
	
	return GPS_data("", {0.123456, 9.876543}, 0.1, 0.0, false);
}

void set_LED(bool enable)
{
	log_msg(MSG_VERBOSE, _YELLOW "LED %s" _RESET "\n", enable ? "On" : "Off"); 
}

void audio_play(const std::string &mp3)
{
	audio_sim.play(mp3);
}

bool audio_is_playing()
{
	return audio_sim.is_playing();
}

void audio_setup_stop_callback(std::function<void(void)> func)
{
	audio_sim.set_stop_callback(func);
}

void audio_set_gain_level(int value)
{
	audio_sim.set_gain_level(value);
}

int audio_get_gain_level()
{
	return audio_sim.get_gain_level();
}

void audio_stop()
{
	audio_sim.stop();
}

// Buttons
void set_button_cb(button_t id, button_callback short_press, button_callback long_press)
{
	iface_sim.cbs[id].first = short_press;
	iface_sim.cbs[id].second = long_press;
}

void set_button_long_press_threshold(button_t id, double sec){}

void set_buttons_long_press_threshold(double sec){}

// LCD methods
void lcd_backlight(bool on_off)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.set_backlight(on_off);
}

bool lcd_get_backlight_state()
{ 
	std::lock_guard<std::mutex> lck(lcd_mutex);
	return iface_sim.get_backlight(); 
}

void lcd_print(const std::string &str, LCD1602::Alignment align)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.print(str);
}

void lcd_print(char ch)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.print(ch);
}

void lcd_print_with_padding(const std::string &str, char symb) 
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.print_with_padding(str, symb);
}

uint8_t lcd_get_num_rows()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	return iface_sim.get_rows_num();
}

uint8_t lcd_get_num_cols()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	return iface_sim.get_cols_num();
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.move_cursor(row, col);
}

void lcd_clear()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.clear();
}

void lcd_home()
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.home_cursor();
}

void lcd_add_custom_char(uint8_t location, const uint8_t *charmap)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	// dummy
}

void lcd_print_custom_char(uint8_t location)
{
	std::lock_guard<std::mutex> lck(lcd_mutex);
	iface_sim.print_custom_char(location);
}


void deinit()
{
	platform::set_LED(false);
}

#endif // #ifndef _HOST_BUILD

} // namespace platform
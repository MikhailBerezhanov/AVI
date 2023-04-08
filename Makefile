
CFLAGS = -O2 \
	-DFEATURE_OPENSSL_SDK

CXXFLAGS = -O2 -std=c++11

LDFLAGS = -Wl,-rpath,/data/lib

VERSION = "1.0"

#-DFEATURE_JSON_SDK    \
#-DFEATURE_CURL_SDK    \
#-DFEATURE_MQTT_SDK
DEFINES += -DAPP_VERSION=\"$(VERSION)\"
DEFINES += -D_SHARED_LOG

MAIN_DIR = .

BIN_NAME = avi.out
TARGET_PATH = /data/avi/avi

BIN_DIR = $(MAIN_DIR)/output
OBJ_DIR = $(MAIN_DIR)/obj
TEST_DIR = $(MAIN_DIR)/tests

SRC_DIR = $(MAIN_DIR)/src
SDK_DIR = $(MAIN_DIR)/../sim_open_sdk
API_DIR = $(SRC_DIR)/simcom_api

SRCS_DIRS = $(SRC_DIR)
SRCS_DIRS += $(SRC_DIR)/drivers
SRCS_DIRS += $(SRC_DIR)/utils
SRCS_DIRS += $(SRC_DIR)/gps_simulator
SRCS_DIRS += $(SRC_DIR)/board_test
SRCS_DIRS += $(SRC_DIR)/lc_client/src/
SRCS_DIRS += $(SRC_DIR)/lc_client/src/proto
SRCS_DIRS += $(SRC_DIR)/lc_client/src/logger/cpp_src
SRCS_DIRS += $(SRC_DIR)/amqp_test
SRCS_DIRS += $(API_DIR)
SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/common
SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/qmi-framework
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/data
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/dsi
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/dsutils
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/gps
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/qmi
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/linux/spi
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/mqtt
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/openssl
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/curl
# SRCS_DIRS += $(SDK_DIR)/simcom-demo/sdk-includes/json-c
# SRCS_DIRS += $(LIB_PATH)/usr/include

INCLUDE_PREFIX = -I
INCLUDES = $(addprefix $(INCLUDE_PREFIX), $(SRCS_DIRS))

LINK_PREFIX = -L
LINKS = $(addprefix $(LINK_PREFIX), $(SDK_DIR)/simcom-demo/libs)

# Path for templates to search source files in directories list
VPATH = $(SRCS_DIRS)

# Search source files in specified directories 
#search_wildcards = $(addsuffix /*.cpp, $(SRCS_DIRS))
#OBJS = $(addprefix $(OBJ_DIR)/, $(notdir $(patsubst %.cpp, %.o, $(wildcard $(search_wildcards)))))

# Objects to build
OBJS = 	$(OBJ_DIR)/logger.o 		\
		$(OBJ_DIR)/i2c.o 			\
		$(OBJ_DIR)/nau8810.o 		\
		$(OBJ_DIR)/at_cmd.o 		\
		$(OBJ_DIR)/audio.o 			\
		$(OBJ_DIR)/gpio.o 			\
		$(OBJ_DIR)/lcd1602.o 		\
		$(OBJ_DIR)/uart.o 			\
		$(OBJ_DIR)/hardware.o 		\
		$(OBJ_DIR)/announ.o 		\
		$(OBJ_DIR)/lc_utils.o 		\
		$(OBJ_DIR)/lc_protocol.o 	\
		$(OBJ_DIR)/lc_sys_ev.o 		\
		$(OBJ_DIR)/lc.pb.o  		\
		$(OBJ_DIR)/log.pb.o  		\
		$(OBJ_DIR)/dev_status.pb.o  \
		$(OBJ_DIR)/push.pb.o  		\
		$(OBJ_DIR)/lc_trans.o 		\
		$(OBJ_DIR)/lc_client.o 		\
		$(OBJ_DIR)/utility.o 		\
		$(OBJ_DIR)/timer.o 			\
		$(OBJ_DIR)/iconvlite.o 		\
		$(OBJ_DIR)/fs.o 			\
		$(OBJ_DIR)/crypto.o 		\
		$(OBJ_DIR)/datetime.o 		\
		$(OBJ_DIR)/nmea_parser.o 	\
		$(OBJ_DIR)/gps_gen.o 		\
		$(OBJ_DIR)/bg_task.o 		\
		$(OBJ_DIR)/platform.o 		\
		$(OBJ_DIR)/app_db.o 		\
		$(OBJ_DIR)/app_cfg.o 		\
		$(OBJ_DIR)/app_lc.o 		\
		$(OBJ_DIR)/app_menu.o 		\
		$(OBJ_DIR)/app.o 			\
		$(OBJ_DIR)/main.o 			\
# 	$(OBJ_DIR)/LedControl.o 	\
# 	$(OBJ_DIR)/I2C.o 		\

LIBS = -pthread -lm -lsdk -lcurl -lsqlite3 -lconfig -lrt -lcrypto -luuid -lprotobuf


# LIBS = 	$(LIB_PATH)/usr/lib/libdsi_netctrl.so 	\
#         $(LIB_PATH)/usr/lib/libdsutils.so 	\
#         $(LIB_PATH)/usr/lib/libqmiservices.so 	\
#         $(LIB_PATH)/usr/lib/libqmi_cci.so 	\
#         $(LIB_PATH)/usr/lib/libqmi_common_so.so \
#         $(LIB_PATH)/usr/lib/libqmi.so 		\
# 	$(LIB_PATH)/lib/libpthread.so.0		\
# 	$(LIB_PATH)/usr/lib/libqdi.so		\
# 	$(LIB_PATH)/usr/lib/librt.so		\
# 	$(LIB_PATH)/usr/lib/libssl.so		\
# 	$(LIB_PATH)/lib/libcrypto.so		\
# 	$(LIB_PATH)/usr/lib/libdl.so		\
#         $(LIB_PATH)/usr/lib/libloc_hal_test_shim_extended.so \
#         $(LIB_PATH)/usr/lib/libloc_xtra.so 	\
#         $(LIB_PATH)/usr/lib/libcurl.so 		\
#         $(LIB_PATH)/usr/lib/libsqlite3.so 	\
#         $(LIB_PATH)/usr/lib/libconfig.so 	\
#        $(LIB_PATH)/usr/lib/libprotobuf.so 	\
#	 $(LIB_PATH)/usr/lib/libjson-c.so    \
#	 $(LIB_PATH)/usr/lib/libmosquitto.so \
	   


.PHONY: all clean env install info

# Main target
all:prep info bin 

info:
	@echo "Using compiler: $(notdir $(CXX))"

# Prepare directories for output
prep:
	@if test ! -d $(BIN_DIR); then mkdir $(BIN_DIR); fi
	@if test ! -d $(OBJ_DIR); then mkdir $(OBJ_DIR); fi
	@mkdir -p $(TEST_DIR)

install:
	@adb push $(BIN_DIR)/$(BIN_NAME) $(TARGET_PATH)
	@adb shell chmod +x $(TARGET_PATH)


# Start compiler for different files
# $@, $<, $^  -  Automatic Variables
# $@	-	the name of the target being generated
# $<	-	the first prerequisite (usually a source file)
# $^	-	the names of all the prerequisites, with spaces between them
# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html#Automatic-Variables

# c sources
$(OBJ_DIR)/%.o : %.c
	@echo Building OBJECT $(@) from C SOURCE $<
	@$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

# cpp sources
$(OBJ_DIR)/%.o : %.cpp
	@echo "\033[32m>\033[0m CXX compile: \t" $<" >>> "$@
	@$(CXX) -c $(CXXFLAGS) $(INCLUDES) $(DEFINES) -o $@ $<

# Start linker
bin:$(OBJS)
	@echo Linking bin file $(BIN_NAME)
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(BIN_DIR)/$(BIN_NAME) $^ $(LIBS) 
	@echo "\033[32mBuilding finished [$(shell date +"%T")]. Version: \033[1m$(VERSION)\033[0m"


clean:
	@rm -fr $(OBJ_DIR) $(BIN_DIR) 
	
clean-proto:
	@cd $(SRC_DIR)/lc_client && make clean-proto

clean-all: clean clean-proto

# LC-client Proto files build
proto-src:
	@echo "Using proto-comliper: $(PROTOC)"
	@cd $(SRC_DIR)/lc_client/proto && make PROTOC=$(PROTOC)

proto-host: PROTOC=protoc
proto-host: proto-src

# Test-util building 
# lc_utils.o lc_protocol.o lc_sys_ev.o lc.pb.o log.pb.o log_result.pb.o lc_trans.o lc_client.o
test-util-bin: BIN_NAME = test_avi
test-util-bin: 	$(addprefix $(OBJ_DIR)/, logger.o utility.o fs.o i2c.o nau8810.o lcd1602.o fs.o audio.o gpio.o at_cmd.o uart.o \
hardware.o tests.o at_test.o nau_test.o lcd_test.o audio_test.o sd_test.o gpio_test.o uart_test.o test_util.o )
	@echo "\033[32m>\033[0m linking test-util: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(BIN_DIR)/$(BIN_NAME) $^ -lm -lsdk -pthread
test-util: prep info test-util-bin

install-test-util: BIN_NAME = test_avi
install-test-util: TARGET_PATH = /data/autoinformer/$(BIN_NAME)
install-test-util: install


#at_cmd-test-bin: TEST_DIR = $(MAIN_DIR)/tests/AT
at_cmd-test-bin: DEFINES += -D_AT_CMD_TEST
at_cmd-test-bin: $(addprefix $(OBJ_DIR)/, at_cmd.o)
	@echo "\033[32m>\033[0m linking test: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(BIN_DIR)/$(BIN_NAME) $^ $(LIBS) 
at_cmd-test: BIN_NAME = AT
at_cmd-test: prep info at_cmd-test-bin


cfg-test-bin: BIN_NAME = cfg.test
cfg-test-bin: DEFINES += -D_APP_CFG_TEST -D_SHARED_LOG	
cfg-test-bin: $(addprefix $(OBJ_DIR)/, logger.o app_cfg.o fs.o)
	@echo "\033[32m>\033[0m linking test: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(TEST_DIR)/$(BIN_NAME) $^ -lconfig -lcrypto
cfg-test: TEST_DIR = $(MAIN_DIR)/tests/cfg
cfg-test: prep info cfg-test-bin


db-test-bin: BIN_NAME = db.test
db-test-bin: DEFINES += -D_APP_DB_TEST -D_SHARED_LOG	
db-test-bin: $(addprefix $(OBJ_DIR)/, logger.o app_db.o)
	@echo "\033[32m>\033[0m linking test: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(TEST_DIR)/$(BIN_NAME) $^ -lsqlite3
db-test: TEST_DIR = $(MAIN_DIR)/tests/db
db-test: prep info db-test-bin


gpsgen-test-bin: BIN_NAME = gpsgen.test
gpsgen-test-bin: DEFINES += -D_GPS_GEN_TEST
gpsgen-test-bin: $(addprefix $(OBJ_DIR)/, nmea_parser.o gps_gen.o)
	@echo "\033[32m>\033[0m linking test: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(TEST_DIR)/$(BIN_NAME) $^ 
gpsgen-test: TEST_DIR = $(SRC_DIR)/gps_simulator
gpsgen-test: prep info gpsgen-test-bin


menu-test-bin: BIN_NAME = menu.test
menu-test-bin: DEFINES += -D_APP_MENU_TEST
menu-test-bin: $(addprefix $(OBJ_DIR)/, \
i2c.o nau8810.o at_cmd.o audio.o gpio.o lcd1602.o uart.o hardware.o nmea_parser.o gps_gen.o \
logger.o platform.o timer.o app_menu.o)
	@echo "\033[32m>\033[0m linking test: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(TEST_DIR)/$(BIN_NAME) $^ -pthread -lrt -lsdk
menu-test: TEST_DIR = $(MAIN_DIR)/tests/menu
menu-test: prep info menu-test-bin


app-test-bin: BIN_NAME = avi.test
app-test-bin: CXXFLAGS = -std=c++11 -g 
app-test-bin: DEFINES += -D_APP_TEST -D_SHARED_LOG -D_HOST_BUILD -DMAKE_VALGRIND_HAPPY
app-test-bin: $(addprefix $(OBJ_DIR)/, logger.o utility.o fs.o datetime.o crypto.o iconvlite.o timer.o bg_task.o  \
lc_trans.o lc_sys_ev.o lc.pb.o log.pb.o push.pb.o dev_status.pb.o lc_utils.o lc_protocol.o lc_client.o \
i2c.o lcd1602.o platform.o nmea_parser.o gps_gen.o announ.o app_db.o app_cfg.o app_lc.o app_menu.o app.o main.o)
	@echo "\033[32m>\033[0m linking test: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(TEST_DIR)/$(BIN_NAME) $^ -pthread -lsqlite3 -lconfig -lcurl -lcrypto -lprotobuf -luuid -lrt -lncursesw
app-test: TEST_DIR = $(MAIN_DIR)/tests/avi
app-test: prep info app-test-bin

# AMQP (RabbitMQ client) tests
amqp-test-bin: BIN_NAME = amqp_send_recv.test
amqp-test-bin: DEFINES += -D_SHARED_LOG
amqp-test-bin: $(addprefix $(OBJ_DIR)/, logger.o my_handler.o send_recv.o)
	@echo "\033[32m>\033[0m linking test: $(BIN_NAME)"
	@$(CXX) $(LINKS) $(LDFLAGS) -o $(TEST_DIR)/$(BIN_NAME) $^ -pthread -lamqpcpp -ldl -lssl
amqp-test: TEST_DIR = $(MAIN_DIR)/tests/amqp
amqp-test: prep info amqp-test-bin
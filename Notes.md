
# Connection to Ubuntu18.04:
sudo apt install adb
sudo cp ./99-simcom-adb.rules /etc/udev/rules.d/
sudo reboot
adb devices


# Virtual FS (used in AT commands)   |	Real FS

(media storage)			E:/				/data/media
(external storage)		D:/				/sdcard
(cache storage)			F:/				/cache
(local storage)			C:/				???


# --- Main inet iface ---

ifconfig rmnet_data0


# --- Application autostart ---

sudo cp start_autoinformer ../sim_rootfs/etc/init.d/

cd ../sim_rootfs/etc/rc5.d/

sudo ln -sf ../init.d/start_autoinformer ./S99start_autoinformer

cd sim_open_sdk/ && make rootfs # rebuild filesystem

В sim_open_sdk/output должен появиться system.img

# Прошивка модуля новыми образами
# Образ можно загрузить в модуль при помощи ADB FastBoot:

adb reboot bootloader
fastboot flash system B:\system.img
fastboot reboot

После этого нужно будет снова открыть ADB порт АТ-командой

AT+CUSBADB=1
AT+CRESET

# --- Change userspace partition ---

nano sim_open_sdk/tools/ubinize_system_userdata.cfg

edit [cache_volume] vol_size=..MiB (45 by default)

поставить 25. оставшееся используется в /data

# --- Edit userfs ---

mkdir ../sim_usrfs/autoinformer

cp ainf.out ../sim_usrfs/autoinformer/

cd sim_open_sdk/ && make rootfs # rebuild filesystem


# --- GPIO CONTROL ---

Информация о GPIO
cat /sys/kernel/debug/gpio

Настройки дравйеров регуляторов, использующие GPIO
sim_open_sdk/sim_kernel/arch/arm/boot/dts/qcom/mdm9607-regulator.dtsi

# NETLIGHT LED (pin51, GPIO18)
echo 18 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio18/direction
echo 1 > /sys/class/gpio/gpio18/value
echo 0 > /sys/class/gpio/gpio18/value

# --- AT commands ---

# вывод параметров модуля (версия прошивки, конфигурация, IMEI)
AT+SIMCOMATI
AT+CQCNV
AT+SIMEI?
# установка IMEI (написано на верхней крышке модуля)
AT+SIMEI=...

# Открыть доступ к ADB порту (по умолчанию закрыт)
AT+CUSBADB=1
AT+CRESET

# Проверка доступности SIM-карты
AT+CPIN?
+CPIN: READY

# Воспроизведение аудио из бинарной последовательности
AT+CTTS=1,"6B228FCE4F7F75288BED97F3540862107CFB7EDF"

# Записаь аудио в формате wav
AT+CREC=1,”e:/rec.wav”
AT+CREC=0

# Воспроизведение аудио
AT+CCMXPLAY="E:/test2.mp3",0,0
AT+CCMXSTOP

# Настройка GPS
AT+CGPS=1
OK

AT+CGPS?
+CGPS: 1,1

AT+CGPSINFO                                                                     
+CGPSINFO: ,,,,,,,,

# Вывод GPS координат в формате NMEA-0183 RMC кадые 10 сек
AT+CGPSINFOCFG=10,2
$GPRMC,,V,,,,,,,,,,N*53

# Прекратить вывод координат
AT+CGPSINFOCFG=0,2


# Получение значений АЦП (тип 1 - сырые значения, 2 - в мВ)
AT+CADC=1
AT+CADC2=2

# Получение текущего значения напряжения питания
AT+CBC

# Текущая температура модуля
AT+CPMUTEMP

# Уровень сигнала сотовой связи <rssi> <ber>
AT+CSQ

RSSI: 2...30 – - 109... - 53 dBm

Отличные показатели: CINR от 30 и выше / RSSI до -65
Хорошие показатели: CINR от 20 до 30 / RSSI от -65 до -75

# Просмотр UART 
```bash
stty -F /dev/ttyHS1 115200 raw -echo -iexten ignbrk
cat /dev/ttyHS1 | hexdump -C &
printf "Hello SIMCOM\x0A" > /dev/ttyHS1
````


# Конфигурация при сборке различных библиотек

## libconfig, libconfig++

```bash
source ../sim_open_sdk/sim_crosscompile/sim-crosscompile-env-init

cd ./external/libconfig

./configure --host=arm-oe-linux-gnueabi --with-sysroot=/home/mik/projects/sim_open_sdk/sim_crosscompile/sysroots/mdm9607-perf/ CPP='arm-oe-linux-gnueabi-gcc -E  -march=armv7-a -mfloat-abi=softfp -mfpu=neon --sysroot=/home/mik/projects/sim_open_sdk/sim_crosscompile/sysroots/mdm9607-perf/'

make
```

Собранная библиотека в ./lib/.libs/


## libprotobuf

### Сборка protoc для хост-машины 

```bash
cd ~/projects/ainf/autoinformer/external/protobuf-3.20.0

./autogen.sh
./configure --prefix=/home/mik/projects/ainf/auoinformer/external/output/host/proto
make 
make check
make install

export LD_LIBRARY_PATH=/home/mik/projects/ainf/autoinformer/external/protobuf-3.20.0/src/.libs/
sudo ldconfig
./src/.libs/protoc --version
libprotoc 3.20.0
````

### Компиляция libprotobuf 3.20

```bash
source ../sim_open_sdk/sim_crosscompile/sim-crosscompile-env-init

export CXXFLAGS=$CXXFLAGS' -std=c++11'
# ??
export CFLAGS=$CFLAGS' -fPIE -fPIC'
export LDFLAGS=$LDFLAGS' -pie -llog'


./configure --prefix=/home/mik/projects/ainf/autoinformer/external/output/target/proto --with-protoc=/home/mik/projects/ainf/autoinformer/external/output/host/proto/protoc --host=arm-oe-linux-gnueabi --with-sysroot=/home/mik/projects/ainf/sim_open_sdk/sim_crosscompile/sysroots/mdm9607-perf/ CPP='arm-oe-linux-gnueabi-gcc -E  -march=armv7-a -mfloat-abi=softfp -mfpu=neon --sysroot=/home/mik/projects/ainf/sim_open_sdk/sim_crosscompile/sysroots/mdm9607-perf/'

````

_Для прохождения тестов на хост-машине необходимо 8 GB RAM_

### Фикс линковки (https://github.com/protocolbuffers/protobuf/issues/5144)

I'm seeing this issue as well on some machines, when building using autotools. It's mentioned at #5107 that this is an issue with the GOLD linker, but I can reproduce it on GNU LD 2.30 (and GCC 7.3.0) as well, although it works fine on another machine also with GNU LD 2.30 and GCC 7.5.0.

It seems that the problem is that the scc_info_* and descriptor_table_* symbols are not extern "C++", and therefore on some linkers do not match the *google* wildcard in the version script. If I modify the src/libprotoc.map, src/libprotobuf.map and src/libprotobuf-lite.map files to read

```
{
  global:
    extern "C++" {
      *google*;
    };
    scc_info_*;
    descriptor_table_*;

  local:
    *;
};
```

then it builds correctly using autotools. Possible a broader *google*; pattern outside of the extern "C++" is a better solution in the long run, but this seems to be the minimal match needed on Protobuf 3.10.0 at least.

It looks like building with CMake is not really a fix, #6113 says that there's a bug in the CMake build leading it to not use the version scripts, which hides the problem (because all symbols are now public) but doesn't solve it properly. It may be good to reopen this issue because of this. 

## libamqpcpp

```bash
source ../sim_open_sdk/sim_crosscompile/sim-crosscompile-env-init

cd ./external/AMQP-CPP
```

Патч для src/Makefile

```
CPP             = $(CXX)
RM              = rm -f
CPPFLAGS      = ${CXXFLAGS}
CPPFLAGS      += -Wall -c -I../include -std=c++11 -MD -Wno-class-conversion
LD              = $(CXX)
LD_FLAGS      = $(LDFLAGS)
LD_FLAGS      += -Wall -shared
```

```bash
make release

ls src/libamqpcpp*
````


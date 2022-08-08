# __Автоинформатор 1.0__
### (На основе SIM7600E-H OpenLinux SDK)

Основная функция устройства - автоматическое воспроизведение заданных аудио-файлов в зависимости от текущего местоположения. Обновление карты зон и аудио-фрагментов осуществляется онлайн по мобильной сети. Для контроля и управления устройством используется LCD-дисплей с набором кнопок.

Помимо целевой платформы (модуль SIM7600E-H) приложение можно собрать и под хост систему Ubuntu18.04 - в таком случае будут симулироваться следующие компоненты (см. src/platform.cpp):

* поток GPS NMEA данных
* интерфейс LCD-дисплея и кнопок 
* аудио воспроизведение

## Вспомогательное ПО

Драйверы:  

* [для всех версий Windows][win_driver]
* [для Linux][linux_driver]

Программа для обновления прошивки модуля:

* [SIM_QDL][sim_qdl]

Программа конфигурации и восстановления модуля:

* [SIM_QPST][sim_qpst]

Инструментарий для разработки приложений:

* [SIM_OPEN_SDK][open_sdk]

## Подключение модуля к ПК

Модуль подключается к ПК по USB и отображается как несколько USB-устройств:

* `SimTech HS-USB AT Port` - порт АТ-команд
* `SimTech HS-USB Audio`   - порт аудио-данных
* `SimTech HS-USB Diagnostics` - порт для обновления прошивки
* `SimTech HS-USB NMEA` - порт GPS данных

При сбросе прошивки модуля (перемычка BOOT) модуль станет отображаться как

* `SimTech HS-USB QDLoader` - порт конфигурации (доступен только из QPST)

---

Для диагностики и отладки модуль можно подключить к ОС __Windows 7/10__. Для этого нужно установить драйвер из `SIM7000_SIM7100_SIM7500_SIM7600_SIM7800 Series Windows USB driver_V1.02.rar`. Модуль поддерживает технологию __Android Debug Bridge (ADB)__. Для ее активации требуется: 

* установить `platform-tools_r30.0.5-windows.zip` в корень диска `C:\platform-tools`  
* через COM-порт АТ-команд послать команды разрешения ADB и перезагрузки:  
 `AT+CUSBADB=1`  
 `AT+CRESET`  
 
 После этого в диспетчере устройств должно появиться Android-устройство. Теперь из командной строки можно подключаться к консоли модуля:  
 
```sh
# старт adb (должно обноружиться устройство)
adb devices
# List of devices attached
# * daemon not running; starting now at tcp:5037
# * daemon started successfully
# 0123456789ABCDEF    device

# подключение к оболочке модуля
adb shell
```

---

Для подключения к __Ubuntu18.04__ или __Debian9/10__:  

```sh
sudo apt install adb
# добавление правила автоматического обнаружения модуля
sudo cp ./99-simcom-adb.rules /etc/udev/rules.d/
sudo reboot
adb devices
adb shell
```

Модуль определяется как /dev/ttyUSB*
```sh
$ lsusb
Bus 001 Device 002: ID 1e0e:9001 Qualcomm / Option 
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 002 Device 002: ID 80ee:0021 VirtualBox USB Tablet
Bus 002 Device 001: ID 1d6b:0001 Linux Foundation 1.1 root hub

$ dmesg | grep ttyUSB*
[ 2595.050771] usb 1-1: GSM modem (1-port) converter now attached to ttyUSB0
[ 2595.051810] usb 1-1: GSM modem (1-port) converter now attached to ttyUSB1
[ 2595.054387] usb 1-1: GSM modem (1-port) converter now attached to ttyUSB2
[ 2595.057850] usb 1-1: GSM modem (1-port) converter now attached to ttyUSB3
[ 2595.061169] usb 1-1: GSM modem (1-port) converter now attached to ttyUSB4
```

## Диагностика и управление модулем

Файловая система модуля: 

| Назначение | Виртуальная точка монтирования | Реальная точка монтирования |
|--|--|--|
|media storage   | `E:/` | `/data/media`|
|external storage| `D:/` |   `/sdcard`  |
|cache storage   | `F:/` |   `/cache`   |
|local storage   | `C:/` |   `/data` (?)|

_Виртуальные точки монтирования используются в АТ-командах_

Состояние сетевого интерфейса:
```sh
ifconfig rmnet_data0
```

Управление UART:
```sh
stty -F /dev/ttyHS1 115200 raw -echo -iexten ignbrk
cat /dev/ttyHS1 | hexdump -C &
printf "Hello SIMCOM\x0A" > /dev/ttyHS1
```

Информация о задействованных системой GPIO:
```sh
cat /sys/kernel/debug/gpio
```

Настройка пользовательских GPIO (_некоторые экспортированы по-умолчанию_)
```sh
# Hardware pin51 (OS: gpio18)
echo 18 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio18/direction
echo 1 > /sys/class/gpio/gpio18/value
echo 0 > /sys/class/gpio/gpio18/value
```

Полезные АТ-команды:
```sh
# активация поддержки ADB
AT+CUSBADB=1
# мягкая перезагрузка
AT+CRESET
# вывод параметров модуля (версия прошивки, конфигурация, IMEI)
AT+SIMCOMATI
AT+GMR
AT+CQCNV
AT+SIMEI?
# установка IMEI (написано на верхней крышке модуля)
AT+SIMEI=...
# Получение текущего значения напряжения питания
AT+CBC
# Текущая температура модуля
AT+CPMUTEMP
# Получение значений АЦП1 (тип 1 - сырые значения, 2 - в мВ)
AT+CADC=2
# Получение значений АЦП2
AT+CADC2=2
# Выключить модуль
AT+CPOF

# Проверка доступности SIM-карты
AT+CPIN?
# +CPIN: READY
# Проверка Уровеня сигнала сотовой связи <rssi> <ber>
AT+CSQ
# RSSI: 2...30 – - 109... - 53 dBm
# Отличные показатели: CINR от 30 и выше / RSSI до -65
# Хорошие показатели: CINR от 20 до 30 / RSSI от -65 до -75

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
# OK
AT+CGPS?
# +CGPS: 1,1
AT+CGPSINFO                                                                     
# +CGPSINFO: ,,,,,,,,
# Вывод GPS координат в формате NMEA-0183 RMC кадые 10 сек
AT+CGPSINFOCFG=10,2
# $GPRMC,,V,,,,,,,,,,N*53
# Прекратить вывод координат
AT+CGPSINFOCFG=0,2

```

## Разработка встраиваемого ПО
Для разработки предлагается использовать __Ubuntu18.04__ или __Debian9/10__.
SDK из скаченного `MDM9x07_OL_2U_22_V1.14_210525.tar.gz` нужно распаковать рядом с директорией проекта в `sim_open_sdk`:

```sh
# Структура рабочего каталога
base_dir/
\_ autoinformer/
\_ sim_open_sdk/
```

Перед началом сборки проекта необходимо инициализировать переменные окружения SDK:

```sh
# Выполняется из директории autoinformer
source ./env_init.sh
```




[win_driver]: https://goo.gl/jK5KS4 "win_driver"
[linux_driver]: https://goo.gl/g7qr5w "linux_driver"
[sim_qdl]: https://drive.google.com/file/d/1t4O6gTTZkQGW6CEIo9JCpMjjQwqoZaTL/view "QDL v1.66"
[sim_qpst]: https://drive.google.com/file/d/1K5DxiYI6Ah3ygFKLkLt_xe-94qXrWEeB/view "QPST v2.7"
[open_sdk]: https://drive.google.com/drive/folders/1rhtenK5DoPCR-x09FrPCWT3WQtmrcy5L "OpenLinux SDK v1.14"





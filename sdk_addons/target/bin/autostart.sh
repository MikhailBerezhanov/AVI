#!/bin/sh

set -e # fail on any error

# NETLIGHT LED (pin51, GPIO_18)
if test ! -d /sys/class/gpio/gpio18
then
	echo 18 > /sys/class/gpio/export
	echo out > /sys/class/gpio/gpio18/direction
fi

trap err_handler 1 2 3 15
err_handler() 
{
	local exit_status=${1:-$?}
	echo "exiting with status: $exit_status"
	exit $exit_status
}

# AVI application start
/data/avi/avi &

while :
do
	echo 1 > /sys/class/gpio/gpio18/value
	sleep 1
	echo 0 > /sys/class/gpio/gpio18/value
	sleep 1
done

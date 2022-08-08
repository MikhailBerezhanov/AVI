#!/bin/bash

if [ $# -eq 0 ]; then
	echo "no nmea file provided"
	exit 1
fi

filepath=${1}

filename="${filepath%.*}" 

# echo ${filename}

sed '/$GPGGA\|$GPGSA/d' ${filepath} > ${filename}.rmc
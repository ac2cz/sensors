#!/bin/bash

#
# SCRIPT for testing that sets up the lib paths on the PI
#
LOGFILE=sensor_data

export PATH="/mnt/usb-disk/ariss/bin:/home/pi/bin:$PATH"
export LD_LIBRARY_PATH=/mnt/usb-disk/ariss/lib:/usr/local/lib/iors_common:$LD_LIBRARY_PATH

echo Launching sensors

echo LD_LIBRARY_PATH: $LD_LIBRARY_PATH

./sensors -f $LOGFILE -p 30 -v 

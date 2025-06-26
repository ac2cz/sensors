/*
 * config.h
 *
 *  Created on: Sep 28, 2022
 *      Author: g0kla
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "common_config.h"
#include "sensor_telemetry.h"
#include "cosmic_watch.h"

#define VERSION __DATE__ " ARISS Sensors - Version 0.1a"

#define SENSOR_OFF 0
#define SENSOR_ON 1
#define SENSOR_ERR 2

/* These global variables are not in the config file */
extern int g_run_self_test;    /* true when the self test is running */
extern int g_verbose;          /* print verbose output when set */
extern char g_log_filename[MAX_FILE_PATH_LEN];
extern sensor_telemetry_t g_sensor_telemetry;
extern cw_data_t cw_raw_data;
extern cw_data_t cw_coincident_data;


/* These are declared here and defined in sensors.c */
extern char g_mic_serial_dev[MAX_FILE_PATH_LEN]; // device name for the serial port for ultrasonic mic
extern char g_cw1_serial_dev[MAX_FILE_PATH_LEN]; // device name for the serial port for cosmic watch
extern char g_cw2_serial_dev[MAX_FILE_PATH_LEN]; // device name for the serial port for cosmic watch

void load_config(char *filename);

#endif /* CONFIG_H_ */

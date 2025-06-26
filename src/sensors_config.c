/*
 * config.c
 *
 *  Created on: May 17, 2022
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
 *
 *  Load user configuration variables from a file.  This should hold all of the values
 *  that might change from one environment to the next.  e.g. different gains may
 *  be needed for different sound cards.
 *
 */

#include <string.h>
#include <stdlib.h>

#include "common_config.h"
#include "sensor_telemetry.h"
#include "cosmic_watch.h"
#include "ultrasonic_mic.h"
#include "str_util.h"

/* Define paramaters for config file */
#define MAX_CONFIG_LINE_LENGTH 128
#define CONFIG_MIC_SERIAL_DEVICE "mic_serial_device"
#define CONFIG_CW1_SERIAL_DEVICE "cw1_serial_device"
#define CONFIG_CW2_SERIAL_DEVICE "cw2_serial_device"
#define CONFIG_PERIOD_TO_SAMPLE_TELEM_IN_SECONDS "period_to_sample_telem_in_seconds"

/* These global variables are in the sensors_config.h file */
char g_mic_serial_dev[MAX_FILE_PATH_LEN] = "/dev/serial0"; // device name for the serial port for ultrasonic mic
char g_cw1_serial_dev[MAX_FILE_PATH_LEN] = "/dev/serial1"; // device name for the serial port for cosmic watch
char g_cw2_serial_dev[MAX_FILE_PATH_LEN] = "/dev/serial2"; // device name for the serial port for cosmic watch

#include <sensors_config.h>

void load_config(char *filename) {
	char *key;
	char *value;
	char *search = "=";
	debug_print("Loading config from: %s:\n", filename);
	FILE *file = fopen ( filename, "r" );
	if ( file != NULL ) {
		char line [ MAX_CONFIG_LINE_LENGTH ]; /* or other suitable maximum line size */
		while ( fgets ( line, sizeof line, file ) != NULL ) /* read a line */ {

			/* Token will point to the part before the =
			 * Using strtok safe here because we do not have multiple delimiters and
			 * no other threads started at this time. */
			key = strtok(line, search);

			// Token will point to the part after the =.
			value = strtok(NULL, search);
			if (value != NULL) { /* Ignore line with no key value pair */;

				debug_print(" %s",key);
				value[strcspn(value,"\n")] = 0; // Move the nul termination to get rid of the new line
				debug_print(" = %s\n",value);
				if (strcmp(key, CONFIG_MIC_SERIAL_DEVICE) == 0) {
					strlcpy(g_mic_serial_dev, value,sizeof(g_mic_serial_dev));
				} else if (strcmp(key, CONFIG_CW1_SERIAL_DEVICE) == 0) {
					strlcpy(g_cw1_serial_dev, value,sizeof(g_cw1_serial_dev));
				} else if (strcmp(key, CONFIG_CW2_SERIAL_DEVICE) == 0) {
					strlcpy(g_cw2_serial_dev, value,sizeof(g_cw2_serial_dev));
				} else {
					error_print("Unknown key in %s file: %s\n",filename, key);
				}
			}
		}
		fclose ( file );
	} else {
		error_print("FATAL..  Could not load sensor config file: %s\n", filename);
		exit(1);
	}
}


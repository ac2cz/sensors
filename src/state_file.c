/*
 * state_file.c
 *
 *  Created on: Jan 1, 2024
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
 *  Load user state variables from a file.
 *
 */

#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "state_file.h"
#include "str_util.h"

/* Forward functions */
int save_int_key_value(char * key, int val, FILE *file);

static char filename[MAX_FILE_PATH_LEN];

void load_state(char *filepath) {
	strlcpy(filename, filepath, sizeof(filename));
	char *key;
	char *value;
	char *search = "=";
	debug_print("Loading state from: %s:\n", filename);
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
				if (strcmp(key, STATE_SENSORS_ENABLED) == 0) {
					g_state_sensors_enabled = atoi(value);
				} else if (strcmp(key, STATE_PERIOD_TO_SEND_TELEM_IN_SECONDS) == 0) {
					g_state_period_to_send_telem_in_seconds = atoi(value);
				} else if (strcmp(key, STATE_PERIOD_TO_STORE_WOD_IN_SECONDS) == 0) {
					g_state_period_to_store_wod_in_seconds = atoi(value);
				} else if (strcmp(key, WOD_MAX_FILE_SIZE) == 0) {
					g_wod_max_file_size = atoi(value);
				} else if (strcmp(key, STATE_SENSOR_LOG_LEVEL) == 0) {
					g_state_sensor_log_level = atoi(value);
				} else {
					error_print("Unknown key in state file: %s : %s\n",filename, key);
				}
			}
		}
		fclose ( file );
	} else {
		debug_print("Could not load state file: %s\n", filename);
	}
}

void save_state() {
	char tmp_filename[MAX_FILE_PATH_LEN];
	strlcpy(tmp_filename, filename, sizeof(tmp_filename));
	strlcat(tmp_filename, ".tmp", sizeof(tmp_filename));

	//debug_print("Saving state to: %s:\n", tmp_filename);
	FILE *file = fopen ( tmp_filename, "w" );
	if ( file != NULL ) {
		if(save_int_key_value(STATE_SENSORS_ENABLED, g_state_sensors_enabled, file) == EXIT_FAILURE) { fclose(file); return;}
		if(save_int_key_value(STATE_PERIOD_TO_SEND_TELEM_IN_SECONDS, g_state_period_to_send_telem_in_seconds, file) == EXIT_FAILURE) { fclose(file); return;}
		if(save_int_key_value(STATE_PERIOD_TO_STORE_WOD_IN_SECONDS, g_state_period_to_store_wod_in_seconds, file) == EXIT_FAILURE) { fclose(file); return;}
		if(save_int_key_value(WOD_MAX_FILE_SIZE, g_wod_max_file_size, file) == EXIT_FAILURE) { fclose(file); return;}
	}
	fclose(file);
	/* This rename is atomic and overwrites the existing file.  So we either get the whole new file or we stay with the old one.*/
	rename(tmp_filename, filename);
}

int save_int_key_value(char * key, int val, FILE *file) {
	char buf[MAX_CONFIG_LINE_LENGTH];
	strlcpy(buf, key, sizeof(buf));
	strlcat(buf, "=", sizeof(buf));
	char int_str[25];
	snprintf(int_str, 25, "%d\n",val);
	strlcat(buf, int_str, sizeof(buf));
	int rc = fputs(buf, file);
	if (rc == EOF) return EXIT_FAILURE;
		return EXIT_SUCCESS;
}

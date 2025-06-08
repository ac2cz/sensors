/*
 * state_file.h
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
 */

#ifndef STATE_FILE_H_
#define STATE_FILE_H_

#include "common_config.h"

/* Define paramaters for state file */
#define STATE_SENSORS_ENABLED "sensors_enabled"
#define STATE_PERIOD_TO_SEND_TELEM_IN_SECONDS "period_to_send_telem_in_seconds"
#define STATE_PERIOD_TO_STORE_WOD_IN_SECONDS "period_to_store_wod_in_seconds"
#define WOD_MAX_FILE_SIZE "wod_max_file_size"
#define STATE_SENSOR_LOG_LEVEL "sensor_log_level"

extern int g_state_sensors_enabled;
extern int g_state_period_to_send_telem_in_seconds;
extern int g_state_period_to_store_wod_in_seconds;
extern int g_wod_max_file_size;
extern int g_state_sensor_log_level;

void load_state(char *filename);
void save_state();


#endif /* STATE_FILE_H_ */

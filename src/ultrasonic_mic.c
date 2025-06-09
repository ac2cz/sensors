/*
 * ultrasonic_mic.c
 *
 *  Created on: Jun 9, 2025
 *      Author: g0kla
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "state_file.h"
#include "debug.h"
#include "serial_util.h"
#include "ultrasonic_mic.h"
#include "serial_util.h"
#include "sensor_telemetry.h"

/* Forward declarations */


int mic_read_data(sensor_telemetry_t *sensor_telemetry) {
	unsigned char response[RESPONSE_LEN];
	char * cmd = "D";
	int cmd_len = 1;

	int rc = serial_send_cmd(g_mic_serial_dev, cmd, cmd_len, response, RESPONSE_LEN);
	debug_print("%s\n",response);
	int i;
	sensor_telemetry->microphone_valid = 1;
	for (i=0; i<32; i++) {
		sensor_telemetry->sound_psd[i] = response[i+5];
		debug_print("%d:%d, ", i*250/64,response[i+5]);
	}
	debug_print("\n");
	return rc;
}




/*
 * ultrasonic_mic.c
 *
 *  Created on: Jun 9, 2025
 *      Author: g0kla
 */
#include <sensors_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "sensors_state_file.h"
#include "debug.h"
#include "serial_util.h"
#include "ultrasonic_mic.h"
#include "serial_util.h"
#include "sensor_telemetry.h"

/* Forward declarations */

/* Local variables */
static int mic_listen_thread_called = false;
static unsigned char response[MIC_RESPONSE_LEN];

void mic_err(int err) {
	int i;
	g_sensor_telemetry.microphone_valid = err;
	for (i=0; i<32; i++) {
		g_sensor_telemetry.sound_psd[i] = 0;
	}
	if (err == SENSOR_ERR)
		debug_print("No Microphone connected\n");

}
/**
 * This reads the Data via serial from the Pi Pico ultrasonic mic.
 * The data is in the following format:
 * D nn,B0B1....Bnn
 *
 * Where nn is the number of bins in the FFT.  Each bin is a byte of data.  It is sent as raw bytes.
 * By default the FFT length 64 and there are 32 bins in the result
 *
 */
void mic_read_data() {
	char * cmd = "D";
	int cmd_len = 1;
	int i;

	if (g_state_sensors_cosmic_watch_enabled) {

		int rc = serial_send_cmd(g_mic_serial_dev, B38400, cmd, cmd_len, response, MIC_RESPONSE_LEN);
		//debug_print("%s\n",response);
		if (rc > 0) {
			if (response[0] == 'D') { // We have data
				for (i=0; i<32; i++) {
					g_sensor_telemetry.sound_psd[i] = response[i+5];
					debug_print("%d:%d, ", i*250/64,response[i+5]);
				}
				g_sensor_telemetry.microphone_valid = SENSOR_ON;
				debug_print("\n");
			} else {
				mic_err(SENSOR_ERR);
			}
		} else {
			mic_err(SENSOR_ERR);
		}
	} else {
		mic_err(SENSOR_OFF);
	}

}

/**
 * This process is run for ultrasonic mic.  It listens to a serial port and collects the data that is received.
 * It writes the data into the relavant parts of the telemetry structure.  It appends all of the data to a
 * file.
 *
 * Data is sent in from the Mic in the following format:
 *
 *
 */
void *mic_listen_process(void * arg) {
	char *name;
	name = (char *) arg;
	if (mic_listen_thread_called) {
		error_print("Thread already started.  Exiting: %s\n", name);
	}
	mic_listen_thread_called = true;
	//debug_print("Starting Thread: %s\n", name);

	while (mic_listen_thread_called) {
		// Test that we can open the serial, otherwise we get errors continually
		int fd = open_serial(g_mic_serial_dev, B38400);
		if (fd) {
			tcflush(fd,TCIOFLUSH );
			while (1) {
				if (g_verbose) debug_print("Waiting for mic..\n");
				mic_read_data(&g_sensor_telemetry);
			}
			close_serial(fd);
		} else {
			if (g_verbose)
				error_print("Error while initializing %s.\n", g_mic_serial_dev);
			mic_listen_thread_called = false;
		}
	}

	mic_listen_thread_called = false;
	return NULL;
}




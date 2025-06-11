/*
 * cosmic_watch.c
 *
 *  Created on: Jun 10, 2025
 *      Author: g0kla
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "config.h"
#include "state_file.h"
#include "debug.h"
#include "serial_util.h"
#include "ultrasonic_mic.h"
#include "serial_util.h"
#include "sensor_telemetry.h"

/* Forward declarations */

/* Local vars */
static int listen_thread_called = 0;

void *cw_listen_process(void * arg) {
	unsigned char response[RESPONSE_LEN];

	char *name;
	name = (char *) arg;
	if (listen_thread_called) {
		error_print("Thread already started.  Exiting: %s\n", name);
	}
	listen_thread_called = true;
	//debug_print("Starting Thread: %s\n", name);

	while (listen_thread_called) {

		int fd = open_serial(g_cw1_serial_dev, B9600);
		if (fd) {
			tcflush(fd,TCIOFLUSH );
			while (1) {
				int n = read(fd, response, RESPONSE_LEN);
				if (n > 0) {
					response[n] = 0; // terminate the string
					debug_print("%s",response);
				}
				usleep(50*1000);
				/*
						int i;
						for (i=0; i< n; i++) {
				//			debug_print("%c",response[i]);
												debug_print(" %0x",response[i] & 0xff);
						}
						debug_print("\n");
				 */

				//		set_rts(g_serial_fd, 0);
			}
			close_serial(fd);
		} else {
			if (g_verbose)
				error_print("Error while initializing %s.\n", g_cw1_serial_dev);
			listen_thread_called = false;
		}
	}

	//	debug_print("Exiting Thread: %s\n", name);
	listen_thread_called = false;
	return NULL;
}

int cw_listen_process_running() {
	return listen_thread_called;
}

void cw_exit_listen_process() {
	listen_thread_called = false;
}

int cwatch_read_data(sensor_telemetry_t *sensor_telemetry) {
	unsigned char response[RESPONSE_LEN];





	int rc = serial_read_data(g_cw1_serial_dev, B9600, response, RESPONSE_LEN);
	debug_print("%s\n",response);
	//	int i;
	sensor_telemetry->cosmic_watch_valid = 1;
	//	for (i=0; i<32; i++) {
	//		sensor_telemetry->sound_psd[i] = response[i+5];
	//		//debug_print("%d:%d, ", i*250/64,response[i+5]);
	//	}
	debug_print("\n");
	return rc;
}




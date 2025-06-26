/*
 * cosmic_watch.c
 *
 *  Created on: Jun 10, 2025
 *      Author: g0kla
 */


#include <sensors_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "iors_log.h"
#include "iors_command.h"
#include "sensors_state_file.h"
#include "debug.h"
#include "serial_util.h"
#include "ultrasonic_mic.h"
#include "serial_util.h"
#include "sensor_telemetry.h"
#include "cosmic_watch.h"
#include "str_util.h"

/* Forward declarations */
int cw_listen_process(char *data_folder_path, char *serial_dev, speed_t speed, int *thread_status);
cw_data_t *cw_parse_data(char *str_data);
void cw_debug_print_data(cw_data_t *data);

/* shared variable declared in cosmic_watch.h */
pthread_mutex_t cw_mutex = PTHREAD_MUTEX_INITIALIZER;
cw_data_t cw_raw_data; // This is raw data from one of the detectors
cw_data_t cw_coincident_data; // This is the co-incident data

/* Local vars */
static int cw1_listen_thread_called = 0;
static int cw2_listen_thread_called = 0;

/**
 * This process is run for cosmic watch 1.  It listens to a serial port and collects the data that is received.
 * It writes the data into the relavant parts of the telemetry structure.  It appends all of the data to a
 * file.
 *
 * Data is sent in plain text, space delimited, with the following columns:
 * Event_number Time_in_ms_since_start ADC sipm(mV) dead_time_ms temp_deg_c
 *
 */
int cw_listen_process(char *data_folder_path, char *serial_dev, speed_t speed, int *thread_status) {
	char response[CW_RESPONSE_LEN];
	FILE *fptr;
	int file_error = false;
	int first_entry = true;



	// Test that we can open the serial, otherwise we get errors continually
	int fd = open_serial(serial_dev, speed);
	if (fd) {
		tcflush(fd,TCIOFLUSH );
		while (*thread_status) { // monitor the serial port while program running
			//if (g_verbose) debug_print("Waiting for CW: %s..\n",serial_dev);
			//				int n = read(fd, response, CW_RESPONSE_LEN);
			int n = read_serial_line(serial_dev, speed, response, CW_RESPONSE_LEN, '\r');
			usleep(10*1000);
			if (n > 0) {
				//response[n] = 0; // terminate the string
				//debug_print("cw1##%s##",response);
				pthread_mutex_lock(&cw_mutex);
				cw_data_t *cw_data = cw_parse_data(response);
				if (cw_data != NULL) {
					cw_debug_print_data(cw_data);
					/* Write data to the temp file */
					char log_path[MAX_FILE_PATH_LEN];
					strlcpy(log_path, data_folder_path,MAX_FILE_PATH_LEN);
					strlcat(log_path,"/",MAX_FILE_PATH_LEN);
					strlcat(log_path,get_folder_str(FolderLog),MAX_FILE_PATH_LEN);
					strlcat(log_path,"/",MAX_FILE_PATH_LEN);
					if (cw_data->master_slave[0] == 'M')
						strlcat(log_path,g_sensors_cw_raw_log_path,MAX_FILE_PATH_LEN);
					else
						strlcat(log_path,g_sensors_cw_coincident_log_path,MAX_FILE_PATH_LEN);

					char tmp_filename[MAX_FILE_PATH_LEN];
					log_make_tmp_filename(log_path, tmp_filename);
					fptr = fopen(tmp_filename, "a");
					if (fptr != NULL) {
						if (first_entry) {
							/* *Write the date time */
							char data_str[256];
							time_t now = time(0);
							strftime(data_str, sizeof(data_str), "SOOSS CosmicWatch start: %y%m%d %H%M%S UTC", gmtime(&now));
							fwrite(data_str, 1, 256, fptr);
							fwrite("\n", 1, 1, fptr);
							first_entry=false;
						}
						fwrite(response, 1, CW_RESPONSE_LEN, fptr);
						fwrite("\n", 1, 1, fptr);
						fclose(fptr);
						file_error = false;

						long size = get_file_size(tmp_filename);

						if (size/1024 > g_state_sensors_cw_max_file_size_in_kb) {
							debug_print("Rolling SENSOR CW file %s as it is: %.1f KB\n",log_path, size/1024.0);
							log_add_to_directory(log_path);
						}
					} else {
						if (!file_error)
							log_err(g_log_filename, SENSOR_ERR_CW_FAILURE);
						file_error = true;
					}
				} /* If cw_data != NULL */
				pthread_mutex_unlock(&cw_mutex);
			}
		}
		close_serial(fd);
		return EXIT_SUCCESS;
	} else {
		if (g_verbose)
			error_print("Error while initializing %s.\n", serial_dev);
		log_err(g_log_filename, SENSOR_ERR_CW_FAILURE);
		return EXIT_FAILURE;
	}
}
void *cw1_listen_process(void * arg) {
	char *data_folder_path;
	data_folder_path = (char *) arg;
	if (cw1_listen_thread_called) {
		error_print("CW1 Thread already started.  Exiting: %s\n", data_folder_path);
		return NULL;
	}
	cw1_listen_thread_called = true;
	//debug_print("Starting Thread: %s\n", name);
	cw_listen_process(data_folder_path, g_cw1_serial_dev, B9600, &cw1_listen_thread_called);
	debug_print("CW1 Thread.  Exiting: %s\n", data_folder_path);
	return NULL;
}

void *cw2_listen_process(void * arg) {
	char *data_folder_path;
	data_folder_path = (char *) arg;
	if (cw2_listen_thread_called) {
		error_print("CW2 Thread already started.  Exiting: %s\n", data_folder_path);
		return NULL;
	}
	cw2_listen_thread_called = true;
	//debug_print("Starting Thread: %s\n", name);
	cw_listen_process(data_folder_path, g_cw2_serial_dev, B9600, &cw2_listen_thread_called);
	debug_print("CW2 Thread.  Exiting: %s\n", data_folder_path);
	return NULL;
}

int debug_parsing = false;

cw_data_t *cw_parse_data(char *str_data) {
	cw_data_t tmp_data;

	char response[CW_RESPONSE_LEN];
	strlcpy(response, str_data, CW_RESPONSE_LEN); //make a copy as the analysis is destructive
	char *master_slave;
	char *event;
	char *time_ms;
	char *count_avg;
	//char *count_std;
	char *sipm_voltage;
	char *deadtime_ms;
	char *temperature_deg_c;
	char *search = " ";

	master_slave = strtok(response, search);
	if (master_slave == NULL) { if (debug_parsing) debug_print("*** Missing master slave\n");return NULL;}
	if (strlen(master_slave) > 1) return NULL;

	strlcpy(tmp_data.master_slave, master_slave, sizeof(tmp_data.master_slave));
	//debug_print(" MS:%s:%s:\n",master_slave, data->master_slave);

	event = strtok(NULL, search);
	if (event == NULL) { if (debug_parsing) debug_print("*** Missing event\n");return NULL;}
	tmp_data.event_num = atoi(event);
	//debug_print(" Num: %s - %d\n",event, data->event_num);

	time_ms = strtok(NULL, search);
	if (time_ms == NULL) { if (debug_parsing) debug_print("*** Missing time\n");return NULL; }
	tmp_data.time_ms = atoi(time_ms);

	count_avg = strtok(NULL, search);
	if (count_avg == NULL) { if (debug_parsing) debug_print("*** Missing count avg\n");return NULL; }
	tmp_data.count_avg = atof(count_avg);

	//    count_std = strtok(NULL, search);
	//    if (count_std == NULL) { debug_print("*** Missing count_std\n");return false; }
	//    data->count_std = atoi(count_std);

	sipm_voltage = strtok(NULL, search);
	if (sipm_voltage == NULL) { if (debug_parsing) debug_print("*** Missing sipm\n"); return NULL; }
	tmp_data.sipm_voltage = atof(sipm_voltage);

	deadtime_ms = strtok(NULL, search);
	if (deadtime_ms == NULL) { if (debug_parsing) debug_print("*** Missing deadtime\n"); return NULL; }
	tmp_data.deadtime_ms = atoi(deadtime_ms);

	temperature_deg_c = strtok(NULL, search);
	if (temperature_deg_c == NULL) { if (debug_parsing) debug_print("*** Missing temperature\n");return NULL; }
	//temperature_deg_c[strcspn(temperature_deg_c,"\n")] = 0;
	tmp_data.temperature_deg_c = atof(temperature_deg_c);
	//debug_print(" Temp: %s - %f\n",temperature_deg_c, data->temperature_deg_c);

	/* If all the checks passed then we assign this to the correct struct and return it */
	if (master_slave[0] == 'M') {
		debug_print("Particle-");
		cw_raw_data = tmp_data;
		return &cw_raw_data;
	} else {
		debug_print("Coincident-");
		cw_coincident_data = tmp_data;
		return &cw_coincident_data;
	}
}

void cw_debug_print_data(cw_data_t *data) {
	debug_print("%s %d %d %.2f %.2f %d %.1f\n", data->master_slave,
			data->event_num, data->time_ms, data->count_avg, data->sipm_voltage, data->deadtime_ms, data->temperature_deg_c);
}

void cw1_exit_listen_process() {
	cw1_listen_thread_called = false;
}

void cw2_exit_listen_process() {
	cw2_listen_thread_called = false;
}




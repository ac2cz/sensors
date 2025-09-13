/*
 ============================================================================
 Name        : sensors.c
 Author      : VE2TCP Chris Thompson g0kla@arrl.net
 Version     : 1.0
 Copyright   : Copyright 2024 ARISS
 Description : Student On Orbit Sensor System Main
 ============================================================================

 This program reads the Student On Orbit Sensor system and writes the data to several files.
 - The RT telemetry file, which contains one line of data and is overwritten with new data
 - The WOD telemetry file, which is appended until the file is rolled or a max size as
   a safety precaution.
 - A file for events from the CosmicWatch
 - A file for detailed data from the Ultrasonic Microphone

 Fixed settings are stored in the sensors.config file
 Volatile settings are stored in the state file.  This file can be updated by iors_control
 based on commands from the ground, the UI or for other operational reasons.  This file is
 read each cycle so that the latest changes are applied.

 The file names are in the config file and write data to a temporary file until we are
 ready to copy it to a final file.

 The Real Time telemetry file is overwritten each time telemetry is sampled.  This file is read
 by iors_control when telemetry needs to be sent. To avoid reading a partial file the file is
 written to a tmp file and then renamed.  The real time file is written at the same frequency
 as the sample period.  It is read by iors_control periodically as determined by the period to
 send real time telemetry.

 The collection period for the WOD file is in the state file.  Completing the file
 involves renaming the temporary file to its final name.  Usually this will be in a
 queue folder for ingestion into the pacsat directory.

 There is a slight complexity if we are running from the mounted USB drive or if we booted
 from the USB drive.  This means the data folder is passed on the command line, just as
 it is for pi_pacsat

 All telem files should be raw bytes suitable for reading back into a C structure

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#include "sensors_config.h"
#include "sensors_state_file.h"
#include "iors_log.h"
#include "iors_command.h"
#include "sensor_telemetry.h"
#include "sensors_gpio.h"
#include "str_util.h"
#include "AD.h"
#include "LPS22HB.h"
#include "SHTC3.h"
#include "xensiv_pasco2.h"
#include "IMU.h"
#include "TCS34087.h"
#include "ultrasonic_mic.h"
#include "cosmic_watch.h"
#include "dfrobot_gas.h"

#define MAX_FILE_PATH_LEN 256
#define ADC_O2_CHAN 2
#define ADC_METHANE_CHAN 0
#define ADC_AIR_QUALITY_CHAN 1
#define ADC_BUS_V_CHAN 3

/*
 *  GLOBAL VARIABLES defined here.  They are declared in config.h
 *  These are the default values.  Many can be updated with a value
 *  in pacsat.config or can be overridden on the command line.
 *
 */
int g_run_self_test;    /* true when the self test is running */
int g_verbose = false;
char g_log_filename[MAX_FILE_PATH_LEN];
sensor_telemetry_t g_sensor_telemetry;

/* Forward functions */
int read_sensors(uint32_t now);
void help(void);
void signal_exit (int sig);
void signal_load_config (int sig);
int save_rt_telem(char * tmp_filename, char *rt_telem_path);
int read_sensors(uint32_t now);
double linear_interpolation(double x, double x0, double x1, double y0, double y1);

/* Local Variables */
char sensors_state_file_name[MAX_FILE_PATH_LEN] = "sensors.state";
char config_file_name[MAX_FILE_PATH_LEN] = "sensors.config";
char data_folder_path[MAX_FILE_PATH_LEN] = "/ariss";
int gpio_hd = -1;
float board_temperature = 0.0;

sensor_telemetry_t g_sensor_telemetry;

//int PERIOD=10;
//char filename[MAX_FILE_PATH_LEN];
int co2_status = false;
int o2_status = false;
int imu_status = false;
int tcs_status = false;
int calibrate_with_dfrobot_sensor = 0;

extern int debug_counts;

int period_to_load_state_file = 60;
time_t last_time_checked_state_file = 0;
time_t last_time_checked_wod = 0;
time_t last_time_checked_period_to_sample_telem = 0;

pthread_t cw1_listen_pthread = 0;
pthread_t cw2_listen_pthread = 0;
pthread_t mic_listen_pthread = 0;

int g_num_of_file_io_errors = 0; // the cumulative number of file io errors

/* Temperature compensation table for O2 saensor */
#define O2_TEMPERATURE_TABLE_LEN 6
double o2_temp_table[O2_TEMPERATURE_TABLE_LEN][2] = {
		{0.0, 3.0}
		,{10.0, 1.0}
		,{20.0,0.0}
		,{30,-1.0}
		,{40.0,-2.0}
		,{50.0,-3.0}
};

int main(int argc, char *argv[]) {
	signal (SIGQUIT, signal_exit);
	signal (SIGTERM, signal_exit);
	signal (SIGHUP, signal_load_config);
	signal (SIGINT, signal_exit);

	struct option long_option[] = {
			{"help", no_argument, NULL, 'h'},
			{"dir", required_argument, NULL, 'd'},
			{"config", required_argument, NULL, 'c'},
			{"test", no_argument, NULL, 't'},
			{"verbose", no_argument, NULL, 'v'},
			{"print-cw", no_argument, NULL, 'p'},
			{NULL, 0, NULL, 0},
	};

	int more_help = false;

	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hd:c:tvp", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h': // help
			more_help = true;
			break;
		case 't': // calibrate
			calibrate_with_dfrobot_sensor = 1;
			break;
		case 'v': // verbose
			g_verbose = true;
			break;
		case 'p': // verbose
			debug_counts = true;
			break;
		case 'c': // config file name
			strlcpy(config_file_name, optarg, sizeof(config_file_name));
			break;
		case 'd': // data folder
			strlcpy(data_folder_path, optarg, sizeof(data_folder_path));
			break;

		default:
			break;
		}
	}

	if (more_help) {
		help();
		return 0;
	}

	/* Load configuration from the config file */
	load_config(config_file_name);
	load_sensors_state(sensors_state_file_name, g_verbose);

	char rt_telem_path[MAX_FILE_PATH_LEN];
	strlcpy(rt_telem_path, data_folder_path,MAX_FILE_PATH_LEN);
	strlcat(rt_telem_path,"/",MAX_FILE_PATH_LEN);
	strlcat(rt_telem_path,g_sensors_rt_telem_path,MAX_FILE_PATH_LEN);

	char wod_telem_path[MAX_FILE_PATH_LEN];
	strlcpy(wod_telem_path, data_folder_path,MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,"/",MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,get_folder_str(FolderSenWod),MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,"/",MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,g_sensors_wod_telem_path,MAX_FILE_PATH_LEN);

	char log_path[MAX_FILE_PATH_LEN];
	strlcpy(log_path, data_folder_path,MAX_FILE_PATH_LEN);
	strlcat(log_path,"/",MAX_FILE_PATH_LEN);
	strlcat(log_path,get_folder_str(FolderLog),MAX_FILE_PATH_LEN);

	log_init(get_log_name_str(LOG_NAME), log_path, g_log_filename);
	log_set_level(g_state_sensors_log_level);
	log_alog1(INFO_LOG, g_log_filename, ALOG_SENSORS_STARTUP, 0);

	if (strlen(g_sensors_rt_telem_path) == 0) {
		printf("ERROR: Telemetry filename required\n");
		return 1;
	}
	if (strlen(g_sensors_wod_telem_path) == 0) {
		printf("ERROR: WOD Telemetry filename required\n");
		return 2;
	}

	if (g_verbose) {
		printf("Student On Orbit Sensor System Telemetry Capture\n");
		printf("Build: %s\n", VERSION);
	}

	gpio_hd = sensors_gpio_init();

	if (g_state_sensors_methane_enabled)
		lgGpioWrite(gpio_hd, SENSORS_GPIO_MQ6_EN, 1);
	if (g_state_sensors_air_q_enabled)
		lgGpioWrite(gpio_hd, SENSORS_GPIO_MQ135_EN, 1);

	/* Setup the IMU.  Defaults are:
	 * 2g Accelerometer
	 * Gyro 32dps
	 * Mag is -4912 to 4912uT, for 2s complement 16 bit result */
	imu_status = imuInit();
	if (g_verbose)
		if (imu_status == false) printf("QMI8658_init fail\n");
	//	debug_print("IMU State: %d\n",g_imu_state);

	// TODO - redundant??
    o2_status = true; // measure o2

	if (g_state_sensors_co2_enabled)
		lgGpioWrite(gpio_hd, SENSORS_GPIO_CO2_EN, 1);
	int res = xensiv_pasco2_init();
	if (res == EXIT_SUCCESS) {
		co2_status = true;
	} else {
		if (g_verbose)
			printf("Could not open CO2 gas sensor: %d\n",res);
	}

	/* We may need to pass the gain through from config.  We would add to the command line so iors_control
	 * can set it */
	if(TCS34087_Init(TCS34087_GAIN_16X) == 0) {
		if (g_verbose)
			printf("TCS34087 init\n");
		tcs_status = true;
	} else {
		if (g_verbose)
			printf("Could not open TCS34087 light/color sensor\n");
	}

	/* Make a tmp filename so that atomic writes to the RT file can be made with a rename */
	char tmp_filename[MAX_FILE_PATH_LEN];
	log_make_tmp_filename(rt_telem_path, tmp_filename);

	debug_print("RT Telem: %s - Length: %d bytes\n", rt_telem_path, (int)sizeof(g_sensor_telemetry));

	/**
	 * Start a thread to listen to the Cosmic watch.  This will write all received data into
	 * a file.  This thread runs in the background and is always ready to
	 * receive data from the Cosmic Watch.
	 */
	int thread_rc = pthread_create( &cw1_listen_pthread, NULL, cw1_listen_process, (void*) data_folder_path);
	if (thread_rc != EXIT_SUCCESS) {
			log_err(g_log_filename, SENSOR_ERR_CW_FAILURE);
			error_print("Could not start the CW1 listen thread.\n");
		}
	int thread2_rc = pthread_create( &cw2_listen_pthread, NULL, cw2_listen_process, (void*) data_folder_path);
	if (thread2_rc != EXIT_SUCCESS) {
		log_err(g_log_filename, SENSOR_ERR_CW_FAILURE);
		error_print("Could not start the CW2 listen thread.\n");
	}

//	int thread3_rc = pthread_create( &mic_listen_pthread, NULL, mic_listen_process, (void*) data_folder_path);
//	if (thread3_rc != EXIT_SUCCESS) {
//		log_err(g_log_filename, SENSOR_ERR_MIC_FAILURE);
//		error_print("Could not start the MIC listen thread.\n");
//	}

	/* Now read the sensors until we get an interrupt to exit */
	time_t now = time(0);
//	last_time_checked_rt = now;
	last_time_checked_wod = now;

	while (1) {
		now = time(0);

		if (g_state_sensors_period_to_sample_telem_in_seconds > 0) {

			if (g_state_sensors_period_to_store_wod_in_seconds > 0) { /* Then WOD is enabled */
				if ((now - last_time_checked_wod) > g_state_sensors_period_to_store_wod_in_seconds) {
					last_time_checked_wod = now;

					pthread_mutex_lock(&cw_mutex);
					long size = log_append(wod_telem_path,(unsigned char *)&g_sensor_telemetry, sizeof(g_sensor_telemetry));
					pthread_mutex_unlock(&cw_mutex);
					if (size < sizeof(g_sensor_telemetry)) {
						if (g_verbose)
							printf("ERROR, could not save data to filename: %s\n",g_sensors_wod_telem_path);
						g_num_of_file_io_errors++;
					} else {
						if (g_verbose)
							printf("Wrote WOD file: %s at %d\n",g_sensors_wod_telem_path, g_sensor_telemetry.timestamp);
					}

					/* If we have exceeded the WOD size threshold then roll the WOD file */
					if (size/1024 > g_state_sensors_wod_max_file_size_in_kb) {
						debug_print("Rolling SENSOR WOD file as it is: %.1f KB\n", size/1024.0);
						log_add_to_directory(wod_telem_path);
					}

				}
			} /* If Wod is enabled */

			if ((now - last_time_checked_period_to_sample_telem) > g_state_sensors_period_to_sample_telem_in_seconds) {
				last_time_checked_period_to_sample_telem = now;
				load_sensors_state(sensors_state_file_name, false); /* We load the state each cycle, which is normally at least 30 seconds, in case iors_control has changed something */
				last_time_checked_state_file = now;

				read_sensors(now);
				mic_read_data();

				//TODO - some sort of locks here to make sure we get valid data from Muon detectors and wait if it is currently being written.

				/* Put in latest data from the CosmicWatches if we have it */
				pthread_mutex_lock(&cw_mutex);
				if (g_state_sensors_cosmic_watch_enabled) {
					if (strlen(cw_raw_data.master_slave) != 0) {
						g_sensor_telemetry.cw_raw_valid = SENSOR_ON;
						g_sensor_telemetry.cw_raw_count = cw_raw_data.event_num;
						g_sensor_telemetry.cw_raw_rate = cw_raw_data.count_avg;
						if (g_verbose) debug_print("Raw count: %d Raw Rate %d\n",g_sensor_telemetry.cw_raw_count, g_sensor_telemetry.cw_raw_rate);
					} else {
						g_sensor_telemetry.cw_raw_valid = SENSOR_ERR;
						g_sensor_telemetry.cw_raw_count = 0;
						g_sensor_telemetry.cw_raw_rate = 0;
					}
					if (strlen(cw_coincident_data.master_slave) != 0) {
						g_sensor_telemetry.cw_coincident_valid = SENSOR_ON;
						g_sensor_telemetry.cw_coincident_count = cw_coincident_data.event_num;
						g_sensor_telemetry.cw_coincident_rate = cw_coincident_data.count_avg;
						if (g_verbose) debug_print("Co count: %d Co Rate %d\n",g_sensor_telemetry.cw_coincident_count, g_sensor_telemetry.cw_coincident_rate);
					} else {
						g_sensor_telemetry.cw_coincident_valid = SENSOR_ERR;
						g_sensor_telemetry.cw_coincident_count = 0;
						g_sensor_telemetry.cw_coincident_rate = 0;
					}
				} else {
					g_sensor_telemetry.cw_raw_valid = SENSOR_OFF;
					g_sensor_telemetry.cw_coincident_valid = SENSOR_OFF;
					g_sensor_telemetry.cw_coincident_count = 0;
					g_sensor_telemetry.cw_raw_count = 0;
					g_sensor_telemetry.cw_coincident_rate = 0;
					g_sensor_telemetry.cw_raw_rate = 0;
				}

				save_rt_telem(tmp_filename, rt_telem_path);
				pthread_mutex_unlock(&cw_mutex);
			} /* if time to sample sensors */
		} /* if sensors enabled */

		/* If the sensors are not enabled or if the sample period is set to a very long value, then we still want to check
		 * the state file every min */
		if ((now - last_time_checked_state_file) > period_to_load_state_file) {
			last_time_checked_state_file = now;
			load_sensors_state(sensors_state_file_name, g_verbose); /* We load the state each cycle, which is normally at least 30 seconds, in case iors_control has changed something */
		}

		if (g_num_of_file_io_errors > MAX_NUMBER_FILE_IO_ERRORS) {
			log_err(g_log_filename, IORS_ERR_MAX_FILE_IO_ERRORS);
			signal_exit(0);
		}

	} /* while (1) */
}

/**
 * Print this help if the -h or --help command line options are used
 */
void help(void) {
	printf(
			"Usage: sensors [OPTION]... \n"
			"-h,--help                        help\n"
			"-c,--config                      use config file specified\n"
			"-d,--dir                         use this data directory, rather than default\n"
			"-t,--test                        provide readings from additional calibration sensor\n"
			"-v,--verbose                     print additional status and progress messages\n"
	);
	exit(EXIT_SUCCESS);
}


void signal_exit (int sig) {
	if(g_verbose && sig > 0)
		printf (" Signal received, exiting ...\n");
	TCS34087_Close();
	imuClose();
	sensors_gpio_close();
	lguSleep(2/1000);
	log_alog1(INFO_LOG, g_log_filename, ALOG_SENSORS_SHUTDOWN, 0);
	exit (0);
}

void signal_load_config (int sig) {
	load_config(config_file_name);
	load_sensors_state(sensors_state_file_name, g_verbose);
}

int save_rt_telem(char * tmp_filename, char *rt_telem_path) {
	uint8_t * data = (unsigned char *)&g_sensor_telemetry;
	FILE * outfile = fopen(tmp_filename, "wb");
	if (outfile != NULL) {
		/* Save the realtime telemetry bytes into a tmp file then rename it.  This makes the write atomic */
		for (int i=0; i<sizeof(g_sensor_telemetry); i++) {
			int c = fputc(data[i],outfile);
			if (c == EOF) {
				fclose(outfile);
				break;
			}
		}
		fclose(outfile);
		if (rename(tmp_filename, rt_telem_path) != EXIT_SUCCESS) {
			if (g_verbose)
				printf("ERROR, could not rename RT telem filename from: %s to: %s\n",tmp_filename, g_sensors_rt_telem_path);
			g_num_of_file_io_errors++;
		} else {
			if (g_verbose)
				printf("Wrote RT file: %s at %d\n",g_sensors_rt_telem_path, g_sensor_telemetry.timestamp);
			return EXIT_FAILURE;
		}
	} else {
		if (g_verbose)
			printf("ERROR, could not save data to filename: %s\n",g_sensors_rt_telem_path);
		g_num_of_file_io_errors++;
		return EXIT_FAILURE;
		//TODO - store error.  Repeating errors like this should go in the error count, otherwise they would fill the log.
	}
	return EXIT_SUCCESS;

}

int read_sensors(uint32_t now) {
	g_sensor_telemetry.timestamp = now;
	/* Read the PI sensors */

	short val;
	int rc;
	if (g_state_sensors_methane_enabled) {
		lgGpioWrite(gpio_hd, SENSORS_GPIO_MQ6_EN, 1);
		rc = adc_read(ADC_METHANE_CHAN, &val);
		if (rc != EXIT_SUCCESS) {
			if (g_verbose)
				printf("Could not open MQ-6 Methane sensor ADC channel %d\n",ADC_METHANE_CHAN);
			g_sensor_telemetry.methane_conc = 0;
			g_sensor_telemetry.methane_sensor_valid = SENSOR_ERR;
		} else {
			g_sensor_telemetry.methane_conc = val;
			g_sensor_telemetry.methane_sensor_valid = SENSOR_ON;
			if (g_verbose)
				printf("MQ-6 Methane: %d,",val);
		}
	} else {
		lgGpioWrite(gpio_hd, SENSORS_GPIO_MQ6_EN, 0);
		g_sensor_telemetry.methane_conc = 0;
		g_sensor_telemetry.methane_sensor_valid = SENSOR_OFF;
	}

	if (g_state_sensors_air_q_enabled) {
		lgGpioWrite(gpio_hd, SENSORS_GPIO_MQ135_EN, 1);

		rc = adc_read(ADC_AIR_QUALITY_CHAN, &val);
		if (rc != EXIT_SUCCESS) {
			if (g_verbose)
				printf("Could not open MQ-135 Air Quality ADC channel %d\n",ADC_AIR_QUALITY_CHAN);
			g_sensor_telemetry.air_quality = 0;
			g_sensor_telemetry.air_q_sensor_valid = SENSOR_ERR;
		} else {
			if (g_verbose)
				printf("MQ-135 Air Q: %d\n",val);
			g_sensor_telemetry.air_quality = val;
			g_sensor_telemetry.air_q_sensor_valid = SENSOR_ON;
		}
	} else {
		lgGpioWrite(gpio_hd, SENSORS_GPIO_MQ135_EN, 0);
		g_sensor_telemetry.air_quality = 0;
		g_sensor_telemetry.air_q_sensor_valid = SENSOR_OFF;
	}

//	rc = adc_read(ADC_BUS_V_CHAN, &val);
//	if (rc != EXIT_SUCCESS) {
//		if (g_verbose)
//			printf("Could not open Bus Voltage sensor ADC channel %d\n",ADC_BUS_V_CHAN);
//	} else {
//		g_sensor_telemetry.pi_bus_v = val;
//		if (g_verbose)
//			printf("PI Bus (5V): %0.0fmV,",2*val*0.125);
//	}

	/* Read Waveshare C board sensors */
	/* Read the SHTC3 temp and humidity */
	if (g_state_sensors_temp_humidity_enabled) {
		short temperature, humidity;
		if (SHTC3_read(&temperature, &humidity) != EXIT_SUCCESS) {
			if (g_verbose)
				printf("Could not open SHTC3 Temperature sensor\n");
			g_sensor_telemetry.SHTC3_temp = 0;
			g_sensor_telemetry.SHTC3_humidity = 0;
			g_sensor_telemetry.TempHumidityValid = SENSOR_ERR;
		} else {
			g_sensor_telemetry.SHTC3_temp = temperature;
			g_sensor_telemetry.SHTC3_humidity = humidity;
			g_sensor_telemetry.TempHumidityValid = SENSOR_ON;

			board_temperature = 175 * (float)temperature / 65536.0f - 45.0f; // Calculate temperature value, which we use to compensate O2
			if (g_verbose) {
				float RH_Value;
				RH_Value = 100 * (float)humidity / 65536.0f;         // Calculate humidity value
				printf("Temperature = %6.2f°C , Humidity = %6.2f%% \n", board_temperature, RH_Value);
			}
		}
	} else {
		g_sensor_telemetry.SHTC3_temp = 0;
		g_sensor_telemetry.SHTC3_humidity = 0;
		g_sensor_telemetry.TempHumidityValid = SENSOR_OFF;
	}
	/* Read the lps22 pressure sensor and its temperature */
	if (g_state_sensors_pressure_enabled) {
		short lps22_temperature;
		int pressure;
		if (LPS22HB_read(&pressure, &lps22_temperature) != EXIT_SUCCESS) {
			g_sensor_telemetry.LPS22_pressure = 0;
			g_sensor_telemetry.LPS22_temp = 0;
			g_sensor_telemetry.PressureValid = SENSOR_ERR;
			if (g_verbose)
				printf("Could not open LPS22 Pressure sensor\n");
		} else {
			g_sensor_telemetry.LPS22_pressure = pressure;
			g_sensor_telemetry.LPS22_temp = lps22_temperature;
			g_sensor_telemetry.PressureValid = SENSOR_ON;
			if (g_verbose)
				printf("Pressure = %6.3f hPa, Temperature = %6.2f °C\n", pressure/4096.0, lps22_temperature/100.0);
		}
	} else {
		g_sensor_telemetry.LPS22_pressure = 0;
		g_sensor_telemetry.LPS22_temp = 0;
		g_sensor_telemetry.PressureValid = SENSOR_OFF;
	}

	/* Read the Gyroscope */
	if (g_state_sensors_imu_enabled) {
		g_sensor_telemetry.AccelerationX = 0;
		g_sensor_telemetry.AccelerationY = 0;
		g_sensor_telemetry.AccelerationZ = 0;
		g_sensor_telemetry.GyroX = 0;
		g_sensor_telemetry.GyroY = 0;
		g_sensor_telemetry.GyroZ = 0;
		g_sensor_telemetry.MagX = 0;
		g_sensor_telemetry.MagY = 0;
		g_sensor_telemetry.MagZ = 0;
		g_sensor_telemetry.IMUTemp = 0;

		if (imu_status) {
			IMU_ST_SENSOR_DATA stGyroRawData;
			IMU_ST_SENSOR_DATA stAccelRawData;
			IMU_ST_SENSOR_DATA stMagnRawData;

			imuDataGetRaw(&stGyroRawData, &stAccelRawData, &stMagnRawData);
			if (g_verbose) {
				printf("Acceleration: X: %d     Y: %d     Z: %d \n",stAccelRawData.s16X, stAccelRawData.s16Y, stAccelRawData.s16Z);
				printf("Gyroscope: X: %d     Y: %d     Z: %d \n",stGyroRawData.s16X, stGyroRawData.s16Y, stGyroRawData.s16Z);
				printf("Magnetic: X: %d     Y: %d     Z: %d \n",stMagnRawData.s16X, stMagnRawData.s16Y, stMagnRawData.s16Z);
			}
			g_sensor_telemetry.AccelerationX = stAccelRawData.s16X;
			g_sensor_telemetry.AccelerationY = stAccelRawData.s16Y;
			g_sensor_telemetry.AccelerationZ = stAccelRawData.s16Z;
			g_sensor_telemetry.GyroX = stGyroRawData.s16X;
			g_sensor_telemetry.GyroY = stGyroRawData.s16Y;
			g_sensor_telemetry.GyroZ = stGyroRawData.s16Z;
			g_sensor_telemetry.MagX = stMagnRawData.s16X;
			g_sensor_telemetry.MagY = stMagnRawData.s16Y;
			g_sensor_telemetry.MagZ = stMagnRawData.s16Z;
			g_sensor_telemetry.IMUTemp = QMI8658_readTemp();
			g_sensor_telemetry.ImuValid = SENSOR_ON;
		} else {
			g_sensor_telemetry.ImuValid = SENSOR_ERR;
		} /* if imu_status */
	} else {
		g_sensor_telemetry.ImuValid = SENSOR_OFF;
	} /* if g_state_sensors_imu_enabled */

	/* Read the xensiv CO2 sensor
	 * Note that this is dependant on the pressure reading */
	if (g_state_sensors_co2_enabled) {
		lgGpioWrite(gpio_hd, SENSORS_GPIO_CO2_EN, 1);
		if (co2_status == true && g_sensor_telemetry.PressureValid == SENSOR_ON) {
			uint16_t co2_ppm_val;
			uint16_t pressure_ref = (uint16_t)(g_sensor_telemetry.LPS22_pressure/4096.0);
			if (xensiv_pasco2_read(pressure_ref, &co2_ppm_val) != XENSIV_PASCO2_READ_NRDY) {
				if (g_verbose)
					printf("CO2: %d ppm at %d hPa\n",co2_ppm_val, pressure_ref);
				g_sensor_telemetry.CO2_conc = co2_ppm_val;
				g_sensor_telemetry.co2_sensor_valid = SENSOR_ON;
			} else {
				if (g_verbose)
					printf("CO2 Sensor not ready\n");
				g_sensor_telemetry.co2_sensor_valid = SENSOR_ERR;
				g_sensor_telemetry.CO2_conc = 0;
			}
		} else {
			g_sensor_telemetry.co2_sensor_valid = SENSOR_ERR;
			g_sensor_telemetry.CO2_conc = 0;
		}
	} else {
		lgGpioWrite(gpio_hd, SENSORS_GPIO_CO2_EN, 0);
		g_sensor_telemetry.co2_sensor_valid = SENSOR_OFF;
		g_sensor_telemetry.CO2_conc = 0;
	}

	/* Read the PS1 solid state O2 sensor.  Average 10 readings over 10 seconds
	 * Note this is dependant on the temperature reading from the pressure sensor */
	if (g_state_sensors_o2_enabled) {
		if (o2_status && g_sensor_telemetry.TempHumidityValid == SENSOR_ON) {
			int c=0;
			float avg=0.0, max=0.0,min=65555;
			// Dummy read which will be low
			rc = adc_read(ADC_O2_CHAN, &val);
			sleep(1);
			while (c < 10) {
				rc = adc_read(ADC_O2_CHAN, &val);
				if (rc != EXIT_SUCCESS) {
					if (g_verbose)
						printf("Could not open O2 Sensor ADC channel %d\n",ADC_O2_CHAN);
					g_sensor_telemetry.o2_sensor_valid = SENSOR_ERR;
					break;
				} else {
					//float volts = val * 0.125;
					//float o2_conc = -0.01805 * volts + 44.5835;
					//			printf("%.2f%% ..\n",o2_conc);
				//		if (g_verbose)
				//			printf("PS1 O2 : %d(%0.0fmv)\n", val,(float)volts);
							//printf("PS1 O2 Conc: %.2f%% %d(%0.0fmv)\n",o2_conc, val,(float)volts);
					if (val > max) max = val;
					if (val < min) min = val;
					avg+= val;
					g_sensor_telemetry.o2_sensor_valid = SENSOR_ON;
				}
				sleep(1);
				c++;
			}
			avg = avg / c;
			float volts = avg * 0.125;
			float o2_conc = -0.0354 * volts + 86.434;
			// VE2TCP prototype - float o2_conc = -0.01805 * volts + 44.5835;

			if (c > 0 && g_sensor_telemetry.o2_sensor_valid == SENSOR_ON) {
				g_sensor_telemetry.O2_raw = volts;
				/* Compensate for Temperature,  Look up temperature in table and interpolate the correction amount */
				int i = 0;
				double offset = 0.0;
				double first_key = 0;
				double last_key = 0;
				double first_value = 0;
				double last_value = 0;
				//double temp = g_sensor_telemetry.LPS22_temp/100.0;

				if (board_temperature >= 0 && board_temperature <= 50) {
					while (i++ < O2_TEMPERATURE_TABLE_LEN) {
						if (o2_temp_table[i][0] < board_temperature) {
							first_key = o2_temp_table[i][0];
							first_value = o2_temp_table[i][1];
						}
						if (o2_temp_table[i][0] > board_temperature) {
							last_key = o2_temp_table[i][0];
							last_value = o2_temp_table[i][1];
							break;
						}
					}
					offset = linear_interpolation(board_temperature, first_key, last_key, first_value, last_value);
					//offset = -0.6667 * board_temperature * board_temperature + 37.667 * board_temperature - 531.24;

					if (g_verbose)
					    printf("Lookup: between: %2.1f %2.1f compensate by: %2.3f\n",first_key, last_key, offset);
				}

				if (g_verbose)
					printf("PS1 O2 Conc: %.2f (%.2f) %d(%0.2fmv) max:%0.2f min:%0.2f\n",o2_conc + offset, o2_conc, val,(float)volts, max*0.125, min*0.125);
				if (o2_conc > 25 || o2_conc < 0) {
					g_sensor_telemetry.o2_sensor_valid = SENSOR_ERR;
					g_sensor_telemetry.O2_conc = 0;
				} else {
					//g_sensor_telemetry.O2_conc = (short)((o2_conc + offset)*100); // shift percentage like 20.95 to be 2095
					g_sensor_telemetry.O2_conc = (short)((o2_conc - (0.769852 *(board_temperature - 24.90947)))*100);
				}
			} else {
				g_sensor_telemetry.O2_conc = 0;
				g_sensor_telemetry.O2_raw = 0;
			}
		} else {
			g_sensor_telemetry.o2_sensor_valid = SENSOR_ERR;
			g_sensor_telemetry.O2_conc = 0;
			g_sensor_telemetry.O2_raw = 0;
		} /* if o2_status */
	} else {
		g_sensor_telemetry.o2_sensor_valid = SENSOR_OFF;
		g_sensor_telemetry.O2_conc = 0;
		g_sensor_telemetry.O2_raw = 0;
	} /* if g_state_sensors_o2_enabled */

	/* If we are calibrating the O2 sensor then output values from dfrobot sensor if connected */
	if (calibrate_with_dfrobot_sensor) {
		short gas_temp;
		short gas_conc;
		if (dfr_gas_read(&gas_temp, &gas_conc) != EXIT_SUCCESS) {
			if (g_verbose)
				printf("Could not open DF Robot O2 Sensor\n");
		} else {
			printf("O2 Cal = %6.1f%%, Temperature = %6.2f°C\n", gas_conc/100.0, gas_temp/100.0);
			g_sensor_telemetry.O2_conc = gas_conc;
		}
	}

	/* Read the color sensor */
	if (g_state_sensors_color_enabled) {
		if (tcs_status) {
			RGB rgb=TCS34087_Get_RGBData();
			uint32_t RGB888=TCS34087_GetRGB888(rgb);
			uint16_t level = TCS34087_Get_Lux(rgb);

			if (g_verbose)
				printf("RGB888 :R=%d   G=%d  B=%d   RGB888=0X%X  C=%d LUX=%d\n", (RGB888>>16), \
						(RGB888>>8) & 0xff, (RGB888) & 0xff, RGB888, rgb.C,level);

			g_sensor_telemetry.light_level = level;
			g_sensor_telemetry.light_RGB = RGB888;
			g_sensor_telemetry.ColorValid = SENSOR_ON;
		} else {
			g_sensor_telemetry.ColorValid = SENSOR_ERR;
			g_sensor_telemetry.light_level = 0;
			g_sensor_telemetry.light_RGB = 0;
		}
	} else {
		g_sensor_telemetry.ColorValid = SENSOR_OFF;
		g_sensor_telemetry.light_level = 0;
		g_sensor_telemetry.light_RGB = 0;
	}
	return EXIT_SUCCESS;
}

/**
 * Standard algorithm for straight line interpolation
 * @param x - the key we want to find the value for
 * @param x0 - lower key
 * @param x1 - higher key
 * @param y0
 * @param y1
 * @return
 */
double linear_interpolation(double x, double x0, double x1, double y0, double y1) {
	double y = y0 + (y1 - y0) * ((x - x0)/(x1 - x0));
	return y;
}

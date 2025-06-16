/*
 ============================================================================
 Name        : sensors.c
 Author      : VE2TCP Chris Thompson g0kla@arrl.net
 Version     : 1.0
 Copyright   : Copyright 2024 ARISS
 Description : Student On Orbit Sensor System Main
 ============================================================================

 This program reads the Student On Orbit Sensor system and writes the data to two files.
 - The RT telemetry file, which contains one line of data and is overwritten with new data
 - The WOD telemetry file, which is appended until the file is rolled or a max size as
   a safety precaution.

 Fixed settings are stored in the config file
 Volatile settings are stored in the state file.  This file can be updated by iors_control
 based on commands from the ground, the UI or for other operational reasons.  This file is
 read each te;emtry cycle so that the latest changes are applied.

 The file names are in the config file and write data to a temporary file until we are
 ready to copy it to a final file.

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

#include "config.h"
#include "state_file.h"
#include "iors_log.h"
#include "iors_command.h"
#include "sensor_telemetry.h"
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

/* These global variables are in the config file */
char g_mic_serial_dev[MAX_FILE_PATH_LEN] = "/dev/serial0"; // device name for the serial port for ultrasonic mic
char g_cw1_serial_dev[MAX_FILE_PATH_LEN] = "/dev/serial1"; // device name for the serial port for cosmic watch
char g_cw2_serial_dev[MAX_FILE_PATH_LEN] = "/dev/serial2"; // device name for the serial port for cosmic watch
char g_rt_telem_path[MAX_FILE_PATH_LEN] = "rt_telemetry.dat";
char g_wod_telem_path[MAX_FILE_PATH_LEN] = "wod_telemetry.dat";
char g_cw1_log_path[MAX_FILE_PATH_LEN] = "cw1_log.dat";
char g_cw2_log_path[MAX_FILE_PATH_LEN] = "cw2_log.dat";
char g_mic_log_path[MAX_FILE_PATH_LEN] = "mic_log.dat";

/* These global variables are in the state file and are resaved when changed.  These default values are
 * overwritten when the state file is loaded */
int g_state_sensors_enabled = 1;
int g_state_period_to_send_telem_in_seconds = 360;
int g_state_period_to_store_wod_in_seconds = 60;
int g_wod_max_file_size = 200000; // bytes.  Note that WOD every min for a 128 byte layout gives 184320 bytes in 24 hours.  So keep layout under 128 bytes or wod frequency greater
int g_state_sensor_log_level = INFO_LOG;
int g_period_to_sample_telem_in_seconds = 30;

/* Forward functions */
int read_sensors(uint32_t now);
void help(void);
void signal_exit (int sig);
void signal_load_config (int sig);
double linear_interpolation(double x, double x0, double x1, double y0, double y1);

/* Local Variables */
char config_file_name[MAX_FILE_PATH_LEN] = "sensors.config";
char data_folder_path[MAX_FILE_PATH_LEN] = "./pacsat";
sensor_telemetry_t g_sensor_telemetry;
//int PERIOD=10;
//char filename[MAX_FILE_PATH_LEN];
int co2_status = false;
int o2_status = false;
int imu_status = false;
int tcs_status = false;
int calibrate_with_dfrobot_sensor = 0;

time_t last_time_checked_rt = 0;
time_t last_time_checked_wod = 0;

pthread_t cw1_listen_pthread = 0;
pthread_t cw2_listen_pthread = 0;


/* Temperature compensation table for O2 saensor */
#define O2_TEMPERATURE_TABLE_LEN 6
double o2_temp_table[O2_TEMPERATURE_TABLE_LEN][2] = {
		{0.0, 3.0}
		,{10.0, 1.0}
		,{20.0,0.0}
		,{30,-0.5}
		,{40.0,-1.0}
		,{50.0,-1.5}
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
			{NULL, 0, NULL, 0},
	};

	int more_help = false;

	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hd:c:tv", long_option, NULL)) < 0)
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
	load_state("sensors.state");

	char rt_telem_path[MAX_FILE_PATH_LEN];
	strlcpy(rt_telem_path, data_folder_path,MAX_FILE_PATH_LEN);
	strlcat(rt_telem_path,"/",MAX_FILE_PATH_LEN);
	strlcat(rt_telem_path,g_rt_telem_path,MAX_FILE_PATH_LEN);

	char wod_telem_path[MAX_FILE_PATH_LEN];
	strlcpy(wod_telem_path, data_folder_path,MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,"/",MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,get_folder_str(FolderWod),MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,"/",MAX_FILE_PATH_LEN);
	strlcat(wod_telem_path,g_wod_telem_path,MAX_FILE_PATH_LEN);

	char log_path[MAX_FILE_PATH_LEN];
	strlcpy(log_path, data_folder_path,MAX_FILE_PATH_LEN);
	strlcat(log_path,"/",MAX_FILE_PATH_LEN);
	strlcat(log_path,get_folder_str(FolderLog),MAX_FILE_PATH_LEN);

	log_init(get_log_name_str(LOG_NAME), log_path, g_log_filename);
	log_set_level(g_state_sensor_log_level);
	log_alog1(INFO_LOG, g_log_filename, ALOG_SENSORS_STARTUP, 0);

	if (strlen(g_rt_telem_path) == 0) {
		printf("ERROR: Telemetry filename required\n");
		return 1;
	}
	if (strlen(g_wod_telem_path) == 0) {
		printf("ERROR: WOD Telemetry filename required\n");
		return 2;
	}

	if (g_verbose) {
		printf("Student On Orbit Sensor System Telemetry Capture\n");
		printf("Build: %s\n", VERSION);
	}

	/* Setup the IMU.  Defaults are:
	 * 2g Accelerometer
	 * Gyro 32dps
	 * Mag is -4912 to 4912uT, for 2s complement 16 bit result */
	imu_status = imuInit();
	if (g_verbose)
		if (imu_status == false) printf("QMI8658_init fail\n");
	//	debug_print("IMU State: %d\n",g_imu_state);

	// TODO - this should come from command line so iors_control can activate it or not
    o2_status = true; // measure o2

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

	debug_print("Telem Length: %ld bytes\n", sizeof(g_sensor_telemetry));


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


	/* Now read the sensors until we get an interrupt to exit */
	while (1) {
		time_t now = time(0);
		read_sensors(now);

		if ((now - last_time_checked_rt) > g_state_period_to_send_telem_in_seconds) {
			last_time_checked_rt = now;

			uint8_t * data = (unsigned char *)&g_sensor_telemetry;
			FILE * outfile = fopen(tmp_filename, "wb");
			if (outfile != NULL) {
				/* Save the telemetry bytes */
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
						printf("ERROR, could not rename RT telem filename from: %s to: %s\n",tmp_filename, g_rt_telem_path);
				}
			} else {
				if (g_verbose)
					printf("ERROR, could not save data to filename: %s\n",g_rt_telem_path);
				//TODO - store error.  Repeating errors like this should go in the error count, otherwise they would fill the log.
			}
		}
		if ((now - last_time_checked_wod) > g_state_period_to_store_wod_in_seconds) {
			last_time_checked_wod = now;

			int rc = log_append(wod_telem_path,(unsigned char *)&g_sensor_telemetry, sizeof(g_sensor_telemetry));
			if (rc != EXIT_SUCCESS) {
				if (g_verbose)
					printf("ERROR, could not save data to filename: %s\n",g_wod_telem_path);
				//TODO - store error.  Repeating errors like this should go in the error count, otherwise they would fill the log.
			}
		}

		time_t time_after_read = time(0);
		int sleep_time = g_period_to_sample_telem_in_seconds - (time_after_read - now);
		if (sleep_time > 86400) /* Then something went wrong with the calculation or the clocks */
			sleep_time = g_period_to_sample_telem_in_seconds;
		if (sleep_time > 0) {
			if (g_verbose)
				printf("  Waiting %d seconds ...\n", sleep_time);
			sleep(sleep_time);
		}
	}
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
	if(g_verbose)
		printf (" Signal received, exiting ...\n");
	TCS34087_Close();
	lguSleep(2/1000);
	log_alog1(INFO_LOG, g_log_filename, ALOG_SENSORS_SHUTDOWN, 0);
	exit (0);
}

void signal_load_config (int sig) {
	load_config(config_file_name);
	load_state("sensors.state");
}

int read_sensors(uint32_t now) {
	g_sensor_telemetry.timestamp = now;
	/* Read the PI sensors */

//	mic_read_data(&sensor_telemetry);

	short val;
	int rc;
	rc = adc_read(ADC_METHANE_CHAN, &val);
	if (rc != EXIT_SUCCESS) {
		if (g_verbose)
			printf("Could not open MQ-6 Methane sensor ADC channel %d\n",ADC_METHANE_CHAN);
		g_sensor_telemetry.methane_conc = 0;
		g_sensor_telemetry.methane_sensor_valid = 0;
	} else {
		g_sensor_telemetry.methane_conc = val;
		g_sensor_telemetry.methane_sensor_valid = 1;
		if (g_verbose)
			printf("MQ-6 Methane: %d,",val);
	}

	rc = adc_read(ADC_AIR_QUALITY_CHAN, &val);
	if (rc != EXIT_SUCCESS) {
		if (g_verbose)
			printf("Could not open MQ-135 Air Quality ADC channel %d\n",ADC_AIR_QUALITY_CHAN);
		g_sensor_telemetry.air_quality = 0;
		g_sensor_telemetry.air_q_sensor_valid = 0;
	} else {
		if (g_verbose)
			printf("MQ-135 Air Q: %d\n",val);
		g_sensor_telemetry.air_quality = val;
		g_sensor_telemetry.air_q_sensor_valid = 1;
	}

	rc = adc_read(ADC_BUS_V_CHAN, &val);
	if (rc != EXIT_SUCCESS) {
		if (g_verbose)
			printf("Could not open Bus Voltage sensor ADC channel %d\n",ADC_BUS_V_CHAN);
	} else {
		g_sensor_telemetry.pi_bus_v = val;
		if (g_verbose)
			printf("PI Bus (5V): %0.0fmV,",2*val*0.125);
	}

	/* Read Waveshare C board sensors */
	/* Read the SHTC3 temp and humidity */
	short temperature, humidity;
	if (SHTC3_read(&temperature, &humidity) != EXIT_SUCCESS) {
		if (g_verbose)
			printf("Could not open SHTC3 Temperature sensor\n");
	} else {
		g_sensor_telemetry.SHTC3_temp = temperature;
		g_sensor_telemetry.SHTC3_humidity = humidity;

		float TH_Value, RH_Value;
		TH_Value = 175 * (float)temperature / 65536.0f - 45.0f; // Calculate temperature value
		RH_Value = 100 * (float)humidity / 65536.0f;         // Calculate humidity value
		if (g_verbose)
			printf("Temperature = %6.2f°C , Humidity = %6.2f%% \n", TH_Value, RH_Value);
	}

	/* Read the lps22 pressure sensor and its temperature */
	short lps22_temperature;
	int pressure;
	if (LPS22HB_read(&pressure, &lps22_temperature) != EXIT_SUCCESS) {
		if (g_verbose)
			printf("Could not open LPS22 Pressure sensor\n");
	} else {
		g_sensor_telemetry.LPS22_pressure = pressure;
		g_sensor_telemetry.LPS22_temp = lps22_temperature;
		if (g_verbose)
			printf("Pressure = %6.3f hPa, Temperature = %6.2f °C\n", pressure/4096.0, lps22_temperature/100.0);
	}

	/* Read the Gyroscope */
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
		g_sensor_telemetry.IHUTemp = QMI8658_readTemp();
	}

	/* Read the sound sensor */
	//TODO

	/* Read data from cosmic watches */
	//TODO

	/* Read the xensiv CO2 sensor */
	if (co2_status == true) {
		uint16_t co2_ppm_val;
		uint16_t pressure_ref = (uint16_t)(pressure/4096.0);
		if (xensiv_pasco2_read(pressure_ref, &co2_ppm_val) != XENSIV_PASCO2_READ_NRDY) {
			if (g_verbose)
				printf("CO2: %d ppm at %d hPa\n",co2_ppm_val, pressure_ref);
		} else {
			if (g_verbose)
				printf("CO2 Sensor not ready\n");
		}
	}

	/* Read the PS1 solid state O2 sensor.  Average 10 readings over 10 seconds */
	if (o2_status) {
		int c=0;
		float avg=0.0, max=0.0,min=65555;
		while (c < 10) {
			rc = adc_read(ADC_O2_CHAN, &val);
			if (rc != EXIT_SUCCESS) {
				if (g_verbose)
					printf("Could not open O2 Sensor ADC channel %d\n",ADC_O2_CHAN);
				break;
			} else {
				//					rttelemetry.BatteryV = val;
				//float volts = val * 0.125;
				//float o2_conc = -0.01805 * volts + 44.5835;
				//			printf("%.2f%% ..\n",o2_conc);
				//	if (g_verbose)
				//		printf("PS1 O2 Conc: %.2f%% %d(%0.0fmv)\n",o2_conc, val,(float)volts);
				if (val > max) max = val;
				if (val < min) min = val;
				avg+= val;
			}
			sleep(1);
			c++;
		}
		avg = avg / c;
		float volts = avg * 0.125;
		float o2_conc = -0.01805 * volts + 44.5835;
		//float o2_conc = -0.0103 * volts + 25.103;

		if (c > 0 && g_verbose) {
			/* Compensate for Temperature,  Look up temperature in table and interpolate the correction amount */
			int i = 0;
			double offset = 0.0;
			double first_key = 0;
			double last_key = 0;
			double first_value = 0;
			double last_value = 0;
			double temp = lps22_temperature/100.0;

			if (temp >= 0 && temp <= 50) {
				while (i++ < O2_TEMPERATURE_TABLE_LEN) {
					if (o2_temp_table[i][0] < temp) {
						first_key = o2_temp_table[i][0];
						first_value = o2_temp_table[i][1];
					}
					if (o2_temp_table[i][0] > temp) {
						last_key = o2_temp_table[i][0];
						last_value = o2_temp_table[i][1];
						break;
					}
				}
				offset = linear_interpolation(temp, first_key, last_key, first_value, last_value);

				//printf("Lookup: keys: %2.1f %2.1f compensate by: %2.3f\n",first_key, last_key, offset);
			}

			printf("PS1 O2 Conc: %.2f (%.2f) %d(%0.2fmv) max:%0.2f min:%0.2f\n",o2_conc + offset, o2_conc, val,(float)volts, max*0.125, min*0.125);
		}
		g_sensor_telemetry.O2_conc = (short)avg;
	}

	/* If we are calibrating the O2 sensor then output values from dfrobot sensor if connected */
	if (calibrate_with_dfrobot_sensor) {
		short gas_temp;
		short gas_conc;
		if (dfr_gas_read(&gas_temp, &gas_conc) != EXIT_SUCCESS) {
			if (g_verbose)
				printf("Could not open DF Robot O2 Sensor\n");
		}
	}

	/* Read the color sensor */
	if (tcs_status) {
		RGB rgb=TCS34087_Get_RGBData();
		uint32_t RGB888=TCS34087_GetRGB888(rgb);
		uint16_t level = TCS34087_Get_Lux(rgb);

		if (g_verbose)
			printf("RGB888 :R=%d   G=%d  B=%d   RGB888=0X%X  C=%d LUX=%d\n", (RGB888>>16), \
				(RGB888>>8) & 0xff, (RGB888) & 0xff, RGB888, rgb.C,level);

		g_sensor_telemetry.light_level = level;
        g_sensor_telemetry.light_RGB = RGB888;

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

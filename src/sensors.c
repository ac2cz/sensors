/*
 ============================================================================
 Name        : sensors.c
 Author      : VE2TCP Chris Thompson g0kla@arrl.net
 Version     : 1.0
 Copyright   : Copyright 2024 ARISS
 Description : Student On Orbit Sensor System Main
 ============================================================================

 This program reads the Student On Orbit Sensor system and writes the data to a file.
 The sample interval is supplied on the command line.
 The file name is supplied on the command line but data is written into a temporary
 file until we are ready to copy it to a final file.  By definition, this is a whole
 orbit data file because telemetry is collected while the program is running and saved
 in the background.

 The collection period for the file is also supplied or the file can be completed when
 a signal is received.  Completing the file involved renaming the temporary file to its
 final name.  Usually this will be in a queue folder for ingestion into the pacsat
 directory

 We can also supplu the name of the temporary file so that the calling program, most
 likely iors_control, can read the latest record from that file if it is sending
 real time telemetry of this type.

 All files should be raw bytes.

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "iors_log.h"
#include "sensor_telemetry.h"
#include "str_util.h"
#include "AD.h"
#include "LPS22HB.h"
#include "SHTC3.h"
#include "dfrobot_gas.h"
#include "xensiv_pasco2.h"
#include "IMU.h"

#define MAX_FILE_PATH_LEN 256
#define ADC_O2_CHAN 0
#define ADC_METHANE_CHAN 1
#define ADC_AIR_QUALITY_CHAN 2
#define ADC_BUS_V_CHAN 3

/* Forward functions */
int read_sensors(uint32_t now);
void help(void);
void signal_exit (int sig);
void signal_load_config (int sig);
double linear_interpolation(double x, double x0, double x1, double y0, double y1);

/* Variables */
static sensor_telemetry_t sensor_telemetry;
int PERIOD=10;
char filename[MAX_FILE_PATH_LEN];
int co2_status = false;
int imu_status = false;
int verbose = 1;
int calibrate_with_dfrobot_sensor = 0;
int o2_temp_table_len = 6;
double o2_temp_table[6][2] = {
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

	filename[0] = 0; // make sure string is empty and terminated

	struct option long_option[] = {
			{"help", no_argument, NULL, 'h'},
			{"period", required_argument, NULL, 'p'},
			{"filename", required_argument, NULL, 'f'},
			{"test", no_argument, NULL, 't'},
			{"verbose", no_argument, NULL, 'v'},
			{NULL, 0, NULL, 0},
	};

	int more_help = false;

	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "htvp:f:", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h': // help
			more_help = true;
			break;
		case 't': // calibrate
			calibrate_with_dfrobot_sensor = 1;
			break;
		case 'v': // verbose
			verbose = true;
			break;
		case 'p': // period
			int t = (int)strtol(optarg, NULL, 10);
			if (t > 0 && t < 86400) /* Set a sensible min max from 1 second to 24 hours */
				PERIOD = t;
			break;
		case 'f': // filename
			strlcpy(filename, optarg, sizeof(filename));
			break;


		default:
			break;
		}
	}

	if (more_help) {
		help();
		return 0;
	}

	if (strlen(filename) == 0) {
		printf("ERROR: Telemetry filename required with the -f paramaeter\n");
		help();
		return 1;
	}

	if (verbose) {
		puts("Student On Orbit Sensor System Telemetry Capture");
		printf("V1\n");
		printf("Period: %d File: %s\n",PERIOD, filename);
	}

	/* Setup the IMU.  Defaults are:
	 * 2g Accelerometer
	 * Gyro 32dps
	 * Mag is -4912 to 4912uT, for 2s complement 16 bit result */
	imu_status = imuInit();
	//	debug_print("IMU State: %d\n",g_imu_state);

	int res = xensiv_pasco2_init();
	if (res == EXIT_SUCCESS) {
		co2_status = true;
	} else {
		if (verbose)
			printf("Could not open CO2 gas sensor: %d\n",res);
	}
	while (1) {
		time_t now = time(0);
		read_sensors(now);
		time_t time_after_read = time(0);
		int sleep_time = PERIOD - (time_after_read - now);
		if (sleep_time > 86400) /* Then something went wrong with the calculation or the clocks */
			sleep_time = PERIOD;
		if (sleep_time > 0) {
			if (verbose)
				printf("Waiting %d seconds ...\n", sleep_time);
			sleep(sleep_time);
		}
		int rc = log_append(filename,(unsigned char *)&sensor_telemetry, sizeof(sensor_telemetry));
		if (rc != EXIT_SUCCESS) {
			if (verbose)
				printf("ERROR, could not save data to filename: %s\n",filename);
			//TODO - store error.  Repeating errors like this should go in the error count, otherwise they would fill the log.
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
			"-d,--dir                         use this data directory, rather than default\n"
			"-t,--test                        provide readings from additional calibration sensor\n"
			"-v,--verbose                     print additional status and progress messages\n"
	);
	exit(EXIT_SUCCESS);
}


void signal_exit (int sig) {
	if(verbose)
		printf (" Signal received, exiting ...\n");
	exit (0);
}

void signal_load_config (int sig) {
	if (verbose)
		printf ("ERROR: Signal received, updating config not yet implemented...\n");
	// TODO SIHUP should reload the config perhaps
}

int read_sensors(uint32_t now) {
	sensor_telemetry.timestamp = now;
	/* Read the PI sensors */
	short val;
	int rc;
	rc = adc_read(ADC_METHANE_CHAN, &val);
	if (rc != EXIT_SUCCESS) {
		if (verbose)
			printf("Could not open MQ-6 Methane sensor ADC channel %d\n",ADC_METHANE_CHAN);
	} else {
		sensor_telemetry.methane_conc = val;
		if (verbose)
			printf("MQ-6 Methane: %d,",val);
	}

	rc = adc_read(ADC_AIR_QUALITY_CHAN, &val);
	if (rc != EXIT_SUCCESS) {
		if (verbose)
			printf("Could not open MQ-135 Air Quality ADC channel %d\n",ADC_AIR_QUALITY_CHAN);
	} else {
		if (verbose)
			printf("MQ-135 Air Q: %d\n",val);
	}

	rc = adc_read(ADC_BUS_V_CHAN, &val);
	if (rc != EXIT_SUCCESS) {
		if (verbose)
			printf("Could not open Bus Voltasge sensor ADC channel %d\n",ADC_BUS_V_CHAN);
	} else {
		sensor_telemetry.pi_bus_v = val;
		if (verbose)
			printf("PI Bus (5V): %0.0fmV,",2*val*0.125);
		//			printf("Bus Voltage = %d(%0.0fmv)\n",rttelemetry.BatteryV,(float)2*rttelemetry.BatteryV*0.125);
	}

	/* Read Waveshare C board sensors */
	/* Read the SHTC3 temp and humidity */
	short temperature, humidity;
	if (SHTC3_read(&temperature, &humidity) != EXIT_SUCCESS) {
		if (verbose)
			printf("Could not open SHTC3 Temperature sensor\n");
	} else {
		sensor_telemetry.SHTC3_temp = temperature;
		sensor_telemetry.SHTC3_humidity = humidity;

		float TH_Value, RH_Value;
		TH_Value = 175 * (float)temperature / 65536.0f - 45.0f; // Calculate temperature value
		RH_Value = 100 * (float)humidity / 65536.0f;         // Calculate humidity value
		if (verbose)
			printf("Temperature = %6.2f°C , Humidity = %6.2f%% \n", TH_Value, RH_Value);
	}

	/* Read the lps22 pressure sensor and its temperature */
	short lps22_temperature;
	int pressure;
	if (LPS22HB_read(&pressure, &lps22_temperature) != EXIT_SUCCESS) {
		if (verbose)
			printf("Could not open LPS22 Pressure sensor\n");
	} else {
		sensor_telemetry.LPS22_pressure = pressure;
		sensor_telemetry.LPS22_temp = lps22_temperature;
		if (verbose)
			printf("Temperature = %6.2f °C, Pressure = %6.2f hPa\n", lps22_temperature/100.0, pressure/4096.0);
	}

	/* Read the color sensor */
	//TODO

	/* Read the Gyroscope */
	if (imu_status) {
		IMU_ST_SENSOR_DATA stGyroRawData;
		IMU_ST_SENSOR_DATA stAccelRawData;
		IMU_ST_SENSOR_DATA stMagnRawData;

		imuDataGetRaw(&stGyroRawData, &stAccelRawData, &stMagnRawData);
		if (verbose) {
			printf("Acceleration: X: %d     Y: %d     Z: %d \n",stAccelRawData.s16X, stAccelRawData.s16Y, stAccelRawData.s16Z);
			printf("Gyroscope: X: %d     Y: %d     Z: %d \n",stGyroRawData.s16X, stGyroRawData.s16Y, stGyroRawData.s16Z);
			printf("Magnetic: X: %d     Y: %d     Z: %d \n",stMagnRawData.s16X, stMagnRawData.s16Y, stMagnRawData.s16Z);
		}
		sensor_telemetry.AccelerationX = stAccelRawData.s16X;
		sensor_telemetry.AccelerationY = stAccelRawData.s16Y;
		sensor_telemetry.AccelerationZ = stAccelRawData.s16Z;
		sensor_telemetry.GyroX = stGyroRawData.s16X;
		sensor_telemetry.GyroY = stGyroRawData.s16Y;
		sensor_telemetry.GyroZ = stGyroRawData.s16Z;
		sensor_telemetry.MagX = stMagnRawData.s16X;
		sensor_telemetry.MagY = stMagnRawData.s16Y;
		sensor_telemetry.MagZ = stMagnRawData.s16Z;
		sensor_telemetry.IHUTemp = QMI8658_readTemp();
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
			if (verbose)
				printf("CO2: %d ppm at %d hPa\n",co2_ppm_val, pressure_ref);
		} else {
			if (verbose)
				printf("CO2 Sensor not ready\n");
		}
	}

	/* Read the PS1 solid state O2 sensor.  Average 10 readings over 10 seconds */
	int c=0;
	float avg=0.0, max=0.0,min=65555;
	while (c < 10) {
		rc = adc_read(ADC_O2_CHAN, &val);
		if (rc != EXIT_SUCCESS) {
			if (verbose)
				printf("Could not open O2 Sensor ADC channel %d\n",ADC_O2_CHAN);
		} else {
			//					rttelemetry.BatteryV = val;
			float volts = val * 0.125;
			//float o2_conc = -0.09 * volts + 222.3;
			float o2_conc = -0.01805 * volts + 44.5835;
			//float o2_conc = -0.0103 * volts + 25.103;
			//			printf("%.2f%% ..\n",o2_conc);
			if (verbose)
				printf("PS1 O2 Conc: %.2f%% %d(%0.0fmv)\n",o2_conc, val,(float)volts);
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

	if (verbose) {
		/* Compensate for Temperature,  Look up temperature in table and interpolate the correction amount */
		int i = 0;
		double offset = 0.0;
		double first_key = 0;
		double last_key = 0;
		double first_value = 0;
		double last_value = 0;
		double temp = lps22_temperature/100.0;

		if (temp >= 0 && temp <= 50) {
			while (i++ < o2_temp_table_len) {
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
	sensor_telemetry.O2_conc = (short)avg;

	/* If we are calibrating the O2 sensor then output values from dfrobot sensor if connected */
	if (calibrate_with_dfrobot_sensor) {
		short gas_temp;
		short gas_conc;
		if (dfr_gas_read(&gas_temp, &gas_conc) != EXIT_SUCCESS) {
			if (verbose)
				printf("Could not open DF Robot O2 Sensor\n");
		}
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

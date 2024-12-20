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
#include "rttelemetry.h"
#include "AD.h"
#include "LPS22HB.h"
#include "SHTC3.h"
#include "dfrobot_gas.h"
#include "xensiv_pasco2.h"

#define ADC_O2_CHAN 0
#define ADC_LDR_CHAN 1
#define ADC_BUS_V_CHAN 3

/* Variables */
static rttelemetry_t rttelemetry;
int PERIOD=1;


int main(void) {
	puts("Student On Orbit Sensor System telemetry capture");

	int co2_status = false;
	int res = xensiv_pasco2_init();
	if (res == EXIT_SUCCESS) {
		co2_status = true;
	} else {
		printf("Could not open CO2 gas sensor: %d\n",res);
	}

	while(1) {
		/* Read the PI sensors */
		int val;
		int rc = adc_read(ADC_BUS_V_CHAN, &val);
		if (rc != EXIT_SUCCESS) {
			printf("Could not open ADC channel %d\n",ADC_BUS_V_CHAN);
		} else {
			rttelemetry.BatteryV = val;
			printf("Bus Voltage = %d(%0.0fmv)\n",rttelemetry.BatteryV,(float)2*rttelemetry.BatteryV*0.125);
		}

		rc = adc_read(ADC_LDR_CHAN, &val);
		if (rc != EXIT_SUCCESS) {
			printf("Could not open ADC channel %d\n",ADC_LDR_CHAN);
		} else {
//			rttelemetry.BatteryV = val;
			printf("Photoresistor = %d(%0.0fmv)\n",val,(float)val*0.125);
		}

		int c=0;
		float avg=0.0, max=0.0,min=65555;
		while (c < 12) {
			rc = adc_read(ADC_O2_CHAN, &val);
			if (rc != EXIT_SUCCESS) {
				printf("Could not open ADC channel %d\n",ADC_O2_CHAN);
			} else {
				//					rttelemetry.BatteryV = val;
//y = -0.018x + 44.46
			float volts = val * 0.125;
		    	//float o2_conc = -0.09 * volts + 222.3;
		    	float o2_conc = -0.01805 * volts + 44.5835;
//			printf("%.2f%% ..\n",o2_conc);
		printf("PS1 O2 Conc: %.2f%% %d(%0.0fmv)\n",o2_conc, val,(float)volts);
			if (val > max) max = val;
			if (val < min) min = val;
			avg+= val;
			}
			sleep(5);
                        c++;
		}
		avg = avg / c;
		float volts = val * 0.125;
		float o2_conc = -0.01805 * volts + 44.5835;
		printf("PS1 O2 Conc: %.2f%% %d(%0.2fmv) max:%0.2f min:%0.2f\n",o2_conc, val,(float)volts, max*0.125, min*0.125);

		short temperature, humidity;
		if (SHTC3_read(&temperature, &humidity) != EXIT_SUCCESS) {
			printf("Could not open SHTC3 Temperature sensor\n");
		} else {
			rttelemetry.SHTC3_temp = temperature;
			rttelemetry.SHTC3_humidity = humidity;

			float TH_Value, RH_Value;
			TH_Value = 175 * (float)temperature / 65536.0f - 45.0f; // Calculate temperature value
			RH_Value = 100 * (float)humidity / 65536.0f;         // Calculate humidity value
			printf("Temperature = %6.2f°C , Humidity = %6.2f%% \n", TH_Value, RH_Value);
		}
		short lps22_temperature;
		int pressure;
		if (LPS22HB_read(&pressure, &lps22_temperature) != EXIT_SUCCESS) {
			printf("Could not open LPS22 Pressure sensor\n");
		} else {
			rttelemetry.LPS22_pressure = pressure;
			rttelemetry.LPS22_temp = lps22_temperature;
			printf("Temperature = %6.2f °C, Pressure = %6.2f hPa\n", lps22_temperature/100.0, pressure/4096.0);
		}
		short gas_temp;
		short gas_conc;
		if (dfr_gas_read(&gas_temp, &gas_conc) != EXIT_SUCCESS) {
			printf("Could not open DF Robot O2 Sensor\n");
		} else {

			//	float gas_temp_value, CONC_Value;
			//
			//	gas_temp_value = 175 * (float)TH_DATA / 65536.0f - 45.0f; // Calculate temperature value
			//	CONC_Value = 100 * (float)CONC_DATA / 65536.0f;         // Calculate gas value
			//	debug_print("Temperature = %6.2f°C , Humidity = %6.2f%% \r\n", gas_temp_value, CONC_Value);
		}

		if (co2_status == true) {
			uint16_t co2_ppm_val;
			uint16_t pressure_ref = (uint16_t)(pressure/4096.0);
			if (xensiv_pasco2_read(pressure_ref, &co2_ppm_val) != XENSIV_PASCO2_READ_NRDY) {
				printf("CO2: %d ppm at %d hPa\n",co2_ppm_val, pressure_ref);
			} else {
				printf("CO2 Sensor not ready\n");
			}
		}

		sleep(PERIOD);
	}

	return EXIT_SUCCESS;
}

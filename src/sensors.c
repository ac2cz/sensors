/*
 ============================================================================
 Name        : sensors.c
 Author      : VE2TCP Chris Thompson g0kla@arrl.net
 Version     : 1.0
 Copyright   : Copyright 2024 ARISS
 Description : Student On Orbit Sensor System Main
 ============================================================================
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

/* Variables */
static rttelemetry_t rttelemetry;


int main(void) {
	puts("Student On Orbit Sensor System telemetry capture");

	while(1) {
		/* Read the PI sensors */
		int val;
		int rc = adc_read(3, &val);
		if (rc != EXIT_SUCCESS) {
			printf("Could not open ADC\n");
		} else {
			rttelemetry.BatteryV = val;
			printf("Battery V = %d(%0.2fmv)\n",rttelemetry.BatteryV,(float)rttelemetry.BatteryV*0.125);
		}
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

		int res = xensiv_pasco2_init();
		if (res != EXIT_SUCCESS) {
			printf("Could not open CO2 gas sensor: %d\n",res);
		} else {
			uint16_t co2_ppm_val;
			xensiv_pasco2_read(0, &co2_ppm_val);
			printf("CO2: %d ppm\n",co2_ppm_val);
		}

		sleep(60);
	}

	return EXIT_SUCCESS;
}

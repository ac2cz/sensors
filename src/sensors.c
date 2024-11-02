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
#include "rttelemetry.h"
#include "AD.h"
#include "LPS22HB.h"
#include "SHTC3.h"
#include "dfrobot_gas.h"

/* Variables */
static rttelemetry_t rttelemetry;


int main(void) {
	puts("Student On Orbit Sensor System telemetry capture");
	/* Read the PI sensors */
	rttelemetry.BatteryV = adc_read();

	printf("Battery V = %d(%0.2fmv)\n\r",rttelemetry.BatteryV,(float)rttelemetry.BatteryV*0.125);

	short temperature, humidity;
	SHTC3_read(&temperature, &humidity);
	rttelemetry.SHTC3_temp = temperature;
	rttelemetry.SHTC3_humidity = humidity;

	float TH_Value, RH_Value;
	TH_Value = 175 * (float)temperature / 65536.0f - 45.0f; // Calculate temperature value
	RH_Value = 100 * (float)humidity / 65536.0f;         // Calculate humidity value
	printf("Temperature = %6.2f°C , Humidity = %6.2f%% \n", TH_Value, RH_Value);

	short lps22_temperature;
	int pressure;
	LPS22HB_read(&pressure, &lps22_temperature);
	rttelemetry.LPS22_pressure = pressure;
	rttelemetry.LPS22_temp = lps22_temperature;

	printf("Temperature = %6.2f °C, Pressure = %6.2f hPa\n", pressure/4096.0, lps22_temperature/100.0);

	short gas_temp;
	short gas_conc;
	dfr_gas_read(&gas_temp, &gas_conc);

//	float gas_temp_value, CONC_Value;
//
//	gas_temp_value = 175 * (float)TH_DATA / 65536.0f - 45.0f; // Calculate temperature value
//	CONC_Value = 100 * (float)CONC_DATA / 65536.0f;         // Calculate gas value
//	debug_print("Temperature = %6.2f°C , Humidity = %6.2f%% \r\n", gas_temp_value, CONC_Value);


	return EXIT_SUCCESS;
}

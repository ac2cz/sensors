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

/* Variables */
static rttelemetry_t rttelemetry;


int main(void) {
	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
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
		printf("Temperature = %6.2f°C , Humidity = %6.2f%% \r\n", TH_Value, RH_Value);

		short lps22_temperature;
		int pressure;
		LPS22HB_read(&pressure, &lps22_temperature);
		rttelemetry.LPS22_pressure = pressure;
		rttelemetry.LPS22_temp = lps22_temperature;

		printf("Pressure = %6.2f hPa , Temperature = %6.2f °C\r\n", pressure/4096.0, lps22_temperature/100.0);

	return EXIT_SUCCESS;
}

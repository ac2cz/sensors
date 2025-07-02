/*
 * sensors_gpio.c
 *
 *  Created on: Jul 2, 2025
 *      Author: g0kla
 */
#include <stdlib.h>
#include <stdio.h>
#include <lgpio.h>

#include "debug.h"
#include "sensors_gpio.h"

static int hd;

/**
 * Open the GPIO chip and claim the pins we need.  Set them to have a pull up resistor and set all levels to 0
 * GPIO Line Flags are one of:
   LG_SET_ACTIVE_LOW
   LG_SET_OPEN_DRAIN
   LG_SET_OPEN_SOURCE
   LG_SET_PULL_UP
   LG_SET_PULL_DOWN
   LG_SET_PULL_NONE
 */
int sensors_gpio_init() {
	hd = lgGpiochipOpen(GPIO_DEV);
	if (hd >= 0) {

		if (lgGpioClaimOutput(hd, LG_SET_PULL_DOWN, SENSORS_GPIO_CO2_EN, 0) != EXIT_SUCCESS) {
			debug_print("ERROR: Could not claim CO2 en pin %d\n",SENSORS_GPIO_CO2_EN);
			sensors_gpio_close();
			return EXIT_FAILURE;
		}
		if (lgGpioClaimOutput(hd, LG_SET_PULL_DOWN, SENSORS_GPIO_MQ6_EN, 0) != EXIT_SUCCESS) {
			debug_print("ERROR: Could not claim MQ6 en pin %d\n",SENSORS_GPIO_MQ6_EN);
			sensors_gpio_close();
			return EXIT_FAILURE;
		}
		if (lgGpioClaimOutput(hd, LG_SET_PULL_DOWN, SENSORS_GPIO_MQ135_EN, 0) != EXIT_SUCCESS) {
			debug_print("ERROR: Could not claim MQ135 en pin %d\n",SENSORS_GPIO_MQ135_EN);
			sensors_gpio_close();
			return EXIT_FAILURE;
		}

		lgGpioWrite(hd, SENSORS_GPIO_CO2_EN, 0);
		lgGpioWrite(hd, SENSORS_GPIO_MQ6_EN, 0);
		lgGpioWrite(hd, SENSORS_GPIO_MQ135_EN, 0);

		return hd;
	} else {
		debug_print("ERROR: Could not open Sensors GPIO chip\n");
		return -1;
	}
}

void sensors_gpio_close() {
	if (hd >= 0)
		lgGpiochipClose(hd);
	hd = -1;
}

/*
 * sensors_gpio.h
 *
 *  Created on: Jul 2, 2025
 *      Author: g0kla
 */

#ifndef SENSORS_GPIO_H_
#define SENSORS_GPIO_H_

#define GPIO_DEV 0

#define SENSORS_GPIO_CO2_EN 17
#define SENSORS_GPIO_MQ6_EN 23
#define SENSORS_GPIO_MQ135_EN 24

int sensors_gpio_init();
void sensors_gpio_close();

#endif /* SENSORS_GPIO_H_ */

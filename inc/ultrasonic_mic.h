/*
 * ultrasonic_mic.h
 *
 *  Created on: Jun 9, 2025
 *      Author: g0kla
 */

#ifndef ULTRASONIC_MIC_H_
#define ULTRASONIC_MIC_H_

#define RESPONSE_LEN 256

#include "sensor_telemetry.h"

int mic_read_data(sensor_telemetry_t *sensor_telemetry);


#endif /* ULTRASONIC_MIC_H_ */

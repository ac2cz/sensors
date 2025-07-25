/*
 * ultrasonic_mic.h
 *
 *  Created on: Jun 9, 2025
 *      Author: g0kla
 */

#ifndef ULTRASONIC_MIC_H_
#define ULTRASONIC_MIC_H_

#define MIC_RESPONSE_LEN 256

#include "sensor_telemetry.h"

//typedef struct mic_data {
//	unsigned char sound_psd[32];
//    unsigned int max_sound_level : 8;
//    unsigned int max_sound_bin : 8;
//    unsigned int mic_valid : 1;
//} mic_data_t;

void mic_read_data();

#endif /* ULTRASONIC_MIC_H_ */

/*
 * cosmic_watch.h
 *
 *  Created on: Jun 10, 2025
 *      Author: g0kla
 */

#ifndef COSMIC_WATCH_H_
#define COSMIC_WATCH_H_

#include <stdint.h>
#include <pthread.h>

#define CW_RESPONSE_LEN 1024

/* This is defined in cosmic_watch.c and used in sensors.c as well */
extern pthread_mutex_t cw_mutex;

typedef struct cw_data {
	char master_slave[2];
    uint16_t event_num; /* The event number */
    uint32_t time_ms;  /* Time in ms since we started.  Used for coincidence and later analysis. */
    uint16_t count_avg;  /* The running average of counts since we started this run */
   // float count_std; /* The standard deviation of counts since we started this run */
    float sipm_voltage; /* The voltage from the scintilator block, scaled from the ADC reading with a lookup table */
    uint32_t deadtime_ms; /* The deadtime */
    float temperature_deg_c; /* The temperature in degrees C */
} cw_data_t;

void *cw1_listen_process(void * arg);
void *cw2_listen_process(void * arg);

#endif /* COSMIC_WATCH_H_ */

/*
 * xensiv_pasco2.c
 *
 *  Created on: Nov 3, 2024
 *      Author: g0kla
 *
 *  Ported from xensiv_pasco2.c to use lgpio library
 *  https://github.com/Infineon/sensor-xensiv-pasco2/blob/master/xensiv_pasco2.c
 *
 */

/***********************************************************************************************//**
 * \file xensiv_pasco2.c
 *
 * Description: This file contains the functions for interacting with the
 *              XENSIV™ PAS CO2 sensor.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2021-2022 Infineon Technologies AG
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>
#include <lgpio.h>
#include "xensiv_pasco2.h"

#define XENSIV_PASCO2_COMM_DELAY_MS             (5U)
#define XENSIV_PASCO2_COMM_TEST_VAL             (0xA5U)

#define XENSIV_PASCO2_SOFT_RESET_DELAY_MS       (2000U)

#define XENSIV_PASCO2_FCS_MEAS_RATE_S           (10)

#define XENSIV_PASCO2_I2C_WRITE_BUFFER_LEN      (17U)
#define XENSIV_PASCO2_UART_WRITE_XFER_BUF_SIZE  (8U)
#define XENSIV_PASCO2_UART_READ_XFER_BUF_SIZE   (5U)

#define XENSIV_PASCO2_UART_WRITE_XFER_RESP_LEN  (2U)
#define XENSIV_PASCO2_UART_READ_XFER_RESP_LEN   (3U)
#define XENSIV_PASCO2_UART_ACK                  (0x06U)
#define XENSIV_PASCO2_UART_NAK                  (0x15U)

int32_t xensiv_pasco2_cmd(int dev, xensiv_pasco2_cmd_t cmd) {
    return lgI2cWriteByteData(dev, (uint8_t)XENSIV_PASCO2_REG_SENS_RST, cmd);
}

int32_t xensiv_pasco2_start_single_mode(int dev) {
    xensiv_pasco2_measurement_config_t meas_config;
    /* Get measurement Config */
    int32_t res = XENSIV_PASCO2_OK; 
    int32_t count = lgI2cReadI2CBlockData(dev, (uint8_t)XENSIV_PASCO2_REG_MEAS_CFG, (char *)&(meas_config.u), 1U);

    if (count == 1) {
	   //printf("CO2 Sensor read measurement reg\n");
        if (meas_config.b.op_mode != XENSIV_PASCO2_OP_MODE_IDLE) {
	   printf("CO2 Sensor not set to op mode idle\n");
            meas_config.b.op_mode = XENSIV_PASCO2_OP_MODE_IDLE;
            /* Set measurement congfig */
            res = lgI2cWriteI2CBlockData(dev, (uint8_t)XENSIV_PASCO2_REG_MEAS_CFG, (char *)&(meas_config.u), 1U);
        }
    }

    if (XENSIV_PASCO2_OK == res)
    {
        meas_config.b.op_mode = XENSIV_PASCO2_OP_MODE_SINGLE;
        meas_config.b.boc_cfg = XENSIV_PASCO2_BOC_CFG_AUTOMATIC;
	   //printf("CO2 Sensor writing single mode\n");
        res = lgI2cWriteI2CBlockData(dev, (uint8_t)XENSIV_PASCO2_REG_MEAS_CFG, (char *)&(meas_config.u), 1U);
    }

    return res;
}

int xensiv_pasco2_init() {
	int xensiv_pasco2_fd = -1;
	xensiv_pasco2_fd = lgI2cOpen(1, XENSIV_PASCO2_I2C_ADDR, 0);
	if (xensiv_pasco2_fd < 0)
		return EXIT_FAILURE;
	 /* Check communication */
	    uint8_t data = XENSIV_PASCO2_COMM_TEST_VAL;

        int res = lgI2cWriteI2CBlockData(xensiv_pasco2_fd, (uint8_t)XENSIV_PASCO2_REG_SCRATCH_PAD, (char *)&data, 1U);

	    if (XENSIV_PASCO2_OK == res){
	        lgI2cReadI2CBlockData(xensiv_pasco2_fd, (uint8_t)XENSIV_PASCO2_REG_SCRATCH_PAD, (char *)&data, 1U);
	    }
	    if ((XENSIV_PASCO2_OK == res) && (XENSIV_PASCO2_COMM_TEST_VAL == data)) {
	    	//printf("CO2 Sensor Scratch Read OK\n");
	    	/* Soft reset */
	    	res = xensiv_pasco2_cmd(xensiv_pasco2_fd, XENSIV_PASCO2_CMD_SOFT_RESET);
	    	lguSleep(XENSIV_PASCO2_SOFT_RESET_DELAY_MS/1000.0);

	    	if (XENSIV_PASCO2_OK == res) {
	    		//printf("CO2 Sensor Reset OK\n");
	    		/* Read the sensor status and verify if the sensor is ready */
	    		res = lgI2cReadI2CBlockData(xensiv_pasco2_fd, (uint8_t)XENSIV_PASCO2_REG_SENS_STS, (char *)&data, 1U);
                        printf("CO2 Sensor Status: %0x\n",data);
	    	}
	    	if (data != 0xC0) {
	    		if ((data & XENSIV_PASCO2_REG_SENS_STS_ICCER_MSK) != 0U) {
	    	                printf("CO2 Sensor ICCERR\n");
	    			res = XENSIV_PASCO2_ICCERR;
	    		}
	    		else if ((data & XENSIV_PASCO2_REG_SENS_STS_ORVS_MSK) != 0U) {
	    	                printf("CO2 Sensor ORVS\n");
	    			res = XENSIV_PASCO2_ORVS;
	    		}
	    		else if ((data & XENSIV_PASCO2_REG_SENS_STS_ORTMP_MSK) != 0U) {
	    	                printf("CO2 Sensor ORTMP\n");
	    			res = XENSIV_PASCO2_ORTMP;
	    		}
	    		else if ((data & XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_MSK) == 0U) {
	    	                printf("CO2 Sensor NOT READY\n");
	    			res = XENSIV_PASCO2_ERR_NOT_READY;
	    		}
	 	} else {
	            //printf("CO2 Sensor Status Read OK\n");
	        	res = XENSIV_PASCO2_OK;
	        }
	  
	    } else {
	    	printf("CO2 Sensor FAIL\n");
	    	res = XENSIV_PASCO2_ERR_COMM;
	    }

	    lgI2cClose(xensiv_pasco2_fd);
	    return res;
}

//int32_t xensiv_pasco2_set_pressure_compensation(int dev, uint16_t val) {
//
//    val = (uint16_t)htons(val);
//    return lgI2cWriteI2CBlockData(dev, (uint8_t)XENSIV_PASCO2_REG_PRESS_REF_H, (char *)&val, 2U);
//}
//
int32_t xensiv_pasco2_get_result(int dev, uint16_t * val) {
    xensiv_pasco2_meas_status_t meas_status;
    int32_t res = EXIT_SUCCESS;
    /* Get measurement status */
    int32_t count = lgI2cReadI2CBlockData(dev, (uint8_t)XENSIV_PASCO2_REG_MEAS_STS, (char *)&(meas_status), 1U);

    if (count == 1) {
        if (meas_status.b.drdy != 0U) {
            count = lgI2cReadI2CBlockData(dev, (uint8_t)XENSIV_PASCO2_REG_CO2PPM_H, (char *)val, 2U);
            *val = htons(*val);
        }
        else {
            res =  XENSIV_PASCO2_READ_NRDY;
        }
    }

    return res;
}

int xensiv_pasco2_read(uint16_t press_ref, uint16_t * co2_ppm_val) {
	int xensiv_pasco2_fd = -1;
	xensiv_pasco2_fd = lgI2cOpen(1, XENSIV_PASCO2_I2C_ADDR, 0);
	if (xensiv_pasco2_fd < 0)
		return EXIT_FAILURE;
	int32_t res = XENSIV_PASCO2_OK;
	//printf("CO2 Sensor Start Single Mode\n");
	res = xensiv_pasco2_start_single_mode(xensiv_pasco2_fd);
        /* Wait at least 1 second for sensor to measure conc */
	lguSleep(1.2);
        
		//if (press_ref != 0)
		//	res = xensiv_pasco2_set_pressure_compensation(xensiv_pasco2_fd, press_ref);
	if (XENSIV_PASCO2_OK == res) {
		res = xensiv_pasco2_get_result(xensiv_pasco2_fd, co2_ppm_val);
	}
	lgI2cClose(xensiv_pasco2_fd);

    return res;
}

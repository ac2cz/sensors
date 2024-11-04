/*
 * xensiv_pasco2.c
 *
 *  Created on: Nov 3, 2024
 *      Author: g0kla
 *
 *  Ported from xensiv_pasco2.c to use lgpio library
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
#include <assert.h>
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
	    	printf("CO2 Sensor OK\n");
	    } else {
	    	printf("CO2 Sensor FAIL\n");
	    	res = XENSIV_PASCO2_ERR_COMM;
	    }

	    lgI2cClose(xensiv_pasco2_fd);
	    return res;
}

/*
 * dfrobot_gas.h
 *
 *  Created on: Nov 1, 2024
 *      Author: g0kla
 */

#ifndef DFROBOT_GAS_H_
#define DFROBOT_GAS_H_

#include <stdlib.h>
#include <stdint.h>

#define DFR_GAS_I2C_ADDR 0x74

#define CMD_CHANGE_GET_METHOD          0X78
#define CMD_GET_GAS_CONCENTRATION      0X86
#define CMD_GET_TEMP                   0X87
#define CMD_GET_ALL_DTTA               0X88
#define CMD_SET_THRESHOLD_ALARMS       0X89
#define CMD_IIC_AVAILABLE              0X90
#define CMD_SENSOR_VOLTAGE             0X91
#define CMD_CHANGE_IIC_ADDR            0X92

/**
 * @struct sProtocol_t
 * @brief Data protocol package for communication
 */
typedef struct
{
  uint8_t head;
  uint8_t addr;
  uint8_t data[6];
  uint8_t check;
} sProtocol_t;

/**
 * @enum eSwitch_t
 * @brief Whether to enable ALA alarm function
 */
typedef enum
{
  ON = 0x01,
  OFF = 0x00
} eSwitch_t;

/**
 * @enum eType_t
 * @brief Gas Type
 */
typedef enum
{
  O2 = 0x05,
  CO = 0x04,
  H2S = 0x03,
  NO2 = 0x2C,
  O3 = 0x2A,
  CL2 = 0x31,
  NH3 = 0x02,
  H2 = 0x06,
  HCL = 0X2E,
  SO2 = 0X2B,
  HF = 0x33,
  _PH3 = 0x45
} eType_t;

int dfr_gas_read(short *temp, short *conc);

#endif /* DFROBOT_GAS_H_ */

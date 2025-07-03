/*
 * dfrobot_gas.c
 *
 *  Created on: Nov 1, 2024
 *      Author: g0kla
 */
/*
 * Portions of this code ported from from:
  * @file  DFRobot_MultiGasSensor.cpp
  * @brief This is function implementation .cpp file of a library for the sensor that can detect gas concentration in the air.
  * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
  * @license     The MIT License (MIT)
  * @author      PengKaixing(kaixing.peng@dfrobot.com)
  * @version     V2.0.0
  * @date        2021-09-26
  * @url         https://github.com/DFRobot/DFRobot_MultiGasSensor
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lgpio.h>
#include <math.h>
#include "dfrobot_gas.h"

//unsigned short TH_DATA, CONC_DATA;
int dfr_gas_fd = -1;
int _tempswitch = 0;

int dfr_write_command(unsigned short cmd) {
  char buf[] = {(cmd >> 8), cmd};
  return lgI2cWriteByteData(dfr_gas_fd, buf[0], buf[1]);
  // 1:error 0:No error
}

static uint8_t FucCheckSum(uint8_t* i,uint8_t ln)
{
  uint8_t j,tempq=0;
  i+=1;
  for(j=0;j<(ln-2);j++)
  {
    tempq+=*i;i++;
  }
  tempq=(~tempq)+1;
  return(tempq);
}

sProtocol_t pack(uint8_t *pBuf, uint8_t len) {
  sProtocol_t _protocol;
  _protocol.head = 0xff;
  _protocol.addr = 0x01;
  memcpy(_protocol.data, pBuf, len);
  _protocol.check = FucCheckSum((uint8_t *)&_protocol, 8);
  return _protocol;
}

float readTempC(void) {
  uint8_t buf[6] = {0};
  uint8_t recvbuf[9] = {0};
  buf[0] = CMD_GET_TEMP;
  sProtocol_t _protocol = pack(buf, sizeof(buf));
  lgI2cWriteI2CBlockData(dfr_gas_fd, 0, (char *)&_protocol, sizeof(_protocol));
  lguSleep(0.02);
  lgI2cReadI2CBlockData(dfr_gas_fd, 0, (char *)recvbuf, 9);
  if (recvbuf[8] != FucCheckSum(recvbuf, 8))
    return 0.0;
  uint16_t temp_ADC = (recvbuf[2] << 8) + recvbuf[3];
  float Vpd3=3*(float)temp_ADC/1024;
  float Rth = Vpd3*10000/(3-Vpd3);
  float Tbeta = 1/(1/(273.15+25)+1/3380.13*log(Rth/10000))-273.15;
  return Tbeta;
}

float readGasConcentrationPPM(void) {
  uint8_t buf[6] = {0};
  uint8_t recvbuf[9] = {0};
  uint8_t gastype;
  uint8_t decimal_digits;
  buf[0] = CMD_GET_GAS_CONCENTRATION;
  sProtocol_t _protocol = pack(buf, sizeof(buf));
  lgI2cWriteI2CBlockData(dfr_gas_fd, 0, (char *)&_protocol, sizeof(_protocol));
  lguSleep(0.02);
  lgI2cReadI2CBlockData(dfr_gas_fd,0, (char *)recvbuf, 9);
  float Con=0.0;
  float _temp = 0.0;
  if(FucCheckSum(recvbuf,8) == recvbuf[8])
  {
    Con=((recvbuf[2]<<8)+recvbuf[3])*1.0;
    gastype = recvbuf[4];
    decimal_digits = recvbuf[5];
    switch(decimal_digits){
      case 1:
        Con *= 0.1;
        break;
      case 2:
        Con *= 0.01;
        break;
      default:
        break;
    }
    if (_tempswitch == ON)
    {
      switch (gastype)
      {
        case O2:
          break;
        case CO:
          if (((_temp) > -20) && ((_temp) <= 20)){
            Con = (Con / (0.005 * (_temp) + 0.9));
          }else if (((_temp) > 20) && ((_temp) <= 40)){
            Con = (Con / (0.005 * (_temp) + 0.9) - (0.3 * (_temp)-6));
          }else{
            Con = 0.0;
          }
          break;
        case H2S:
          if (((_temp) > -20) && ((_temp) <= 20)){
            Con = (Con / (0.005 * (_temp) + 0.92));
          }else if (((_temp) > 20) && ((_temp) <= 60)){
            Con = Con / (0.015*_temp - 0.3 );
          }else{
            Con = 0.0;
          }
          break;
        case NO2:
          if (((_temp) > -20) && ((_temp) <= 0)){
            Con = ((Con / (0.005 * (_temp) + 0.9) - (-0.0025 * (_temp) + 0.005)));
          }else if (((_temp) > 0) && ((_temp) <= 20)){
            Con = ((Con / (0.005 * (_temp) + 0.9) - (0.005 * (_temp) + 0.005)));
          }else if (((_temp) > 20) && ((_temp) <= 40)){
            Con = ((Con / (0.005 * (_temp) + 0.9) - (0.0025 * (_temp) + 0.1)));
          }else{
            Con = 0.0;
          }
          break;
        case O3:
          if (((_temp) > -20) && ((_temp) <= 0)){
            Con = ((Con / (0.015 * (_temp) + 1.1) - 0.05));
          }else if (((_temp) > 0) && ((_temp) <= 20)){
            Con = ((Con / 1.1 - (0.01 * (_temp))));
          }else if (((_temp) > 20) && ((_temp) <= 40)){
            Con = ((Con / 1.1 - (-0.005 * (_temp) + 0.3)));
          }else{
            Con = 0.0;
          }
          break;
        case CL2:
          if (((_temp) > -20) && ((_temp) <= 0)){
            Con = ((Con / (0.015 * (_temp) + 1.1) - (-0.0025 * (_temp))));
          }else if (((_temp) > 0) && ((_temp) <= 20)){
            Con = ((Con / 1.1 - 0.005 * (_temp)));
          }else if (((_temp) > 20) && ((_temp) <= 40)){
            Con = ((Con / 1.1 - (-0.005 * (_temp) +0.3)));
          }else{
            Con = 0.0;
          }
          break;
        case NH3:
          if (((_temp) > -20) && ((_temp) <= 0)){
            Con = (Con / (0.006 * (_temp) + 0.95) - (-0.006 * (_temp) + 0.25));
          }else if (((_temp) > 0) && ((_temp) <= 20)){
            Con = (Con / (0.006 * (_temp) + 0.95) - (-0.012 * (_temp) + 0.25));
          }else if (((_temp) > 20) && ((_temp) <= 40)){
            Con = (Con / (0.005 * (_temp) + 1.08) - (-0.1 * (_temp) + 2));
          }else{
            Con = 0.0;
          }
          break;
        case H2:
          if (((_temp) > -20) && ((_temp) <= 20)){
            Con = (Con / (0.74 * (_temp) + 0.007) - 5);
          }else if (((_temp) > 20) && ((_temp) <= 40)){
            Con = (Con / (0.025 * (_temp) + 0.3) - 5);
          }else if (((_temp) > 40) && ((_temp) <= 60)){
            Con = (Con / (0.001 * (_temp) + 0.9) - (0.75 * _temp -25));
          }else{
            Con = 0.0;
          }
          break;
        case HF:
          if (((_temp) > -20) && ((_temp) <= 0)){
            Con = (((Con / 1) - (-0.0025 * (_temp))));
          }else if (((_temp) > 0) && ((_temp) <= 20)){
            Con = ((Con / 1 + 0.1));
          }else if (((_temp) > 20) && ((_temp) <= 40)){
            Con = ((Con / 1 - (0.0375 * (_temp)-0.85)));
          }else{
            Con = 0.0;
          }
          break;
        case _PH3:
          if (((_temp) > -20) && ((_temp) <= 40)){
            Con = ((Con / (0.005 * (_temp) + 0.9)));
          }else{
            Con = 0.0;
          }
          break;
        case HCL:
          if ((_temp > -20) && (_temp <= 0)){
            Con = Con - (-0.0075 * _temp - 0.1);
          }else if ((_temp > 0) && (_temp <= 20)){
            Con = Con - (-0.1);
          }else if ((_temp > 20) && (_temp < 50)){
            Con = Con - (-0.01 * _temp + 0.1);
          }else{
            Con = 0.0;
          }
          break;
        case SO2:
          if ((_temp >- 40) && (_temp <= 40)){
            Con = Con / (0.006 * _temp + 0.95);
          }else if ((_temp > 40) && (_temp <= 60)){
            Con = Con / (0.006 * _temp + 0.95) - (0.05 * _temp - 2);
          }else{
            Con = 0.0;
          }
          break;
        default:
          break;
      }
    }
  }else{
    Con = 0.0;
  }
  if(Con < 0.00001){
    Con = 0.0;
  }
  return Con;
}

int dfr_gas_read(short *temp, short *conc) {
	//printf("\n SHTC3 Sensor Test Program ...\n");

	dfr_gas_fd = lgI2cOpen(1, DFR_GAS_I2C_ADDR, 0);
	if (dfr_gas_fd < 0)
		return EXIT_FAILURE;
	float c = readGasConcentrationPPM();
	float t = readTempC();
	*temp = (short)t*100;
	*conc = (short)c*100;
	//printf("Temperature = %6.2f°C , O2 Conc = %6.1f%% \n", t, c);
	lgI2cClose(dfr_gas_fd);
	return EXIT_SUCCESS;
}

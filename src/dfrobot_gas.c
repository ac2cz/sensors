/*
 * dfrobot_gas.c
 *
 *  Created on: Nov 1, 2024
 *      Author: g0kla
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lgpio.h>
#include <math.h>
#include "dfrobot_gas.h"

//unsigned short TH_DATA, CONC_DATA;
int dfr_gas_fd;

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

sProtocol_t pack(uint8_t *pBuf, uint8_t len)
{
  sProtocol_t _protocol;
  _protocol.head = 0xff;
  _protocol.addr = 0x01;
  memcpy(_protocol.data, pBuf, len);
  _protocol.check = FucCheckSum((uint8_t *)&_protocol, 8);
  return _protocol;
}

float readTempC(void)
{
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

void dfr_gas_read_data() {

  char buf[3];
  dfr_write_command(CMD_GET_TEMP); // Read temperature first
  lguSleep(0.02);
  lgI2cReadDevice(dfr_gas_fd, buf, 3);

  dfr_write_command(CMD_GET_GAS_CONCENTRATION); // Get gas concentration
  lguSleep(0.02);
  lgI2cReadDevice(dfr_gas_fd, buf, 3);

}

int dfr_gas_read(short *temp, short *conc) {
	//printf("\n SHTC3 Sensor Test Program ...\n");

	dfr_gas_fd = lgI2cOpen(1, DFR_GAS_I2C_ADDR, 0);
	//dfr_gas_read_data();
	float c = 0.0;
	float t = readTempC();
//	*temp = TH_DATA;
//	*conc = CONC_DATA;
	printf("Temperature = %6.2f°C , Conc = %6.2f%% \n", t, c);
	lgI2cClose(dfr_gas_fd);
	return 0;
}

#include <stdlib.h>
#include <lgpio.h>
#include <stdio.h>
#include <math.h>
#include "LPS22HB.h"

int lps22_fd;

char LPS22HB_readByte(int reg) {
	return lgI2cReadByteData(lps22_fd, reg);
}

unsigned short LPS22HB_readU16(int reg) {
	return lgI2cReadWordData(lps22_fd, reg);
}

void LPS22HB_writeByte(int reg, int val) {
	lgI2cWriteByteData(lps22_fd, reg, val);
}

void LPS22HB_RESET() {
	unsigned char Buf;
    Buf=LPS22HB_readU16(LPS_CTRL_REG2);
    Buf|=0x04;                                         
    LPS22HB_writeByte(LPS_CTRL_REG2,Buf);                  //SWRESET Set 1
    while(Buf)
    {
        Buf=LPS22HB_readU16(LPS_CTRL_REG2);
        Buf&=0x04;
    }
}

void LPS22HB_START_ONESHOT() {
    unsigned char Buf;
    Buf=LPS22HB_readU16(LPS_CTRL_REG2);
    Buf|=0x01;                                         //ONE_SHOT Set 1
    LPS22HB_writeByte(LPS_CTRL_REG2,Buf);
}

unsigned char LPS22HB_INIT() {
    lps22_fd = lgI2cOpen(1,LPS22HB_I2C_ADDRESS,0);
    if (lps22_fd < 0)
    	return EXIT_FAILURE;
    if(LPS22HB_readByte(LPS_WHO_AM_I)!=LPS_ID) return EXIT_FAILURE;    //Check device ID
    LPS22HB_RESET();                                    //Wait for reset to complete
    LPS22HB_writeByte(LPS_CTRL_REG1 ,   0x02);              //Low-pass filter disabled , output registers not updated until MSB and LSB have been read , Enable Block Data Update , Set Output Data Rate to 0
    return EXIT_SUCCESS;
}

int LPS22HB_read(int *pressure, short *temperature) {
	//float PRESS_DATA=0;
	//float TEMP_DATA=0;
	unsigned char u8Buf[3];

	//printf("\nPressure Sensor Test Program ...\n");
	if(LPS22HB_INIT() != EXIT_SUCCESS) {
		//debug_print("Pressure Sensor Error\n");
		return EXIT_FAILURE;
	}
	//    printf("\nPressure Sensor OK\n");
	LPS22HB_START_ONESHOT();        //Trigger one shot data acquisition
	if((LPS22HB_readByte(LPS_STATUS)&0x01)==0x01)   //a new pressure data is generated
	{
		u8Buf[0]=LPS22HB_readByte(LPS_PRESS_OUT_XL);
		u8Buf[1]=LPS22HB_readByte(LPS_PRESS_OUT_L);
		u8Buf[2]=LPS22HB_readByte(LPS_PRESS_OUT_H);
		*pressure=(u8Buf[2]<<16)+(u8Buf[1]<<8)+u8Buf[0];
		//PRESS_DATA=(float)((u8Buf[2]<<16)+(u8Buf[1]<<8)+u8Buf[0])/4096.0f;
	}
	if((LPS22HB_readByte(LPS_STATUS)&0x02)==0x02)   // a new pressure data is generated
	{
		u8Buf[0]=LPS22HB_readByte(LPS_TEMP_OUT_L);
		u8Buf[1]=LPS22HB_readByte(LPS_TEMP_OUT_H);
		*temperature=(u8Buf[1]<<8)+u8Buf[0];
		//TEMP_DATA=(float)((u8Buf[1]<<8)+u8Buf[0])/100.0f;
	}

	//debug_print("Pressure = %6.2f hPa , Temperature = %6.2f °C\r\n", PRESS_DATA, TEMP_DATA);
	lgI2cClose(lps22_fd);
	return EXIT_SUCCESS;
}

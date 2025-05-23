#include <stdlib.h>
#include <lgpio.h>
#include <stdio.h>
#include <math.h>
#include"AD.h"

int Config_Set;
int adc_fd; 

int AD_readU16(int reg) {
	int val;
    unsigned char Val_L,Val_H;
    val=lgI2cReadWordData(adc_fd,reg);                    //High and low bytes are the opposite       
    Val_H=val&0xff;
    Val_L=val>>8;
    val=(Val_H<<8)|Val_L;                               //Correct byte order
    return val;
}

void AD_writeWord(int reg, int val) {
	unsigned char Val_L,Val_H;
    Val_H=val&0xff;
    Val_L=val>>8;
    val=(Val_H<<8)|Val_L;                               ////Correct byte order
	lgI2cWriteWordData(adc_fd,reg,val);
}

unsigned int ADS1015_INIT() {
    unsigned int state;
    state=AD_readU16(ADS_POINTER_CONFIG) & 0x8000  ;
    return state;
}

unsigned int ADS1015_SINGLE_READ(unsigned int channel)  {          //Read single channel data
    unsigned int data;
    Config_Set = ADS_CONFIG_MODE_NOCONTINUOUS        |   //mode：Single-shot mode or power-down state    (default)
                 ADS_CONFIG_PGA_4096                 |   //Gain= +/- 4.096V                              (default)
                 ADS_CONFIG_COMP_QUE_NON             |   //Disable comparator                            (default)
                 ADS_CONFIG_COMP_NONLAT              |   //Nonlatching comparator                        (default)
                 ADS_CONFIG_COMP_POL_LOW             |   //Comparator polarity：Active low               (default)
                 ADS_CONFIG_COMP_MODE_TRADITIONAL    |   //Traditional comparator                        (default)
                 ADS_CONFIG_DR_RATE_480             ;    //Data rate=480Hz                             (default)
    switch (channel)
    {
        case (0):
            Config_Set |= ADS_CONFIG_MUX_SINGLE_0;
            break;
        case (1):
            Config_Set |= ADS_CONFIG_MUX_SINGLE_1;
            break;
        case (2):
            Config_Set |= ADS_CONFIG_MUX_SINGLE_2;
            break;
        case (3):
            Config_Set |= ADS_CONFIG_MUX_SINGLE_3;
            break;
    }
    Config_Set |=ADS_CONFIG_OS_SINGLE_CONVERT;
    AD_writeWord(ADS_POINTER_CONFIG,Config_Set);
    lguSleep(0.02);
    data=AD_readU16(ADS_POINTER_CONVERT);
    return data;
}

int adc_read(int channel, short *AIN3_DATA) {
//	int  AIN0_DATA,AIN1_DATA,AIN2_DATA,AIN3_DATA;

	if (channel <0 || channel > 3)
		return EXIT_FAILURE;
    adc_fd=lgI2cOpen(1,ADS_I2C_ADDRESS,0);
    if (adc_fd < 0)
    	return EXIT_FAILURE;
	//printf("\nADS1015 Test Program ...\n");
    if(ADS1015_INIT()!=0x8000) {
    	printf("\nADS1015 Error\n");
		return EXIT_FAILURE;
	}
	lguSleep(0.01);
	//printf("\nADS1015 OK\n");
//        AIN0_DATA=ADS1015_SINGLE_READ(0);
//       AIN1_DATA=ADS1015_SINGLE_READ(1);
//        AIN2_DATA=ADS1015_SINGLE_READ(2);
        *AIN3_DATA = ADS1015_SINGLE_READ(channel);
        /* 16 bit ADC, but can read +ve and -ve.  If we assume positive then value is scaled by FullScale/32768.  When Full scale is 4096 this = 0.125 */
//        debug_print("\nAIN0 = %d(%0.2fmv) ,AIN1 = %d(%0.2fmv) ,AIN2 = %d(%0.2fmv) AIN3 = %d(%0.2fmv)\n\r", AIN0_DATA,(float)AIN0_DATA*0.125, AIN1_DATA,(float)AIN1_DATA*0.125,AIN2_DATA,(float)AIN2_DATA*0.125,AIN3_DATA,(float)AIN3_DATA*0.125);
 //       debug_print("Battery V = %d(%0.2fmv)\n\r",AIN3_DATA,(float)AIN3_DATA*0.125);
        lguSleep(0.1);

    lgI2cClose(adc_fd);
    return EXIT_SUCCESS;
}

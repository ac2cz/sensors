#include "AK09918.h"
#include "debug.h"

uint8_t buf[8];
int AK09918_dev;
// This is the default calibration value. 
// If it is not accurate, uncomment line 36 and calibrate it manually once
IMU_ST_SENSOR_DATA gstMagOffset = {-188, 49, 35};

uint16_t AK09918_ReadnByte(uint8_t reg)
{
    lgI2cReadI2CBlockData(AK09918_dev,reg,(char *)buf,8);
    return 0;
}

uint8_t AK09918_I2C_Write(uint8_t reg, uint8_t Value)
{
    lgI2cWriteByteData(AK09918_dev,reg,Value);
    return 0;
}

uint8_t AK09918_I2C_ReadByte(uint8_t reg)
{
    uint8_t value;
    value = lgI2cReadByteData(AK09918_dev,reg);
    return value;
}
int AK09918_init(uint8_t mode) {
    AK09918_dev = lgI2cOpen(1, AK09918_I2C_ADDR, 0);

    if(AK09918_I2C_ReadByte(AK09918_WIA2) != 0x0C) {
        debug_print("AK09918: Fail to read\r\n");
        return 0;
    } else{
        //printf("Success to read\r\n");
        AK09918_I2C_Write(AK09918_CNTL2,mode);
    }
    return 1;
    // AK09918_MagnOffset();
}

void AK09918_close() {
	lgI2cClose(AK09918_dev);
}

uint8_t AK09918_Read_data(IMU_ST_SENSOR_DATA *pstMagnRawData)
{
    int16_t s16Buf[3] = {0};

    AK09918_ReadnByte(AK09918_HXL);
    s16Buf[0] = ((short int)buf[1] << 8) | buf[0];
    s16Buf[1] = ((short int)buf[3] << 8) | buf[2];
    s16Buf[2] = ((short int)buf[5] << 8) | buf[4];
    if (buf[7] & AK09918_HOFL_BIT) 
    {
        printf("Sensor overflow\n");
        // return AK09918_ERR_OVERFLOW;
    }
        

    pstMagnRawData->s16X = s16Buf[0] - gstMagOffset.s16X;
    pstMagnRawData->s16Y = s16Buf[1] - gstMagOffset.s16Y;
    pstMagnRawData->s16Z = s16Buf[2] - gstMagOffset.s16Z;
    // printf("Magnetic:       X= %d ,      Y  = %d ,      Z  = %d\r\n\n",s32OutBuf[0],s32OutBuf[1],s32OutBuf[2]);
    return 0;
}

void AK09918_MagnOffset(void)
{
    int8_t s8CaliCounter = 0;
    int16_t s16MagBuff[9];
    IMU_ST_SENSOR_DATA stMagnRawData;

    while(s8CaliCounter != 3)
    {
        switch(s8CaliCounter)
        {
        case 0:
            printf("Please place it horizontally 10dof\n");
            printf("If you have completed the action, press Enter on your keyboard\n");
            getchar();
            lguSleep(0.1);
            AK09918_Read_data(&stMagnRawData);
            s16MagBuff[0] = stMagnRawData.s16X;
            s16MagBuff[1] = stMagnRawData.s16Y;
            s16MagBuff[2] = stMagnRawData.s16Z;
            lguSleep(0.1);
            break;
        case 1:
            printf("Please rotate the 10dof-D 180 degrees around the z-axis\n");
            printf("If you have completed the action, press Enter on your keyboard\n");
            getchar();
           lguSleep(0.1);        
            AK09918_Read_data(&stMagnRawData);
            s16MagBuff[3] = stMagnRawData.s16X;
            s16MagBuff[4] = stMagnRawData.s16Y;
            s16MagBuff[5] = stMagnRawData.s16Z;
            lguSleep(0.1);
            break;
        case 2:
            printf("Please rotate the 10dof-D 180 degrees around the X-axis\n");
            printf("If you have completed the action, press Enter on your keyboard\n");
            getchar();
            lguSleep(0.1);       
            AK09918_Read_data(&stMagnRawData);
            s16MagBuff[6] = stMagnRawData.s16X;
            s16MagBuff[7] = stMagnRawData.s16Y;
            s16MagBuff[8] = stMagnRawData.s16Z;
            lguSleep(0.1);   
            break;
        default:
            printf("Magnetic calibration error\n");
            break;
        }
        s8CaliCounter++;
    }

    if(s8CaliCounter == 3)
	{ 
			gstMagOffset.s16X=(s16MagBuff[0]+s16MagBuff[3])/2;
			gstMagOffset.s16Y=(s16MagBuff[1]+s16MagBuff[4])/2;
			gstMagOffset.s16Z=(s16MagBuff[5]+s16MagBuff[8])/2;
	}
    printf("%d %d %d \n",gstMagOffset.s16X,gstMagOffset.s16Y,gstMagOffset.s16Z);
}


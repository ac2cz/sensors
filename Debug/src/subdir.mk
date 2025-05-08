################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/AD.c \
../src/LPS22HB.c \
../src/SHTC3.c \
../src/dfrobot_gas.c \
../src/sensors.c \
../src/xensiv_pasco2.c 

C_DEPS += \
./src/AD.d \
./src/LPS22HB.d \
./src/SHTC3.d \
./src/dfrobot_gas.d \
./src/sensors.d \
./src/xensiv_pasco2.d 

OBJS += \
./src/AD.o \
./src/LPS22HB.o \
./src/SHTC3.o \
./src/dfrobot_gas.o \
./src/sensors.o \
./src/xensiv_pasco2.o 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../inc -I/usr/local/include/iors_common -I../imu -I../TCS34087 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src

clean-src:
	-$(RM) ./src/AD.d ./src/AD.o ./src/LPS22HB.d ./src/LPS22HB.o ./src/SHTC3.d ./src/SHTC3.o ./src/dfrobot_gas.d ./src/dfrobot_gas.o ./src/sensors.d ./src/sensors.o ./src/xensiv_pasco2.d ./src/xensiv_pasco2.o

.PHONY: clean-src


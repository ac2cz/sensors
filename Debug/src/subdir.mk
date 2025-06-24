################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/AD.c \
../src/LPS22HB.c \
../src/SHTC3.c \
../src/config.c \
../src/cosmic_watch.c \
../src/dfrobot_gas.c \
../src/sensors.c \
../src/serial_util.c \
../src/ultrasonic_mic.c \
../src/xensiv_pasco2.c 

C_DEPS += \
./src/AD.d \
./src/LPS22HB.d \
./src/SHTC3.d \
./src/config.d \
./src/cosmic_watch.d \
./src/dfrobot_gas.d \
./src/sensors.d \
./src/serial_util.d \
./src/ultrasonic_mic.d \
./src/xensiv_pasco2.d 

OBJS += \
./src/AD.o \
./src/LPS22HB.o \
./src/SHTC3.o \
./src/config.o \
./src/cosmic_watch.o \
./src/dfrobot_gas.o \
./src/sensors.o \
./src/serial_util.o \
./src/ultrasonic_mic.o \
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
	-$(RM) ./src/AD.d ./src/AD.o ./src/LPS22HB.d ./src/LPS22HB.o ./src/SHTC3.d ./src/SHTC3.o ./src/config.d ./src/config.o ./src/cosmic_watch.d ./src/cosmic_watch.o ./src/dfrobot_gas.d ./src/dfrobot_gas.o ./src/sensors.d ./src/sensors.o ./src/serial_util.d ./src/serial_util.o ./src/ultrasonic_mic.d ./src/ultrasonic_mic.o ./src/xensiv_pasco2.d ./src/xensiv_pasco2.o

.PHONY: clean-src


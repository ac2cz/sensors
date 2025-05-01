################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../imu/AK09918.c \
../imu/IMU.c \
../imu/QMI8658.c \
../imu/imu_test.c 

C_DEPS += \
./imu/AK09918.d \
./imu/IMU.d \
./imu/QMI8658.d \
./imu/imu_test.d 

OBJS += \
./imu/AK09918.o \
./imu/IMU.o \
./imu/QMI8658.o \
./imu/imu_test.o 


# Each subdirectory must supply rules for building sources it contributes
imu/%.o: ../imu/%.c imu/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../inc -I/usr/local/include/iors_common -I../imu -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-imu

clean-imu:
	-$(RM) ./imu/AK09918.d ./imu/AK09918.o ./imu/IMU.d ./imu/IMU.o ./imu/QMI8658.d ./imu/QMI8658.o ./imu/imu_test.d ./imu/imu_test.o

.PHONY: clean-imu


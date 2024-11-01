################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/AD.c \
../src/LPS22HB.c \
../src/SHTC3.c \
../src/sensors.c 

C_DEPS += \
./src/AD.d \
./src/LPS22HB.d \
./src/SHTC3.d \
./src/sensors.d 

OBJS += \
./src/AD.o \
./src/LPS22HB.o \
./src/SHTC3.o \
./src/sensors.o 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../inc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src

clean-src:
	-$(RM) ./src/AD.d ./src/AD.o ./src/LPS22HB.d ./src/LPS22HB.o ./src/SHTC3.d ./src/SHTC3.o ./src/sensors.d ./src/sensors.o

.PHONY: clean-src


################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../TCS34087/TCS34087.c 

C_DEPS += \
./TCS34087/TCS34087.d 

OBJS += \
./TCS34087/TCS34087.o 


# Each subdirectory must supply rules for building sources it contributes
TCS34087/%.o: ../TCS34087/%.c TCS34087/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../inc -I/usr/local/include/iors_common -I../imu -I../TCS34087 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-TCS34087

clean-TCS34087:
	-$(RM) ./TCS34087/TCS34087.d ./TCS34087/TCS34087.o

.PHONY: clean-TCS34087


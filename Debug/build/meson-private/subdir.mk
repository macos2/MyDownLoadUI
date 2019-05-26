################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../build/meson-private/sanitycheckc.c 

OBJS += \
./build/meson-private/sanitycheckc.o 

C_DEPS += \
./build/meson-private/sanitycheckc.d 


# Each subdirectory must supply rules for building sources it contributes
build/meson-private/%.o: ../build/meson-private/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -I/usr/include/gtk-3.0 -I/usr/include/pango-1.0 -I/usr/include/fribidi -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/uuid -I/usr/include/harfbuzz -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/gio-unix-2.0/ -I/usr/include/libdrm -I/usr/include/atk-1.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib64/dbus-1.0/include -O0 -g3 -w -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



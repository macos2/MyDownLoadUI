################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../MyCurl.c \
../MyDownloadUi.c \
../MySoupDl.c \
../gresources.c \
../mycurl_demo.c \
../mysoupdl_demo.c 

OBJS += \
./MyCurl.o \
./MyDownloadUi.o \
./MySoupDl.o \
./gresources.o \
./mycurl_demo.o \
./mysoupdl_demo.o 

C_DEPS += \
./MyCurl.d \
./MyDownloadUi.d \
./MySoupDl.d \
./gresources.d \
./mycurl_demo.d \
./mysoupdl_demo.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -I/usr/include/gtk-3.0 -I/usr/include/pango-1.0 -I/usr/include/fribidi -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/uuid -I/usr/include/harfbuzz -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/gio-unix-2.0/ -I/usr/include/libdrm -I/usr/include/atk-1.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib64/dbus-1.0/include -O0 -g3 -w -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



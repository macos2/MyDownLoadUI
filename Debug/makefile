################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include build/mysoupdl_demo@exe/subdir.mk
-include build/mycurl_demo@exe/subdir.mk
-include build/meson-private/subdir.mk
-include build/MySoupDl@sta/subdir.mk
-include build/MyDownloadUi@sta/subdir.mk
-include build/MyCurl@sta/subdir.mk
-include build/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: MyDownLoadUI

# Tool invocations
MyDownLoadUI: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc  -o "MyDownLoadUI" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) MyDownLoadUI
	-@echo ' '

.PHONY: all clean dependents

-include ../makefile.targets

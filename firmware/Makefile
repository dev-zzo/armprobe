# Easy peasy make file for STM32 boards

#
# Application related shenanigans
#

# Application objects
OBJS=main.o debug.o swd.o gpio.o usb_core.o usb_ep0d.o usb_ep0a.o

# The main dependency name
OUTPUT=project

#
# Build related shenanigans
#

# OS stuff
RM=del

# Toolchain stuff
TOOLCHAIN_PREFIX=arm-none-eabi-
AS=$(TOOLCHAIN_PREFIX)as
CC=$(TOOLCHAIN_PREFIX)gcc
LD=$(TOOLCHAIN_PREFIX)ld
SIZE=$(TOOLCHAIN_PREFIX)size
OBJCOPY=$(TOOLCHAIN_PREFIX)objcopy

CFLAGS=-Os -Wall
CFLAGS+=-mthumb -mcpu=cortex-m3
CFLAGS+=-I./src -I./cmsis/CoreSupport -I./cmsis/DeviceSupport/ST/STM32F10x
CFLAGS+=-DSTM32F10X_MD
LDFLAGS+=--gc-sections -Tldscripts/stm32f10x.ld -Map $(OUTPUT).map 

# VPATH to support searching for CMSIS files
VPATH=./src:./cmsis/CoreSupport:./cmsis/DeviceSupport/ST/STM32F10x:./cmsis/DeviceSupport/ST/STM32F10x/startup/gcc_ride7

# CMSIS objects: startup code, system code, core support functions
OBJS+=startup_stm32f10x_md.o
OBJS+=system_stm32f10x.o
OBJS+=core_cm3.o

# Recipes

.PHONY: all clean

OUTPUTS=$(OUTPUT).elf $(OUTPUT).hex $(OUTPUT).bin
all: $(OUTPUTS)

$(OUTPUT).elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@
%.bin: %.elf
	$(OBJCOPY) -O binary -j .text -j .data $< $@

clean:
	$(RM) *.o $(OUTPUTS)

# Project files
PROJ = a-tiny-tractor
OBJS = $(patsubst %.c, build/%.o, $(wildcard *.c))
ELF = $(PROJ).elf
HEX = $(PROJ).hex

# MCU and programmer
MCU = attiny85
F_CPU = 8000000
AD_MCU = t85
AD_PROG = avrisp
AD_PORT = /dev/cu.usbmodem1411

# Compiler flags
CFLAGS = -Wall -Os -fpack-struct -fshort-enums -ffunction-sections \
-fdata-sections -funsigned-char -funsigned-bitfields
STD = gnu99
CDEFS = -DF_CPU=$(F_CPU)UL

all: builddir $(OBJS) Makefile
	mkdir -p build
	avr-gcc -mmcu=$(MCU) $(OBJS) -o build/$(ELF)
	avr-objcopy -j .text -O ihex build/$(ELF) build/$(HEX)
	@echo 'Created Flash image (ihex format)'
	avr-size --format=avr --mcu=$(MCU) build/$(ELF)

builddir:
	mkdir -p build

build/%.o: %.c
	avr-gcc -std=$(STD) $(CFLAGS) $(CDEFS) -mmcu=$(MCU) -c -o $@ $<

clean:
	-rm -rf build
	@echo 'Removed build directory!'

readfuse:
	avrdude -p$(AD_MCU) -c$(AD_PROG) -P$(AD_PORT) -Ulfuse:r:-:i

writefuse:
	@echo 'Write fuses (reset CKDIV8 to have 8MHz clock)'
	avrdude -p$(AD_MCU) -c$(AD_PROG) -P$(AD_PORT) -Ulfuse:w:0xE2:m

program:
	avrdude -p$(AD_MCU) -c$(AD_PROG) -P$(AD_PORT) -Uflash:w:build/$(HEX):a

.PHONY: all clean readfuse writefuse program

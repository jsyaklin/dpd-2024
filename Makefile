# Project source files
SOURCES := main.c i2c.c

# Configuration
TOOLS_DIR := /home/jyaklin/arduino-1.8.19/hardware/tools/avr/bin
AVRDUDE_CONF := ${TOOLS_DIR}/../etc/avrdude.conf
CC := ${TOOLS_DIR}/avr-gcc
OBJCOPY := ${TOOLS_DIR}/avr-objcopy
OBJDUMP := ${TOOLS_DIR}/avr-objdump

AVRDUDE := ${TOOLS_DIR}/avrdude

# MCU options
MCU_NAME := atmega328pb
MCU_ABBREV := m328pb

F_CPU  := 8000000

# set fuse options (see fusegen.py for more options)
FUSE_OPTS := --bod-level=off --clk-sel=rc-slow --div8-disable

FUSE_BYTES := $(shell python fusegen.py ${FUSE_OPTS})
LFUSE := $(word 1,${FUSE_BYTES})
HFUSE := $(word 2,${FUSE_BYTES})
EFUSE := $(word 3,${FUSE_BYTES})

# Programmer options
PROG_NAME := avrisp
AVR_PORT ?= /dev/ttyACM0

CFLAGS := -O2 -Wall -Wextra -ffunction-sections -mmcu=${MCU_NAME} -DF_CPU=${F_CPU}
LDFLAGS := -Wl,--gc-sections -mmcu=${MCU_NAME}

OUT_FILE := test.hex

# -----------------------------------------------------------------------
# Actual makefile stuff

OBJECTS := $(SOURCES:.c=.o)
DEPS    := $(SOURCES:.c=.d)

ELF_FILE := $(OUT_FILE:.hex=.elf)

all: ${OUT_FILE} usage
.PHONY: all

upload: ${OUT_FILE}
	${AVRDUDE} -p ${MCU_ABBREV} -c ${PROG_NAME} -C ${AVRDUDE_CONF} -P ${AVR_PORT} \
		-U lfuse:w:0x${LFUSE}:m -U hfuse:w:0x${HFUSE}:m -U efuse:w:0x${EFUSE}:m \
		-U flash:w:${OUT_FILE}:i
.PHONY: upload

clean:
	rm -f ${ELF_FILE} ${OUT_FILE} *.o *.d
.PHONY: clean

usage: ${ELF_FILE}
	${OBJDUMP} -P mem-usage $<
.PHONY: usage

disasm: ${ELF_FILE}
	${OBJDUMP} -d $<
.PHONY: disasm

%.hex: %.elf
	${OBJCOPY} -O ihex $< $@

${ELF_FILE}: ${OBJECTS}
	${CC} ${LDFLAGS} $^ -o $@

-include ${DEPS}

%.o: %.c
	${CC} ${CFLAGS} -MMD -MF $(patsubst %.o,%.d,$@) -c $< -o $@

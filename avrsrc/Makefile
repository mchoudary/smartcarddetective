############################################################################### # Makefile for the project SCD ############################################################################### #Project name, used for target, etc..
PROJECTNAME = scd

## General Flags PROJECT = SCD
MCU = at90usb1287

# Processor frequency.
F_CPU = 16000000
F_CLOCK = $(F_CPU)


## LUFA STUFF ##

# Target board (see library "Board Types" documentation, NONE for projects not requiring
# # LUFA board drivers). If USER is selected, put custom board drivers in a directory called
# # "Board" inside the application directory.
BOARD = USER

# Path to the LUFA library
LUFA_PATH = .

# # LUFA library compile-time options and predefined tokens
LUFA_OPTS  = -D USB_DEVICE_ONLY
LUFA_OPTS += -D FIXED_CONTROL_ENDPOINT_SIZE=8
LUFA_OPTS += -D FIXED_NUM_CONFIGURATIONS=1
LUFA_OPTS += -D USE_FLASH_DESCRIPTORS
LUFA_OPTS += -D USE_STATIC_OPTIONS="(USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_AUTO_PLL)"
LUFA_OPTS += -D NO_STREAM_CALLBACKS
LUFA_OPTS += -D INTERRUPT_CONTROL_ENDPOINT

# Place -D or -U options here for C sources
LUFA_DEFS  = -DF_CPU=$(F_CPU)UL
LUFA_DEFS += -DF_CLOCK=$(F_CLOCK)UL
LUFA_DEFS += -DBOARD=BOARD_$(BOARD)
LUFA_DEFS += $(LUFA_OPTS)


# Create the LUFA source path variables by including the LUFA root makefile
include $(LUFA_PATH)/LUFA/makefile

## Include Directories, use -I""
INCLUDES = -I"lufa_usb_virtual_serial/"

# librarie directories, use -L""
LIBDIRS= 

# libraries to link in (e.g. -lmylib)
LIBS=

# Optimization level, 
# use s (size opt), 1, 2, 3 or 0 (off)
OPTLEVEL=s

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
DEPFOLDER = dep
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99 -DF_CPU=16000000UL -O$(OPTLEVEL) -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF $(DEPFOLDER)/$(@F).d 
CFLAGS += $(LUFA_DEFS)

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
MAPFILE = $(PROJECTNAME).map
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=$(MAPFILE)


## Intel Hex file production flags
HEX_FORMAT=ihex
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature
HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

## Objects explicitly added by the user
LINKONLYOBJECTS = 

#Compilers
CC = avr-gcc
CPP = avr-g++
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
SIZE=avr-size

#Parameters for programming/debuggin
AVRDUDE = avrdude
AVRDUDE_PROGID = dragon_jtag
AVRDUDE_PARTNO = usb1287
AVRDUDE_PORT = usb
AVARICE = avarice
AVARICE_FLAGS = --dragon --erase --program
AVARICE_PORT = usb
AVARICE_LPORT = 4242
AVRGDB = avr-gdb
GDBCONF = gdb$(PROJECTNAME).conf
AVRINSIGHT = avr-insight
DFUPROG = dfu-programmer

# Targets
TARGET = $(PROJECTNAME).elf
HEXTARGET = $(PROJECTNAME).hex
HEXDFUTARGET = $(PROJECTNAME)-DFU.hex
EEPTARGET = $(PROJECTNAME).eep
LSSTARGET = $(PROJECTNAME).lss
SIZETARGET = $(PROJECTNAME).size
ALLTARGETS = $(TARGET) $(HEXTARGET) $(EEPTARGET) $(LSSTARGET) $(SIZETARGET) $(HEXDFUTARGET)
CLEANTARGETS = $(TARGET) $(EEPTARGET) $(LSSTARGET) $(SIZETARGET)

# All project source files (C, C++, ASM)
PRJSRC = scd.c emv.c scd_hal.c scd_io.c utils.c terminal.c serial.c apps.c scd_hal.S scd.S
PRJSRC += lufa_usb_virtual_serial/VirtualSerial.c lufa_usb_virtual_serial/Descriptors.c
PRJSRC += $(LUFA_SRC_USB)

# Filtered sources
CPPFILES=$(filter %.cpp, $(PRJSRC))
CFILES=$(filter %.c, $(PRJSRC))
ASMFILES=$(filter %.S, $(PRJSRC))

# Object dependencies
OBJECTS=$(CFILES:.c=.o)    \
        $(CPPFILES:.cpp=.o)

## Build
all: $(ALLTARGETS)

## Compile
$(OBJECTS): %.o : %.c
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

##Link
$(TARGET): $(OBJECTS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(OBJECTS) $(ASMFILES) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

$(HEXTARGET): $(TARGET)
	$(OBJCOPY) -O $(HEX_FORMAT) $(HEX_FLASH_FLAGS)  $< $@

$(EEPTARGET): $(TARGET)
	$(OBJCOPY) $(HEX_EEPROM_FLAGS) -O $(HEX_FORMAT) $< $@ || exit 0

$(LSSTARGET): $(TARGET)
	$(OBJDUMP) -h -S $< > $@

$(SIZETARGET): ${TARGET}
	@echo
	@avr-size ${TARGET} > $@

$(HEXDFUTARGET): ${HEXTARGET}
	sed -n '$$!p' ${HEXTARGET} > $@
	cat dfu_bootloader.hex >> $@

##Phony targets
.PHONY: clean program

# Clean target
clean:
	-rm -rf $(OBJECTS) $(CLEANTARGETS) $(MAPFILE) $(DEPFOLDER) $(GDBCONF)

# Program the device via JTAG
program-dragon: $(HEXTARGET)
	$(AVRDUDE) -p $(AVRDUDE_PARTNO) -c $(AVRDUDE_PROGID) -P $(AVRDUDE_PORT) -U flash:w:$<
	
# Program the device via JTAG including bootloader
program-dragon-dfu: $(HEXDFUTARGET)
	$(AVRDUDE) -p $(AVRDUDE_PARTNO) -c $(AVRDUDE_PROGID) -P $(AVRDUDE_PORT) -U flash:w:$<

# Program the device via DFU (USB)
program-dfu: $(HEXTARGET)
	$(DFUPROG) $(MCU) erase
	$(DFUPROG) $(MCU) flash $<
	$(DFUPROG) $(MCU) reset
	$(DFUPROG) $(MCU) reset

# Set the correct values for the fuses
set-dragon-fuses: 
	$(AVRDUDE) -p $(AVRDUDE_PARTNO) -c $(AVRDUDE_PROGID) -P $(AVRDUDE_PORT) -e -U hfuse:w:0x19:m -U lfuse:w:0xCE:m -U efuse:w:0xF3:m

# Create gdb conf file for debug
$(GDBCONF): $(TARGET)
	@echo "file $(TARGET)" > $(GDBCONF)
	@echo "target remote localhost:4242" >> $(GDBCONF)

# Use avarice to connect the AVR Dragon
avarice: $(HEXTARGET)
	$(AVARICE) -j $(AVARICE_PORT) $(AVARICE_FLAGS) --file $< :$(AVARICE_LPORT)

# connect gdb to the AVR Dragon
debug: $(GDBCONF) $(TARGET)
	$(AVRGDB) -x $(GDBCONF)

# connect gdb to the AVR Dragon using insight interface
debug-insight: $(GDBCONF) $(TARGET)
	$(AVRGDB) -x $(GDBCONF) -w

## Other dependencies
-include $(shell mkdir $(DEPFOLDER) 2>/dev/null) $(wildcard $(DEPFOLDER)/*)


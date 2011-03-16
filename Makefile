###############################################################################
# Makefile for the project SCD
###############################################################################

## General Flags PROJECT = SCD
MCU = at90usb1287

#Project name, used for target, etc..
PROJECTNAME = scd

## Include Directories, use -I""
INCLUDES = 

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

# Targets
TARGET = $(PROJECTNAME).elf
HEXTARGET = $(PROJECTNAME).hex
EEPTARGET = $(PROJECTNAME).eep
LSSTARGET = $(PROJECTNAME).lss
SIZETARGET = $(PROJECTNAME).size
ALLTARGETS = $(TARGET) $(HEXTARGET) $(EEPTARGET) $(LSSTARGET) $(SIZETARGET)

# All project source files (C, C++, ASM)
PRJSRC = scd.c emv.c scd_hal.c scd_io.c utils.c terminal.c scd_hal.S scd.S

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

##Phony targets
.PHONY: clean program

# Clean target
clean:
	-rm -rf $(OBJECTS) $(ALLTARGETS) $(MAPFILE) $(DEPFOLDER) $(GDBCONF)

# Program the device
program: $(HEXTARGET)
	$(AVRDUDE) -p $(AVRDUDE_PARTNO) -c $(AVRDUDE_PROGID) -P $(AVRDUDE_PORT) -U flash:w:$<

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


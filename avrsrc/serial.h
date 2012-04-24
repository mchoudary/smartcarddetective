/**
 * \file
 * \brief serial.h header file
 *
 * This file defines the methods and commands for serial communication
 * between the SCD and a host.
 *
 * These functions are not microcontroller dependent but they are intended
 * for the AVR 8-bit architecture
 *
 * Copyright (C) 2012 Omar Choudary (omar.choudary@cl.cam.ac.uk)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <avr/io.h>

#include "scd_logger.h"

extern uint8_t lcdAvailable;                // if LCD is available
extern uint16_t revision;                   // current SVN revision in BCD
extern uint8_t selected;             // ID of application selected


/**
 * Enum defining the different types of AT commands supported
 */
typedef enum {
    AT_NONE,        // No command, used for errors
    AT_CRST,        // Reset the SCD
    AT_CTERM,       // Start the terminal application
    AT_CLET,        // Log an EMV transaction
    AT_CGEE,        // Get EEPROM contents
    AT_CEEE,        // Erase EEPROM contents
    AT_CGBM,        // Go into bootloader mode
    AT_CCINIT,      // Initialise a card transaction
    AT_CCAPDU,      // Send raw terminal CAPDU
    AT_CCEND,       // Ends the current card transaction
    AT_DUMMY
}AT_CMD;


/// Process serial data received from the host
char* ProcessSerialData(const char* data, log_struct_t *logger);

/// Parse an AT command received from the host
uint8_t ParseATCommand(const char *data, AT_CMD *command, char **atparams);

/// Send EEPROM content as Intel Hex format to the virtual serial port
uint8_t SendEEPROMHexVSerial();
    
/// Virtual Serial Terminal application
void TerminalVSerial(log_struct_t *logger);

/// Convert two hex digit characters into a byte
uint8_t hexCharsToByte(char c1, char c2);

/// Convert a nibble into a hex char
char nibbleToHexChar(uint8_t b, uint8_t high);

#endif // _SERIAL_H_


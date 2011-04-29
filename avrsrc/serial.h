/** \file
 *	\brief serial.h header file
 *
 *  This file defines the methods and commands for serial communication
 *  between the SCD and a host.
 *
 *  These functions are not microcontroller dependent but they are intended
 *  for the AVR 8-bit architecture
 *
 *  Copyright (C) 2011 Omar Choudary (osc22@cam.ac.uk)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <avr/io.h>

extern uint8_t lcdAvailable;
extern uint16_t revision;


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
char* ProcessSerialData(const char* data);

/// Parse an AT command received from the host
uint8_t ParseATCommand(const char *data, AT_CMD *command, char **atparams);

/// Send EEPROM content as Intel Hex format to the virtual serial port
uint8_t SendEEPROMHexVSerial();
    
/// Virtual Serial Terminal application
void TerminalVSerial();

/// Convert two hex digit characters into a byte
uint8_t hexCharsToByte(char c1, char c2);

/// Convert a nibble into a hex char
char nibbleToHexChar(uint8_t b, uint8_t high);

#endif // _SERIAL_H_


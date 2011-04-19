/** \file
 *	\brief scd.h Header file
 *
 *  Contains definitions of functions used by the Smart Card Detective
 *  including filtering and modification of EMV commands
 *
 *  Copyright (C) 2010 Omar Choudary (osc22@cam.ac.uk)
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

#ifndef _SCD_H_
#define _SCD_H_

//------------------------------------------------------------

/// This is a magic value that may be used in detecting a warm reset
#define WARM_RESET_VALUE 0xAA

/// Maximum number of command-response pairs recorded when logging
#define MAX_EXCHANGES 50

/// EEPROM address for byte used on warm reset
#define EEPROM_WARM_RESET 0x0		

/// EEPROM address for stored PIN
#define EEPROM_PIN 0x8		

/// EEPROM address for selected application
#define EEPROM_APPLICATION 0x32	

/// EEPROM address for transaction counter
#define EEPROM_COUNTER 0x40	

/// EEPROM address for log high address pointer 
#define EEPROM_TLOG_POINTER_HI 0x48

/// EEPROM address for log low address pointer 
#define EEPROM_TLOG_POINTER_LO 0x49

/// EEPROM address for transaction log data
#define EEPROM_TLOG_DATA 0x80

/// EEPROM maximum allowed address
#define EEPROM_MAX_ADDRESS 0xFE0

/// Store PIN
#define APP_STORE_PIN 0x01

/// Forward Commands and make log
#define APP_LOG_FORWARD 0x02

/// Forward Commands and Modify PIN
#define APP_FW_MODIFY_PIN 0x03

/// Filter Transaction Amount
#define APP_FILTER_GENERATEAC 0x04

/// Filter amount and log commands
#define APP_FILTER_LOG 0x05

/// Erase EEPROM
#define APP_ERASE_EEPROM 0x06

/// Terminal application
#define APP_TERMINAL 0x07

/// USB Virtual Serial Port
#define APP_VIRTUAL_SERIAL_PORT 0x08

/// Number of existing applications
#define APPLICATION_COUNT 8

/// Application Names
static char* appStrings[] = {
		"Store   PIN", 
		"Forward and Log", 
		"Modify  PIN", 
		"Filter  amount",
		"Filter  and Log",
		"Erase   EEPROM",
		"Terminal",
        "Virtual Serial",
		};


//------------------------------------------------------------


/// Main function
int main(void);

/// Initializes the SCD
void InitSCD();

/// Show menu and select application
uint8_t SelectApplication();

/// Forward commands between terminal and ICC through the ICC
uint8_t ForwardData();

/// Hard-codded version of the FilterGenerateAC function
uint8_t FilterGenerateACSimple();

/// Filter Generate AC command until user accepts or denies the transaction
uint8_t FilterGenerateAC();

/// Stores the PIN data from VERIFY command to EEPROM
uint8_t StorePIN();

/// Forward commands and modify VERIFY command with EEPROM PIN data
uint8_t ForwardAndChangePIN();

/// Filter Generate AC command and log transaction
uint8_t FilterAndLog();

/// Run the terminal application
uint8_t Terminal();

/// Virtual Serial Port (send/receive command strings)
uint8_t VirtualSerial();

/// Process the serial data
uint8_t* ProcessSerialData(const uint8_t* data, uint8_t len, uint8_t *replen);

/** Self Test methods **/
/// Tests the SCD-Terminal communication
void TestSCDTerminal();

/// Tests the SCD-ICC communication
void TestSCDICC();

/// Simple application to switch some LEDs on and off
void SwitchLeds();

/// Tests the hardware (LEDs, LCD and buttons)
void TestHardware();

#endif // _SCD_H_

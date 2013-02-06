/**
 * \file
 * \brief scd.h Header file
 *
 * Contains definitions of functions used by the Smart Card Detective
 *
 * Copyright (C) 2013 Omar Choudary (omar.choudary@cl.cam.ac.uk)
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


#ifndef _SCD_H_
#define _SCD_H_

//------------------------------------------------------------

/// This is a magic value that may be used in detecting a warm reset
#define WARM_RESET_VALUE 0xAA

/// Maximum number of command-response pairs recorded when logging
#define MAX_EXCHANGES 50

/// EEPROM address for byte used on warm reset
#define EEPROM_WARM_RESET 0x0

/// EEPROM address for counter value from T2 - 4 bytes little endian
#define EEPROM_TIMER_T2 0x4

/// Temporary space 1 - use this for any purpose, 4 bytes
#define EEPROM_TEMP_1 0x8

/// Temporary space 2 - use this for any purpose, 4 bytes
#define EEPROM_TEMP_2 0x12

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

// External definitions
extern char* appStrings[];

//------------------------------------------------------------


/// Main function
int main(void);

/// Initializes the SCD
void InitSCD();

/// Show menu and select application
uint8_t SelectApplication();

/// Jump to bootloader if required
void BootloaderJumpCheck(void) __attribute__ ((naked, section (".init3")));

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

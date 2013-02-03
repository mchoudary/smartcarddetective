/**
 * \file
 * \brief apps.h header file
 *
 * This file defines the applications available on the SCD
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

#ifndef _APPS_H_
#define _APPS_H_

#include <avr/io.h>

#include "emv.h"
#include "scd_logger.h"

/// Maximum number of command-response pairs recorded when logging
#define MAX_EXCHANGES 50

/// Use for bootloader jump
#define MAGIC_BOOT_KEY 0x77

/// EEPROM address for stored PIN
#define EEPROM_PIN 0x8		

/** Application IDs used in the application selection menu **/
/// USB Virtual Serial Port
#define APP_VIRTUAL_SERIAL_PORT 0x01
/// Forward data and make log
#define APP_FORWARD 0x02
/// Filter Transaction Amount
#define APP_FILTER_GENERATEAC 0x03
/// Terminal application
#define APP_TERMINAL 0x04
/// Erase EEPROM
#define APP_ERASE_EEPROM 0x05

/// Number of existing applications
#define APPLICATION_COUNT 5

/// Application strings shown in the user menu
// These should be in the order of their IDs
static char* appStrings[] = {
        "Virtual Serial",
		"Forward and Log", 
		"Filter  amount",
		"Terminal",
		"Erase   EEPROM",
		};


/* Global external variables */
extern uint8_t warmResetByte;                   // stores the status of last card reset (warm/cold)
extern CRP* transactionData[MAX_EXCHANGES]; 	// used to log data
extern uint8_t nTransactions;		            // used to log data
extern uint8_t lcdAvailable;			        // non-zero if LCD is working
extern uint8_t nCounter;			            // number of transactions
extern uint8_t selected;                        // ID of application selected
extern uint8_t bootkey;                         // used for bootloader jump
extern volatile uint32_t usCounter;			    // micro-second counter

/// Virtual Serial Port (send/receive command strings)
uint8_t VirtualSerial();

/// Serial Port interface (send/receive command strings)
uint8_t SerialInterface();

/// Clears the contents of the EEPROM
void EraseEEPROM();

/// Resets the data in EEPROM to default values
void ResetEEPROM();

/// Jump to bootloader
void RunBootloader();

/// Forward commands between terminal and ICC through the ICC
uint8_t ForwardData(log_struct_t *logger);

/// Forward commands and responses but log only from Generate AC onwards
uint8_t ForwardDataLogAC(log_struct_t *logger);

/// Filter Generate AC command until user accepts or denies the transaction
uint8_t FilterGenerateAC(log_struct_t *logger);

/// Stores the PIN data from VERIFY command to EEPROM
uint8_t StorePIN(log_struct_t *logger);

/// Forward commands and modify VERIFY command with EEPROM PIN data
uint8_t ForwardAndChangePIN(log_struct_t *logger);

/// Run the terminal application
uint8_t Terminal(log_struct_t *logger);

/// Write the log of the last transaction to EEPROM
void WriteLogEEPROM(log_struct_t *logger);

#endif // _APPS_H_


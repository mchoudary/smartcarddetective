/** \file
 *	\brief apps.h header file
 *
 *  This file defines the applications available on the SCD
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

#ifndef _APPS_H_
#define _APPS_H_

#include <avr/io.h>

#include "emv.h"

/// Maximum number of command-response pairs recorded when logging
#define MAX_EXCHANGES 50

/* Global external variables */
extern CRP* transactionData[MAX_EXCHANGES]; 	// used to log data
extern uint8_t nTransactions;		            // used to log data
extern uint8_t lcdAvailable;			        // non-zero if LCD is working
extern uint8_t nCounter;			            // number of transactions

/// Forward commands between terminal and ICC through the ICC
uint8_t ForwardData();

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


#endif // _APPS_H_


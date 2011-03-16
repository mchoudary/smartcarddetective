/** \file
 *	\brief scd_hal.h - SCD hardware abstraction layer header file for AT90USB1287
 *
 *  This file provides an abstract definition for all micro-controller
 *  dependent functions that are used in the Smartcard Defender
 *
 *  For each microcontroller a different halSCD.c source file should
 *  be used.
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



/* SCD to Terminal functions */

/// Returns the frequency of the terminal clock in khz, zero if there is no clock 
uint16_t GetTerminalFreq();

/// Reads the value of the terminal counter
uint16_t ReadCounterTerminal();

/// Starts the counter for the external clock given by the terminal
void StartCounterTerminal();

/// Stops the terminal clock counter
void StopCounterTerminal();

/// Pauses the terminal clock counter
void PauseCounterTerminal();

/// Retrieves the state of the reset line from the terminal
uint8_t GetResetStateTerminal();

/// Sends a byte to the terminal with parity check
uint8_t SendByteTerminalParity(uint8_t byte, uint8_t inverse_convention);

/// Sends a byte to the terminal without parity check
void SendByteTerminalNoParity(uint8_t byte, uint8_t inverse_convention);

/// Receives a byte from the terminal with parity check
uint8_t GetByteTerminalParity(uint8_t inverse_convention, uint8_t *r_byte);

/// Receives a byte from the terminal without parity check
uint8_t GetByteTerminalNoParity(uint8_t inverse_convention, uint8_t *r_byte);

/// Waits (loops) for a number of nEtus based on the Terminal clock
void LoopTerminalETU(uint8_t nEtus);

/// Sends default ATR for T=0 to terminal
void SendT0ATRTerminal(uint8_t inverse_convention, uint8_t TC1);


/** SCD to ICC functions **/

/// Returns non-zero if ICC is inserted, zero otherwise
uint8_t IsICCInserted();

/// Returns non-zero if ICC is powered up
uint8_t IsICCPowered();

/// Powers up the card, if possible
uint8_t PowerUpIcc();

/// Powers down the ICC
void PowerDownICC();

/// Waits (loops) for a number of nEtus based on the ICC clock
void LoopICCETU(uint8_t nEtus);

/// Loops for max_cycles or until the I/O line from ICC becomes low
uint8_t WaitForICCData(uint16_t max_cycles);

/// Receives a byte from the ICC without parity checking
uint8_t GetByteICCNoParity(uint8_t inverse_convention, uint8_t *r_byte);

/// Receives a byte from the ICC with parity checking
uint8_t GetByteICCParity(uint8_t inverse_convention, uint8_t *r_byte);

/// Sends a byte to the ICC without parity check
void SendByteICCNoParity(uint8_t byte, uint8_t inverse_convention);

/// Sends a byte to the ICC with parity check
uint8_t SendByteICCParity(uint8_t byte, uint8_t inverse_convention);

/// Receives the ATR from ICC after a successful activation
uint8_t GetATRICC(uint8_t *inverse_convention, uint8_t *proto,
					uint8_t *TC1, uint8_t *TA3, uint8_t *TB3);

/// Starts a cold or warm reset for ICC
uint8_t ResetICC(uint8_t warm, uint8_t *inverse_convention, uint8_t *proto,
					uint8_t *TC1, uint8_t *TA3, uint8_t *TB3);

/// Starts the activation sequence for the ICC
uint8_t ActivateICC(uint8_t warm);

/// Starts the deactivation sequence for the ICC
void DeactivateICC();

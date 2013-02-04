/**
 * \file
 * \brief scd_hal.h - SCD hardware abstraction layer header file for AT90USB1287
 *
 * This file provides an abstract definition for all micro-controller
 * dependent functions that are used in the Smartcard Defender
 *
 * For each microcontroller a different scd_hal.c source file should
 * be used.
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


#ifndef _SCD_HAL_H_
#define _SCD_HAL_H_


#define ICC_CLK_MODE 0                   // Set to:
                                         // 0 for ICC_CLK = 4 MHz
                                         // 1 for ICC_CLK = 2 MHz
                                         // 2 for ICC_CLK = 1 MHz
                                         // 3 for ICC_CLK = 800 KHz
                                         // 4 for ICC_CLK = 500 KHz
                                         // 5 for external clock - update parameters below as necessary!
#define ETU_TERMINAL 372
#define ETU_HALF(X) ((unsigned int) ((X)/2))
#define ETU_LESS_THAN_HALF(X) ((unsigned int) ((X)*0.46))
#define ETU_EXTENDED(X) ((unsigned int) ((X)*1.075))
#define ICC_VCC_DELAY_US 50   
#define PULL_UP_HIZ_ICC	1		        // Set to 1 to enable pull-ups when setting
								        // the I/O-ICC line to Hi-Z
#define F_CPU 16000000UL                // Change this to the correct frequency (generally CLK = CLK_IO)
#define MAX_WAIT_TERMINAL (1 * F_CPU)   // How many cycles to wait for a terminal response

/* Hardcoded values for ICC clock - selected based on ICC_CLK_MODE above */
#if (ICC_CLK_MODE == 0)
#define ICC_CLK_OCR0A 1                 // F_TIMER0 = CLK_IO / 4
#define ICC_CLK_TCCR1B 0x09             // F_TIMER1 = CLK_IO
#define ETU_ICC 1488                    // 372 * 4
#define ICC_RST_WAIT 50000              // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 1)
#define ICC_CLK_OCR0A 3                 // F_TIMER0 = CLK_IO / 8
#define ICC_CLK_TCCR1B 0x0A             // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 372                     // 372 * 1
#define ICC_RST_WAIT 100000             // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 2)
#define ICC_CLK_OCR0A 7                 // F_TIMER0 = CLK_IO / 16
#define ICC_CLK_TCCR1B 0x0A             // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 744                     // 372 * 2
#define ICC_RST_WAIT 200000             // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 3)
#define ICC_CLK_OCR0A 9                 // F_TIMER0 = CLK_IO / 20
#define ICC_CLK_TCCR1B 0x0A             // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 930                     // 372 * 2.5
#define ICC_RST_WAIT 250000             // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 4)
#define ICC_CLK_OCR0A 15                // F_TIMER0 = CLK_IO / 32
#define ICC_CLK_TCCR1B 0x0A             // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 1488                    // 372 * 4
#define ICC_RST_WAIT 400000             // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 5)
#define ICC_CLK_OCR0A 0                 // TIMER 0 not used, F_EXT = 1MHz
#define ICC_CLK_TCCR1B 0x0A             // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 744                     // 372 * 2
#define ICC_RST_WAIT 200000             // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#endif

/* General SCD functions */

/// Retrieves the value of the sync counter
uint32_t GetCounter();

/// Sets the value of the sync counter to the given value
void SetCounter();

/// Resets to 0 the value of the sync counter
void ResetCounter();

/// Enables the Watch Dog Timer
void EnableWDT(uint16_t ms);

/// Disables the Watch Dog Timer
void DisableWDT();

/// Resets the Watch Dog Timer
void ResetWDT();


/* SCD to Terminal functions */

/// Enable the terminal reset interrupt
void EnableTerminalResetInterrupt();

/// Disable the terminal reset interrupt
void DisableTerminalResetInterrupt();

/// Returns the frequency of the terminal clock in khz, zero if there is no clock 
uint16_t GetTerminalFreq();

/// Checks if we have terminal clock or not
uint16_t IsTerminalClock();

/// Returns the status of the terminal I/O line
uint8_t GetTerminalIOLine();

/// Retrieves the state of the reset line from the terminal
uint8_t GetTerminalResetLine();

/// Loops until the IO or reset line from the terminal become low
uint8_t WaitTerminalResetIOLow(uint32_t max_wait);

/// Reads the value of the timer T2
uint8_t ReadTimerT2();

/// Starts the T2 timer
void StartTimerT2();

/// Stops the T2 timer
void StopTimerT2();

/// Increments the sync counter
void IncrementCounter();

/// Reads the value of the terminal counter
uint16_t ReadCounterTerminal();

/// Starts the counter for the external clock given by the terminal
void StartCounterTerminal();

/// Stops the terminal clock counter
void StopCounterTerminal();

/// Pauses the terminal clock counter
void PauseCounterTerminal();

/// Sends a byte to the terminal with parity check
uint8_t SendByteTerminalParity(uint8_t byte, uint8_t inverse_convention);

/// Sends a byte to the terminal without parity check
void SendByteTerminalNoParity(uint8_t byte, uint8_t inverse_convention);

/// Receives a byte from the terminal with parity check
uint8_t GetByteTerminalParity(
        uint8_t inverse_convention,
        uint8_t *r_byte,
        uint32_t max_wait);

/// Receives a byte from the terminal without parity check
uint8_t GetByteTerminalNoParity(
        uint8_t inverse_convention,
        uint8_t *r_byte,
        uint32_t max_wait);

/// Waits (loops) for a number of nEtus based on the Terminal clock
uint8_t LoopTerminalETU(uint32_t nEtus);


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
uint8_t WaitForICCData(uint32_t max_cycles);

/// Receives a byte from the ICC without parity checking
uint8_t GetByteICCNoParity(uint8_t inverse_convention, uint8_t *r_byte);

/// Receives a byte from the ICC with parity checking
uint8_t GetByteICCParity(uint8_t inverse_convention, uint8_t *r_byte);

/// Sends a byte to the ICC without parity check
void SendByteICCNoParity(uint8_t byte, uint8_t inverse_convention);

/// Sends a byte to the ICC with parity check
uint8_t SendByteICCParity(uint8_t byte, uint8_t inverse_convention);

/// Sets the reset line of the ICC to the desired value
void SetICCResetLine(uint8_t high);

/// Starts the activation sequence for the ICC
uint8_t ActivateICC(uint8_t warm);

/// Starts the deactivation sequence for the ICC
void DeactivateICC();

/// Enable the ICC insert interrupt
void EnableICCInsertInterrupt();

/// Disable the ICC insert interrupt
void DisableICCInsertInterrupt();

#endif // _SCD_HAL_H_

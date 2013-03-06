/**
 * \file
 * \brief utils.h header file
 *
 * This file defines some utility functions used in several parts
 * of the code for this project
 *
 * These functions are not microcontroller dependent but they are intended
 * for the AVR 8-bit architecture
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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <avr/io.h>
#include <scd_logger.h>

extern uint8_t lcdAvailable;

/// Write a 16-bit value
void Write16bitRegister(volatile uint16_t *reg, uint16_t value);

/// Read a 16-bit value
uint16_t Read16bitRegister(volatile uint16_t *reg);

/// Puts the SCD to sleep until receives clock from terminal
void SleepUntilTerminalClock();

/// Puts the SCD to sleep until card is inserted or removed
void SleepUntilCardInserted();

/// Retrieve relative time value and writes it to log
uint8_t LogCurrentTime(log_struct_t *logger);

#endif // _UTILS_H_


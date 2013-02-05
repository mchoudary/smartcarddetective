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
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <avr/io.h>

extern uint8_t lcdAvailable;

/// Write a 16-bit value
void Write16bitRegister(volatile uint16_t *reg, uint16_t value);

/// Read a 16-bit value
uint16_t Read16bitRegister(volatile uint16_t *reg);

/// Puts the SCD to sleep until receives clock from terminal
void SleepUntilTerminalClock();

/// Puts the SCD to sleep until card is inserted or removed
void SleepUntilCardInserted();

#endif // _UTILS_H_


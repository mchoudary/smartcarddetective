/** \file
 *	\brief	utils.c source file
 *
 *  This file implements some utility functions used in several parts
 *  of the code for this project
 *
 *  These functions are not microcontroller dependent but they are intended
 *  for the AVR 8-bit architecture
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

#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "utils.h"
#include "scd_io.h"

void Write16bitRegister(volatile uint16_t *reg, uint16_t value)
{
	uint8_t sreg;

	sreg = SREG;
	cli();
	*reg = value;
	SREG = sreg;	
}

uint16_t Read16bitRegister(volatile uint16_t *reg)
{
	uint16_t i;
	uint8_t sreg;

	sreg = SREG;
	cli();
	i = *reg;
	SREG = sreg;

	return i;
}

/**
 * This function puts the SCD to sleep (including all peripherials), until
 * there is clock received from terminal
 */
void SleepUntilTerminalClock()
{
    uint8_t sreg, lcdstate;

    Write16bitRegister(&OCR3A, 100);
    Write16bitRegister(&TCNT3, 1);
    TCCR3A = 0;
    TIMSK3 = 0x02;  //Interrupt on Timer3 compare A match
    TCCR3B = 0x0F;  // CTC, timer external source
    sreg = SREG;

    // stop LCD and LEDs before going to sleep
    lcdstate = GetLCDState();
    if(lcdAvailable && lcdstate != 0) LCDOff();
    Led1Off();
    Led2Off();
    Led3Off();
    Led4Off();

    // go to sleep
    set_sleep_mode(SLEEP_MODE_IDLE); // it is also possible to use sleep_mode() below
    cli();
    sleep_enable();
    sei();
        sleep_cpu();

    // back from sleep
    sleep_disable();
    SREG = sreg;
    TIMSK3 = 0; // disable interrupts on Timer3
    TCCR3B = 0; // stop timer   
    Led4On();
}

/**
 * This function puts the SCD to sleep (including all peripherials), until
 * the card is inserted or removed
 */
void SleepUntilCardInserted()
{
    uint8_t sreg, lcdstate;

    // stop LCD and LEDs before going to sleep
    lcdstate = GetLCDState();
    if(lcdAvailable && lcdstate != 0) LCDOff();
    Led1Off();
    Led2Off();
    Led3Off();
    Led4Off();

    // go to sleep
    sreg = SREG;
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
    sei();
    sleep_cpu();

    // back from sleep
    sleep_disable();
    SREG = sreg;
    Led4On();
}


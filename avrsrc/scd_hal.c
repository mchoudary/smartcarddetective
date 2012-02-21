/** \file
 *	\brief scd_hal.c - SCD hardware abstraction layer source file for AT90USB1287
 *
 *	This file implements the hardware abstraction layer functions
 *
 *  This functions are implemented specifically for each microcontroller
 *  but the function names should be the same for any microcontroller.
 *  Thus the definition halSCD.h should be the same, while only the
 *  implementation should differ.
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
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#include "scd_hal.h"
#include "scd_io.h"
#include "scd_values.h"
#include "utils.h"

#define EEPROM_ATR 0x10

#define DEBUG 1					    // Set this to 1 to enable debug code
#define F_CPU 16000000UL  		    // Change this to the correct frequency (generally CLK = CLK_IO)
#define ICC_CLK_MODE 0              // Set to:
                                    // 0 for ICC_CLK = 4 MHz
                                    // 1 for ICC_CLK = 2 MHz
                                    // 2 for ICC_CLK = 1 MHz
                                    // 3 for ICC_CLK = 800 KHz
                                    // 4 for ICC_CLK = 500 KHz
#define ETU_TERMINAL 372
#define ETU_HALF(X) ((unsigned int) ((X)/2))
#define ETU_LESS_THAN_HALF(X) ((unsigned int) ((X)*0.46))
#define ETU_EXTENDED(X) ((unsigned int) ((X)*1.075))
#define ICC_VCC_DELAY_US 50   
#define PULL_UP_HIZ_ICC	1		    // Set to 1 to enable pull-ups when setting
								    // the I/O-ICC line to Hi-Z
                                

/* Hardcoded values for ICC clock - selected based on ICC_CLK_MODE above */
#if (ICC_CLK_MODE == 0)
#define ICC_CLK_OCR0A 1             // F_TIMER0 = CLK_IO / 4
#define ICC_CLK_TCCR1B 0x09         // F_TIMER1 = CLK_IO
#define ETU_ICC 1488                // 372 * 4
#define ICC_RST_WAIT 50000          // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 1)
#define ICC_CLK_OCR0A 3             // F_TIMER0 = CLK_IO / 8
#define ICC_CLK_TCCR1B 0x0A         // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 372                 // 372 * 1
#define ICC_RST_WAIT 100000         // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 2)
#define ICC_CLK_OCR0A 7             // F_TIMER0 = CLK_IO / 16
#define ICC_CLK_TCCR1B 0x0A         // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 744                 // 372 * 2
#define ICC_RST_WAIT 200000         // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 3)
#define ICC_CLK_OCR0A 9             // F_TIMER0 = CLK_IO / 20
#define ICC_CLK_TCCR1B 0x0A         // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 930                 // 372 * 2.5
#define ICC_RST_WAIT 250000         // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#elif (ICC_CLK_MODE == 4)
#define ICC_CLK_OCR0A 15            // F_TIMER0 = CLK_IO / 32
#define ICC_CLK_TCCR1B 0x0A         // F_TIMER1 = CLK_IO / 8
#define ETU_ICC 1488                // 372 * 4
#define ICC_RST_WAIT 400000         // Used for card reset; 50000 * ((CLK_IO / 4) / F_TIMER0)
#endif

/* SCD to Terminal functions */


/**
 * @return the frequency of the terminal clock in khz, zero if there is no clock
 *
 * Assumes the terminal counter is already started (Timer 3)
 */
uint16_t GetTerminalFreq()
{
	uint8_t sreg;
	uint16_t time, result;

	sreg = SREG;
	cli();	
	TCNT3 = 1;		// We need to be sure it will not restart in the process	
	asm volatile("nop\n\t"		
             	 "nop\n\t"
             	 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"
             	 "nop\n\t"
		 "nop\n\t"
		 "nop\n\t"  //49x
		 ::);
	time = TCNT3;	

	if(time == 1)
		result = 0;
	else
		result = (uint16_t)(((F_CPU/1000) * time) / 50);

	SREG = sreg;

	return result;
}

/**
 * @return the value of the terminal counter
 */
uint16_t ReadCounterTerminal()
{
	uint16_t i;
	uint8_t sreg;

	// do the reading (16-bit via 2 x 8-bit reads) with interrupts disabled	
	sreg = SREG;
	cli();	
	i = TCNT3;
	SREG = sreg;

	return i;	

	//or just: return Read16bitRegister(&TCNT3); but it might add more code
}


/**
 * Starts the counter for the external clock given by the terminal
 * 
 * From hardware tests it seems that it is better to just leave the
 * counter running all the time and just modify the register values
 * as needed
 */
void StartCounterTerminal()
{
	//TCCR3A = 0;							// No toggle on OC3X pins
	TCCR3A = 0x0C;						// set OC3C to 1 on compare match
	// This is needed because by some misterious hardware bug (i.e. not
	// as in the specs) the OC3C line (I/O for terminal) is changed
	// even if TCCR3A = 0

	Write16bitRegister(&OCR3A, ETU_TERMINAL);
	TCCR3B = 0x0F;						// CTC, timer external source
}

/**
 * Stops the terminal clock counter
 */
void StopCounterTerminal()
{
	TCCR3B = 0;
	Write16bitRegister(&TCNT3, 0); 		//TCNT3 = 0;	
}

/** 
 * Pauses the terminal clock counter
 */
void PauseCounterTerminal()
{
	TCCR3B = 0;
}

/**
 * Retrieves the state of the reset line from the terminal
 * 
 * @return 0 if reset is low, non-zero otherwise
 */
uint8_t GetResetStateTerminal()
{
	return bit_is_set(PIND, PD0);
}

/**
 * Waits (loops) for a number of nEtus based on the Terminal clock
 * 
 * @param nEtus the number of ETUs to loop
 * 
 * Assumes the terminal clock counter is already started
 */
void LoopTerminalETU(uint8_t nEtus)
{
	uint8_t i;
	
	Write16bitRegister(&OCR3A, ETU_TERMINAL);	// set ETU
	TCCR3A = 0x0C;								// set OC3C to 1
	Write16bitRegister(&TCNT3, 1);				// TCNT3 = 1	
	TIFR3 |= _BV(OCF3A);						// Reset OCR3A compare flag		

	for(i = 0; i < nEtus; i++)
	{
		while(bit_is_clear(TIFR3, OCF3A));
		TIFR3 |= _BV(OCF3A);
	}
}


/**
 * Sends a byte to the terminal without parity error retransmission
 * 
 * @param byte byte to be sent
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * 
 * The terminal clock counter must be started before calling this function
 */
void SendByteTerminalNoParity(uint8_t byte, uint8_t inverse_convention)
{
	uint8_t bitval, i, parity;
	volatile uint8_t tmp;	

	// check we have clock from terminal to avoid damage
	// assuming the counter is started
	if(!GetTerminalFreq())
		return;	

	// this code is needed to be sure that the I/O line will not
	// toggle to low when we set DDRC4 as output
	TCCR3A = 0x0C;								// Set OC3C on compare

	PORTC |= _BV(PC4);							// Put to high	
	DDRC |= _BV(PC4);							// Set PC4 (OC3C) as output	
	Write16bitRegister(&OCR3A, ETU_TERMINAL);	// set ETU
	Write16bitRegister(&TCNT3, 1);				// TCNT3 = 1	
	TIFR3 |= _BV(OCF3A);						// Reset OCR3A compare flag		

	// send each bit using OC3C (connected to the terminal I/O line)
	// each TCCR3A value will be visible after the next compare match

	// start bit
	TCCR3A = 0x08;

	// while sending the start bit convert the byte if necessary
	// to match inverse conversion	
	if(inverse_convention)
	{	
		tmp = ~byte;	
		byte = 0;	
		for(i = 0; i < 8; i++)
		{
			bitval = tmp & _BV((7-i));
			if(bitval) byte = byte | _BV(i);
		}
	}

	
	while(bit_is_clear(TIFR3, OCF3A));
	TIFR3 |= _BV(OCF3A);

	// byte value
	parity = 0;
	for(i = 0; i < 8; i++)
	{
		bitval = (uint8_t) (byte & (uint8_t)(1 << i));

		if(bitval != 0)
		{
			TCCR3A = 0x0C;
			if(!inverse_convention) parity = parity ^ 1;
		}
		else
		{
			TCCR3A = 0x08;		
			if(inverse_convention) parity = parity ^ 1;
		}
					
		while(bit_is_clear(TIFR3, OCF3A));
		TIFR3 |= _BV(OCF3A);
	}

	// parity bit
	if((!inverse_convention && parity != 0) || 
		(inverse_convention && parity == 0))
		TCCR3A = 0x0C;		
	else
		TCCR3A = 0x08;

	// wait for the last bit to be sent (need to toggle and
	// keep for ETU_TERMINAL clocks)
	while(bit_is_clear(TIFR3, OCF3A));
	TIFR3 |= _BV(OCF3A);	
	while(bit_is_clear(TIFR3, OCF3A));
	TIFR3 |= _BV(OCF3A);	

	// reset OC3C and put I/O to high (input)
	TCCR3A = 0x0C;						// set OC3C to 1
	DDRC &= ~(_BV(PC4));
	PORTC |= _BV(PC4);		 
}	

/**
 * Sends a byte to the terminal with parity error retransmission
 * 
 * @param byte byte to be sent
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @return 0 if successful, non-zero otherwise
 * 
 * As in the NoParity version, this function relies on counter started
 */
uint8_t SendByteTerminalParity(uint8_t byte, uint8_t inverse_convention)
{
	uint8_t i;
	volatile uint8_t tmp;

	SendByteTerminalNoParity(byte, inverse_convention);

	// wait for one ETU to read I/O line
	LoopTerminalETU(1);	

	// if there is aparity error try 4 times to resend
	if(bit_is_clear(PINC, PC4))
	{
		Write16bitRegister(&OCR3A, ETU_TERMINAL);	// set ETU
		Write16bitRegister(&TCNT3, 1);				// TCNT3 = 1	
		TIFR3 |= _BV(OCF3A);						// Reset OCR3A compare flag		
		TCCR3A = 0x0C;								// set OC3C to 1
		i = 0;

		do{
			i++;

			// wait 2 ETUs before resending
			LoopTerminalETU(2);	
			
			SendByteTerminalNoParity(byte, inverse_convention);			

			// wait for one ETU and read I/O line
			LoopTerminalETU(1);	
			tmp = bit_is_clear(PINC, PC4);
		}while(i<4 && tmp != 0);

		if(tmp != 0)
			return 1;		
	}

	return 0;
}

/**
 * Loops until the I/O line from terminal becomes low
 * or the number of clocks given as parameter elapses (forever if 0)
 * 
 * @return 0 if the I/O line is 0, non-zero otherwise
 */
uint8_t WaitForTerminalData(uint16_t max_cycles)
{
	uint8_t result = 0;
	uint16_t c = 0;
	volatile uint8_t bit;

	do{
		bit = bit_is_set(PINC, PC4);		
		c = c + 1;

		if(max_cycles != 0 && c == max_cycles) break;
	}while(bit != 0);
		

	if(bit != 0) result = 1;

	return result;	
}
		

/**
 * Receives a byte from the terminal without parity checking
 *  
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param r_byte contains the byte read on return
 * @return zero if read was successful, non-zero otherwise
 * 
 * Terminal clock counter must be already enabled
 */
uint8_t GetByteTerminalNoParity(uint8_t inverse_convention, uint8_t *r_byte)
{
	volatile uint8_t bit;
	uint8_t i, byte, parity;

	TCCR3A = 0x0C;										// set OC3C because of chip behavior
	DDRC &= ~(_BV(PC4));								// Set PC4 (OC3C) as input	
	PORTC |= _BV(PC4);									// enable pull-up	
	
	// wait for start bit	
	//loop_until_bit_is_clear(PINC, PC4);	// do NOT use this macro; is BUGGY!
	while(bit_is_set(PINC, PC4));	

	Write16bitRegister(&TCNT3, 1);						// TCNT3 = 1		
	//Write16bitRegister(&OCR3A, 180);					// OCR3A approx. 0.5 ETU
	Write16bitRegister(&OCR3A, ETU_HALF(ETU_TERMINAL));	// OCR3A approx. 0.5 ETU
	TIFR3 |= _BV(OCF3A);								// Reset OCR3A compare flag		

	while(bit_is_clear(TIFR3, OCF3A));
	TIFR3 |= _BV(OCF3A);

	// check result and set timer for next bit
	bit = bit_is_set(PINC, PC4);	
	Write16bitRegister(&OCR3A, ETU_TERMINAL);			// OCR3A = 1 ETU => next bit at 1.5 ETU
	*r_byte = 0;
	byte = 0;
	parity = 0;	
	if(bit)	return 1;	

	// read the byte in correct conversion mode
	for(i = 0; i < 8; i++)
	{
		while(bit_is_clear(TIFR3, OCF3A));
		TIFR3 |= _BV(OCF3A);
		bit = bit_is_set(PINC, PC4);

		if(inverse_convention && bit == 0)
		{
			byte = byte | _BV(7-i);
			parity = parity ^ 1;
		}
		else if(inverse_convention == 0 && bit != 0)
		{
			byte = byte | _BV(i);
			parity = parity ^ 1;
		}
	}

	*r_byte = byte;

	// read the parity bit
	while(bit_is_clear(TIFR3, OCF3A));
	TIFR3 |= _BV(OCF3A);
	bit = bit_is_set(PINC, PC4);
	
	// wait 0.5 ETUs to for parity bit to be completely received
	//Write16bitRegister(&OCR3A, 186);	
	Write16bitRegister(&OCR3A, ETU_HALF(ETU_TERMINAL));	
	while(bit_is_clear(TIFR3, OCF3A));
	TIFR3 |= _BV(OCF3A);	

	if(inverse_convention)
	{ 
		if(parity && bit) return 1;
		if(!parity && !bit) return 1;		
	}
	else
	{
		if(parity && !bit) return 1;
		if(!parity && bit) return 1;		
	}

	return 0;	
}

/**
 * Receives a byte from the terminal with parity checking
 *  
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param r_byte contains the byte read on return
 * @return zero if read was successful, non-zero otherwise
 * 
 * Terminal clock counter must be enabled before calling this function
 */
uint8_t GetByteTerminalParity(uint8_t inverse_convention, uint8_t *r_byte)
{
	uint8_t result;
	
	result = GetByteTerminalNoParity(inverse_convention, r_byte);
	if(result != 0)
	{
		// check we have clock from terminal to avoid damage
		if(!GetTerminalFreq())
			return result;

		// set I/O low for at least 1 ETU starting at 10.5 ETU from start bit	
		TCCR3A = 0x0C;							// OC3C set to 1 on compare		
		DDRC |= _BV(PC4);						// Set PC4 (OC3C) as output
		//Write16bitRegister(&OCR3A, 170);		// OCR3A ~= 0.5 ETU
		Write16bitRegister(&OCR3A, 
			ETU_LESS_THAN_HALF(ETU_TERMINAL));	// OCR3A ~= 0.5 ETU
		Write16bitRegister(&TCNT3, 1);			// TCNT3 = 1	
		TIFR3 |= _BV(OCF3A);					// Reset OCR3A compare flag	
		
		TCCR3A = 0x08;							// OC3C toggles to low on compare

		while(bit_is_clear(TIFR3, OCF3A));
		TIFR3 |= _BV(OCF3A);
		//Write16bitRegister(&OCR3A, 400);		// OCR3A > 1 ETU
		Write16bitRegister(&OCR3A, 
			ETU_EXTENDED(ETU_TERMINAL));		// OCR3A > 1 ETU		
		while(bit_is_clear(TIFR3, OCF3A));
		TIFR3 |= _BV(OCF3A);

		// set I/O to high (input)
		TCCR3A = 0x0C;						
		DDRC &= ~(_BV(PC4));
		PORTC |= _BV(PC4);

		// wait for the last ETU to complete
		//Write16bitRegister(&OCR3A, 158);
		Write16bitRegister(&OCR3A, ETU_LESS_THAN_HALF(ETU_TERMINAL));
		while(bit_is_clear(TIFR3, OCF3A));
		TIFR3 |= _BV(OCF3A);
	}

	return result;
}

/**
 * Sends default ATR for T=0 to terminal
 *
 * @param inverse_convention specifies if direct (0) or inverse
 * convention (non-zero) is to be used. Only direct convention should
 * be used for future applications.
 * @param TC1 specifies the TC1 byte of the ATR. This should be as
 * small as possible in order to limit the latency of communication,
 * or large if a large timeout between bytes is desired.
 */
void SendT0ATRTerminal(uint8_t inverse_convention, uint8_t TC1)
{
	if(inverse_convention)
		SendByteTerminalNoParity(0x3F, inverse_convention);
	else
		SendByteTerminalNoParity(0x3B, inverse_convention);
	
	LoopTerminalETU(250);
	SendByteTerminalNoParity(0x60, inverse_convention);
	LoopTerminalETU(2);
	SendByteTerminalNoParity(0x00, inverse_convention);
	LoopTerminalETU(2);
	SendByteTerminalNoParity(TC1, inverse_convention);
	LoopTerminalETU(2);
}


/* SCD to ICC functions */

/**
 * Returns non-zero if ICC is inserted, zero otherwise
 */
uint8_t IsICCInserted()
{
	return (!bit_is_set(PIND, PD1));
}


/**
 * Returns non-zero if ICC is powered up
 */
uint8_t IsICCPowered()
{
	return bit_is_clear(PIND, PD7);
}

/**
 * Powers up the card, if possible
 * 
 * @return 0 if power up was successful, non-zero otherwise
 */
uint8_t PowerUpICC()
{
	if(!IsICCInserted())
		return 1;

	PORTD &= ~(_BV(PD7));
	DDRD |= _BV(PD7);	

	return 0;
}

/**
 * Powers down the ICC
 */
void PowerDownICC()
{
	DDRD |= _BV(PD7);
	PORTD |= _BV(PD7);
}


/**
 * Waits (loops) for a number of nEtus based on the ICC clock
 *
 * @param nEtus the number of ETUs to loop
 * 
 * Assumes the ICC clock counter is already started
 */
void LoopICCETU(uint8_t nEtus)
{
	uint8_t i;
	
	Write16bitRegister(&OCR1A, ETU_ICC);	// set ETU
	TCCR1A = 0x30;							// set OC1B to 1 on compare match
	Write16bitRegister(&TCNT1, 1);			// TCNT1 = 1	
	TIFR1 |= _BV(OCF1A);					// Reset OCR1A compare flag		

	for(i = 0; i < nEtus; i++)
	{
		while(bit_is_clear(TIFR1, OCF1A));
		TIFR1 |= _BV(OCF1A);
	}
}

/**
 * Loops until the I/O line from ICC becomes low 
 *
 * @param max_cycles the maximum number of clocks to wait for
 * the I/O line to become low. Loops forever if this is 0
 * @return 0 if the I/O line is 0, non-zero otherwise
 */
uint8_t WaitForICCData(uint32_t max_cycles)
{
	uint8_t result = 0;
	uint32_t c = 0;
	volatile uint8_t bit;

	do{
		bit = bit_is_set(PINB, PB6);		
		c = c + 1;

		if(max_cycles != 0 && c == max_cycles) break;
	}while(bit != 0);
		

	if(bit != 0) result = 1;

	return result;	
}

/**
 * Receives a byte from the ICC without parity checking
 * 
 * @param inverse_convention different than 0 if inverse
 * convention is to be used * 
 * @param r_byte contains the byte read on return
 * @return zero if read was successful, non-zero otherwise
 * 
 * ICC clock counter must be already enabled
 */
uint8_t GetByteICCNoParity(uint8_t inverse_convention, uint8_t *r_byte)
{
	volatile uint8_t bit;
	uint8_t i, byte, parity;

	TCCR1A = 0x30;									// set OC1B to 1 on compare match
	DDRB &= ~(_BV(PB6));							// Set I/O (PB6) to reception mode

#if PULL_UP_HIZ_ICC
	PORTB |= _BV(PB6);	
#else
	PORTB &= ~(_BV(PB6));				
#endif	

	// wait for start bit		
	while(bit_is_set(PINB, PB6)); //start bit goes low for 1ETU	

	TIFR1 |= _BV(OCF1A);							// Reset OCR1A compare fl
	Write16bitRegister(&TCNT1, 1);					// TCNT1 = 1		
	Write16bitRegister(&OCR1A, ETU_HALF(ETU_ICC));	// OCR1A 0.5 ETU

	while(bit_is_clear(TIFR1, OCF1A));
	TIFR1 |= _BV(OCF1A);

	// check result and set timer for next bit
	bit = bit_is_set(PINB, PB6);	// this is LO = start bit
	Write16bitRegister(&OCR1A, ETU_ICC);			// OCR1A = 1 ETU => next bit at 1.5 ETU
	*r_byte = 0;
	byte = 0;
	parity = 0;	
	if(bit)	return 1;	

	// read the byte in correct conversion mode
	for(i = 0; i < 8; i++)
	{
		while(bit_is_clear(TIFR1, OCF1A));
		TIFR1 |= _BV(OCF1A);
		bit = bit_is_set(PINB, PB6);

		if(inverse_convention && bit == 0)
		{
			byte = byte | _BV(7-i);
			parity = parity ^ 1;
		}
		else if(inverse_convention == 0 && bit != 0)
		{
			byte = byte | _BV(i);
			parity = parity ^ 1;
		}
	}

	*r_byte = byte;

	// read the parity bit
	while(bit_is_clear(TIFR1, OCF1A));
	TIFR1 |= _BV(OCF1A);
	bit = bit_is_set(PINB, PB6);
	
	// wait 0.5 ETUs to for parity bit to be completely received
	Write16bitRegister(&OCR1A, ETU_HALF(ETU_ICC));	
	while(bit_is_clear(TIFR1, OCF1A));
	TIFR1 |= _BV(OCF1A);		

	if(inverse_convention)
	{ 
		if(parity && bit) return 1;
		if(!parity && !bit) return 1;		
	}
	else
	{
		if(parity && !bit) return 1;
		if(!parity && bit) return 1;		
	}

	return 0;	
}


/**
 * Receives a byte from the ICC with parity checking
 * 
 * @param inverse_convention different than 0 if inverse
 * convention is to be used * 
 * @param r_byte contains the byte read on return
 * @return zero if read was successful, non-zero otherwise
 * 
 * ICC clock counter must be already enabled
 */
uint8_t GetByteICCParity(uint8_t inverse_convention, uint8_t *r_byte)
{
	uint8_t result;
	
	result = GetByteICCNoParity(inverse_convention, r_byte);
	if(result != 0)
	{
		// check the card is still inserted
		if(!IsICCInserted())
			return result;

		// set I/O low for at least 1 ETU starting at 10.5 ETU from start bit			
		TCCR1A = 0x30;							// set OC1B on compare match
		DDRB |= _BV(PB6);						// Set PB6 (OC1B) as output		
		Write16bitRegister(&OCR1A, 
			ETU_LESS_THAN_HALF(ETU_ICC));		
		Write16bitRegister(&TCNT1, 1);					
		TIFR1 |= _BV(OCF1A);					// Reset OCF1A compare flag	
		TCCR1A = 0x20;							// clear OC1B on compare match

		while(bit_is_clear(TIFR1, OCF1A));
		TIFR1 |= _BV(OCF1A);		
		Write16bitRegister(&OCR1A, 
			ETU_EXTENDED(ETU_ICC));				// OCR1A > 1 ETU		
		while(bit_is_clear(TIFR1, OCF1A));
		TIFR1 |= _BV(OCF1A);

		// set I/O to high (input)
		TCCR1A = 0x30;						
		DDRB &= ~(_BV(PB6));
		PORTB |= _BV(PB6);

		// wait for the last ETU to complete
		Write16bitRegister(&OCR1A, ETU_LESS_THAN_HALF(ETU_ICC));
		while(bit_is_clear(TIFR1, OCF1A));
		TIFR1 |= _BV(OCF1A);
	}

	return result;
}

/**
 * Sends a byte to the ICC without parity error retransmission
 * @param byte byte to be sent
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * 
 * The ICC clock counter must be started before calling this function
 */
void SendByteICCNoParity(uint8_t byte, uint8_t inverse_convention)
{
	uint8_t bitval, i, parity;
	volatile uint8_t tmp;	

	if(!IsICCInserted())
		return;	

	// this code is needed to be sure that the I/O line will not
	// toggle to low when we set DDRB6 as output
	TCCR1A = 0x30;								// Set OC1B on compare
	PORTB |= _BV(PB6);							// Put to high	
	DDRB |= _BV(PB6);							// Set PB6 (OC1B) as output	
	Write16bitRegister(&OCR1A, ETU_ICC);	
	Write16bitRegister(&TCNT1, 1);
	TIFR1 |= _BV(OCF1A);						// Reset OCF1A compare flag		

	// send each bit using OC1B (connected to the ICC I/O line)
	// each TCCR1A value will be visible after the next compare match

	// start bit
	TCCR1A = 0x20;

	// while sending the start bit convert the byte if necessary
	// to match inverse conversion	
	if(inverse_convention)
	{	
		tmp = ~byte;	
		byte = 0;	
		for(i = 0; i < 8; i++)
		{
			bitval = tmp & _BV((7-i));
			if(bitval) byte = byte | _BV(i);
		}
	}

	
	while(bit_is_clear(TIFR1, OCF1A));
	TIFR1 |= _BV(OCF1A);

	// byte value
	parity = 0;
	for(i = 0; i < 8; i++)
	{
		bitval = (uint8_t) (byte & (uint8_t)(1 << i));

		if(bitval != 0)
		{
			TCCR1A = 0x30;
			if(!inverse_convention) parity = parity ^ 1;
		}
		else
		{
			TCCR1A = 0x20;		
			if(inverse_convention) parity = parity ^ 1;
		}
					
		while(bit_is_clear(TIFR1, OCF1A));
		TIFR1 |= _BV(OCF1A);
	}

	// parity bit
	if((!inverse_convention && parity != 0) ||
		(inverse_convention && parity == 0))
		TCCR1A = 0x30;		
	else
		TCCR1A = 0x20;

	// wait for the last bit to be sent (need to toggle and
	// keep for ETU_TERMINAL clocks)
	while(bit_is_clear(TIFR1, OCF1A));
	TIFR1 |= _BV(OCF1A);	
	while(bit_is_clear(TIFR1, OCF1A));
	TIFR1 |= _BV(OCF1A);	

	// reset OC1B and put I/O to high (input)
	TCCR1A = 0x30;						
	DDRB &= ~(_BV(PB6));
	PORTB |= _BV(PB6);		 
}	

/**
 * Sends a byte to the ICC with parity error retransmission
 * 
 * @param byte byte to be sent
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @return 0 if successful, non-zero otherwise
 * 
 * As in the No Parity version, this function relies on counter started
 */
uint8_t SendByteICCParity(uint8_t byte, uint8_t inverse_convention)
{
	uint8_t i;
	volatile uint8_t tmp;

	SendByteICCNoParity(byte, inverse_convention);

	// wait for one ETU to read I/O line
	LoopICCETU(1);	

	// if there is aparity error try 4 times to resend
	if(bit_is_clear(PINB, PB6))
	{
		Write16bitRegister(&OCR1A, ETU_ICC);	
		Write16bitRegister(&TCNT1, 1);			
		TIFR1 |= _BV(OCF1A);					// Reset OCF1A compare flag		
		TCCR1A = 0x30;							// set OC1B to 1
		i = 0;

		do{
			i++;

			// wait 2 ETUs before resending
			LoopICCETU(2);	
			
			SendByteICCNoParity(byte, inverse_convention);			

			// wait for one ETU and read I/O line
			LoopICCETU(1);	
			tmp = bit_is_clear(PINB, PB6);
		}while(i<4 && tmp != 0);

		if(tmp != 0)
			return 1;		
	}

	return 0;
}



/**
 * Receives the ATR from ICC after a successful activation
 * 
 * @param inverse_convention non-zero if inverse convention
 * is to be used
 * @param proto 0 for T=0 and non-zero for T=1
 * @param TC1 see ISO 7816-3 or EMV Book 1 section ATR
 * @param TA3 see ISO 7816-3 or EMV Book 1 section ATR
 * @param TB3 see ISO 7816-3 or EMV Book 1 section ATR
 * @return zero if successful, non-zero otherwise
 *
 * This implementation is compliant with EMV 4.2 Book 1
 */
uint8_t GetATRICC(uint8_t *inverse_convention, uint8_t *proto,
					uint8_t *TC1, uint8_t *TA3, uint8_t *TB3)
{
	uint8_t history, i, tmp, ta, tb, tc, td, nb;
	uint8_t check = 0; // used only for T=1
	uint16_t offset = 0;
        uint8_t buffer[32];

Led3On();
	// Get TS
	GetByteICCNoParity(0, &tmp);
	buffer[offset++]=tmp; offset = offset%32;
	if(tmp == 0x3B) *inverse_convention = 0;
	else if(tmp == 0x03) *inverse_convention = 1;
	else return RET_ERR_INIT_ICC_ATR_TS;
		
	// Get T0
	GetByteICCNoParity(*inverse_convention, &tmp);
	buffer[offset++]=tmp; offset = offset%32;
	check ^= tmp;
	history = tmp & 0x0F;
	ta = tmp & 0x10;
	tb = tmp & 0x20;
	tc = tmp & 0x40;
	td = tmp & 0x80;
	if(tb == 0) return RET_ERR_INIT_ICC_ATR_T0;	

	if(ta){
		// Get TA1, coded as [FI, DI], where FI and DI are used to derive
        // the work etu. ETU = (1/D) * (F/f) where f is the clock frequency.
        // From ISO/IEC 7816-3 pag 12, F and D are mapped to FI/DI as follows:
        //
        // FI:  0x1  0x2  0x3  0x4  0x5  0x6  0x9  0xA  0xB  0xC  0xD
        // F:   372  558  744  1116 1488 1860 512  768  1024 1536 2048 
        //
        // DI:  0x1 0x2 0x3 0x4 0x5 0x6 0x8 0x9 0xA 0xB 0xC 0xD  0xE  0xF
        // D:   1   2   4   8   16  32  12  20  1/2 1/4 1/8 1/16 1/32 1/64
        //
        // For the moment the SCD only works with D = 1, F = 372
        // which should be used even for different values of TA1 if the
        // negotiable mode of operation is selected (abscence of TA2)
		GetByteICCNoParity(*inverse_convention, &tmp);
		buffer[offset++]=tmp; offset = offset%32;
		check ^= tmp;
	}

	// Get TB1
	GetByteICCNoParity(*inverse_convention, &tmp);
	buffer[offset++]=tmp; offset = offset%32;
	check ^= tmp;
	//DC   if(tmp != 0) return RET_ERR_INIT_ICC_ATR_TB1;
	
	// Get TC1
	if(tc)
	{
		GetByteICCNoParity(*inverse_convention, TC1);
		buffer[offset++]=tmp; offset = offset%32;
		check ^= tmp;
	}
	else
		*TC1 = 0;

	if(td){
		// Get TD1
		GetByteICCNoParity(*inverse_convention, &tmp);
		buffer[offset++]=tmp; offset = offset%32;
		check ^= tmp;
		nb = tmp & 0x0F;
		ta = tmp & 0x10;
		tb = tmp & 0x20;
		tc = tmp & 0x40;
		td = tmp & 0x80;
		if(nb == 0x01) *proto = 1;
		else if(nb == 0x00) *proto = 0;
		else return RET_ERR_INIT_ICC_ATR_TD1;

        // The SCD does not currently support specific modes of operation.
        // Perhaps we can trigger a PTS selection or reset in the future.
		if(ta) return RET_ERR_INIT_ICC_ATR_TA2;

		if(tb) return RET_ERR_INIT_ICC_ATR_TB2;
		if(tc){
			// Get TC2
			GetByteICCNoParity(*inverse_convention, &tmp);
			buffer[offset++]=tmp; offset = offset%32;
			check ^= tmp;
			if(tmp != 0x0A) return RET_ERR_INIT_ICC_ATR_TC2;
		}
		if(td){
			// Get TD2
			GetByteICCNoParity(*inverse_convention, &tmp);
			buffer[offset++]=tmp; offset = offset%32;
			check ^= tmp;
			nb = tmp & 0x0F;
			ta = tmp & 0x10;
			tb = tmp & 0x20;
			tc = tmp & 0x40;
			td = tmp & 0x80;
            // we allow any value of nb although EMV restricts to some values
            // these values could be used if we implement PTS

			if(ta)
			{	
				// Get TA3
				GetByteICCNoParity(*inverse_convention, &tmp);
				buffer[offset++]=tmp; offset = offset%32;
				check ^= tmp;
				if(tmp < 0x0F || tmp == 0xFF) return RET_ERR_INIT_ICC_ATR_TA3;
				*TA3 = tmp;
			}
			else
				*TA3 = 0x20;

			if(*proto == 1 && tb == 0) return RET_ERR_INIT_ICC_ATR_TB3;
			if(tb)
			{
				// Get TB3
				GetByteICCNoParity(*inverse_convention, &tmp);
				buffer[offset++]=tmp; offset = offset%32;
				check ^= tmp;
				nb = tmp & 0x0F;
				if(nb > 5) return RET_ERR_INIT_ICC_ATR_TB3;
				nb = tmp & 0xF0;
				if(nb > 64) return RET_ERR_INIT_ICC_ATR_TB3;
				*TB3 = tmp;				
			}

			if(*proto == 0 && tc != 0) return RET_ERR_INIT_ICC_ATR_TC3;
			if(tc)
			{
				// Get TC3
				GetByteICCNoParity(*inverse_convention, &tmp);
				buffer[offset++]=tmp; offset = offset%32;
				check ^= tmp;
				if(tmp != 0) return RET_ERR_INIT_ICC_ATR_TC3;
			}
		}		
	} 
	else 
		*proto = 0;
	
	// Get historical bytes
//DC - the next loop does not work for some reason, I suspect problem with timing -> wrong ATR
	for(i = 0; i < history; i++)	
	{
		GetByteICCNoParity(0, &tmp);
		buffer[offset++]=tmp; offset = offset%32;
		check ^= tmp;
	}

	// get TCK if T=1 is used
	if(*proto == 1) 
	{
		GetByteICCNoParity(*inverse_convention, &tmp);
		buffer[offset++]=tmp; offset = offset%32;
		check ^= tmp;
		if(check != 0) return RET_ERR_INIT_ICC_ATR_T1_CHECK;
	}

	eeprom_write_block(buffer, (void*)EEPROM_ATR, offset);
	return 0;
}

/**
 * Starts activation sequence for ICC
 * 
 * @param warm 0 if a cold reset is to be issued, 1 otherwise
 * @param inverse_convention non-zero if inverse convention
 * is to be used
 * @param proto 0 for T=0 and non-zero for T=1
 * @param TC1 see ISO 7816-3 or EMV Book 1 section ATR
 * @param TA3 see ISO 7816-3 or EMV Book 1 section ATR
 * @param TB3 see ISO 7816-3 or EMV Book 1 section ATR
 * @return zero if successful, non-zero otherwise
 */
uint8_t ResetICC(uint8_t warm, uint8_t *inverse_convention, uint8_t *proto,
					uint8_t *TC1, uint8_t *TA3, uint8_t *TB3)
{		
    uint8_t response;

	// Activate the ICC
	if(ActivateICC(warm)) return RET_ERR_INIT_ICC_ACTIVATE;	

	// Wait for approx 42000 ICC clocks = 112 ETUs
	LoopICCETU(112);
	
	// Set RST to high
	PORTD |= _BV(PD4);		
	
	// Wait for ATR from ICC for a maximum of 42000 ICC clock cycles + 40 ms
	if(WaitForICCData(ICC_RST_WAIT))	
	{		
		if(warm == 0) 
			return ResetICC(1, inverse_convention, proto, TC1, TA3, TB3);						

		DeactivateICC();
		return RET_ERR_INIT_ICC_RESPONSE;
	}

	// Get ATR
	response = GetATRICC(inverse_convention, proto, TC1, TA3, TB3);
	if(response)
	{
		if(warm == 0)
			return ResetICC(1, inverse_convention, proto, TC1, TA3, TB3);						

		DeactivateICC();
		return response;
	}
	
	return 0;
}

/**
 * Starts the activation sequence for the ICC
 *
 * @param warm if warm=1 it specifies a warm reset activation,
 * cold reset otherwise *
 * @return 0 if successful, non-zero otherwise
 */
uint8_t ActivateICC(uint8_t warm)
{
	if(warm)
	{
		// Put RST to low
		PORTD &= ~(_BV(PD4));	
		DDRD |= _BV(PD4);	
	}
	else{	
		// Put I/O, CLK and RST lines to 0 and give VCC
		PORTB &= ~(_BV(PB6));
		DDRB |= _BV(PB6);	
		PORTB &= ~(_BV(PB7));
		DDRB |= _BV(PB7);	
		PORTD &= ~(_BV(PD4));
		DDRD |= _BV(PD4);	
		_delay_us(ICC_VCC_DELAY_US);
		if(PowerUpICC())
		{
			DeactivateICC();
			return 1;
		}
		_delay_us(ICC_VCC_DELAY_US);
	}

	// Put I/O to reception mode and then give clock if cold reset
	DDRB &= ~(_BV(PB6));				// Set I/O to reception mode
#if PULL_UP_HIZ_ICC
	PORTB |= _BV(PB6);	
#else
	PORTB &= ~(_BV(PB6));				
#endif

	if(warm == 0)
	{	
		// I use the Timer 0 (8-bit) to give the clock to the ICC
		// and the Timer 1 (16-bit) to count the number of clocks
		// in order to provide the correct ETU reference		
		TCCR0A = 0x42;						// toggle OC0A (PB7) on compare match, CTC mode
		OCR0A = ICC_CLK_OCR0A;			    // set F_TIMER0 = CLK_IO / (2 * (ICC_CLK_OCR0A + 1));
		TCNT0 = 0;
		TCCR0B = 0x01;						// Start timer 0, CLK = CLK_IO

		TCCR1A = 0x30;						// set OC1B (PB6) to 1 on compare match
		Write16bitRegister(&OCR1A, ETU_ICC);// ETU = 372 * (F_TIMER1 / F_TIMER0)
		TCCR1B = ICC_CLK_TCCR1B;		    // Start timer 1, CTC, CLK based on TCCR1B
		TCCR1C = 0x40;						// Force compare match on OC1B so that
											// we get the I/O line to high	
	}

	return 0;
}

/**
 * Starts the deactivation sequence for the ICC
 */
void DeactivateICC()
{
	// Set reset to low 
	PORTD &= ~(_BV(PD4));
	DDRD |= _BV(PD4);	

	// Stop timers
	TCCR0A = 0;
	TCCR0B = 0;
	TCCR1A = 0;
	TCCR1B = 0;	

	// Set CLK line to low to be sure
	PORTB &= ~(_BV(PB7));
	DDRB |= _BV(PB7);	

	// Set I/O line to low
	PORTB &= ~(_BV(PB6));
	DDRB |= _BV(PB6);

	// Depower VCC
	PORTD |= _BV(PD7);
	DDRD |= _BV(PD7);	
}


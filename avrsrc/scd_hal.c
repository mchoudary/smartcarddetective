/**
 * \file
 * \brief scd_hal.c - SCD hardware abstraction layer source file for AT90USB1287
 *
 * This file implements the hardware abstraction layer functions
 *
 * This functions are implemented specifically for each microcontroller
 * but the function names should be the same for any microcontroller.
 * Thus the definition halSCD.h should be the same, while only the
 * implementation should differ.
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

#include <avr/interrupt.h> 
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "scd_hal.h"
#include "scd_io.h"
#include "scd_values.h"
#include "utils.h"

#define DEBUG 1					    // Set this to 1 to enable debug code

/* Global Variables */
volatile uint32_t syncCounter;      // counter updated regularly, e.g. by timer 2

/* SCD to Terminal functions */


/**
 * Enable the terminal reset interrupt. This interrupt should fire
 * when the terminal reset line goes low (0).
 *
 * Enable INT0 on falling edge. This is the recommended procedure, as in the
 * data sheet. First disable the interrupt, then change the mode, then
 * clear the interrupt flag and finally enable the interrupt
 */
void EnableTerminalResetInterrupt()
{
    EIMSK &= ~(_BV(INT0));
    EICRA |= _BV(ISC01);
    EICRA &= ~(_BV(ISC00));
    EIFR |= _BV(INTF0);
    EIMSK |= _BV(INT0);
}

/**
 * Disable the terminal reset interrupt. This interrupt should fire
 * when the terminal reset line goes low (0).
 */
void DisableTerminalResetInterrupt()
{
    EIMSK &= ~(_BV(INT0));
}

/**
 * Get the status of the terminal I/O line
 *
 * @return 1 if the line is high or 0 if low.
 *
 * Assumes the terminal counter is already started (Timer 3)
 */
uint8_t GetTerminalIOLine()
{
    return bit_is_set(PINC, PC4);
}

/**
 * Retrieves the state of the reset line from the terminal
 * 
 * @return 0 if reset is low, non-zero otherwise
 */
uint8_t GetTerminalResetLine()
{
    return bit_is_set(PIND, PD0);
}

/**
 * Enable the Watch Dog Timer. This function will also enable the WDT
 * interrupt.
 *
 * @param ms the number of miliseconds to wait. This is an estimate since each
 * hardware platform might provide just a limited set of possible values.
 * For the AT90USB1287 (this uC) the possible values are 15ms, 30ms, 60ms,
 * 120ms, 250 ms, 500 ms, 1s, 2s, 4s and 8s. Therefore this function simply
 * approximates the closest larger value. For example sending a value of 14 will
 * actually use 15ms, sending 25 will use 30ms, sending 100 will use 120ms and
 * sending 1000 will use 1s.
 */
void EnableWDT(uint16_t ms)
{
    if(ms <= 15)
        wdt_enable(WDTO_15MS);
    else if(ms <= 30)
        wdt_enable(WDTO_30MS);
    else if(ms <= 60)
        wdt_enable(WDTO_60MS);
    else if(ms <= 120)
        wdt_enable(WDTO_120MS);
    else if(ms <= 250)
        wdt_enable(WDTO_250MS);
    else if(ms <= 500)
        wdt_enable(WDTO_500MS);
    else if(ms <= 1000)
        wdt_enable(WDTO_1S);
    else if(ms <= 2000)
        wdt_enable(WDTO_2S);
    else if(ms <= 4000)
        wdt_enable(WDTO_4S);
    else
        wdt_enable(WDTO_8S);

    WDTCSR |= _BV(WDIE);
}

/**
 * Disable the Watch Dog Timer and the WDT interrupt
 */
void DisableWDT()
{
    wdt_disable();
    WDTCSR &= ~(_BV(WDIE));
}

/**
 * Reset the Watch Dog Timer
 */
void ResetWDT()
{
    wdt_reset();
}

/**
 * Loops until the IO or reset line from the terminal become low.
 * 
 * @param max_wait the maximum number of cycles to wait for the reset or the
 * IO line to become low. Give 0 to wait indefinitely.
 * @return 1 if reset is low, 2 if the I/O is low, or 3 if both lines are low.
 * In case the maximum number of wait cycles has elapsed then this function
 * returns 0.
 */
uint8_t WaitTerminalResetIOLow(uint32_t max_wait)
{
    volatile uint8_t tio, treset;
    uint8_t result = 0;
    uint32_t cnt = 0;

    do{
        cnt = cnt + 1;
        tio = bit_is_clear(PINC, PC4);
        treset = bit_is_clear(PIND, PD0);
        result = (tio << 1) | treset;

        if(max_wait != 0 && cnt == max_wait)
            break;
    }while(result == 0);

    return result;
}

/**
 * @return non-zero if we have some terminal clock, zero otherwise.
 *
 * Assumes the terminal counter is already started (Timer 3)
 */
uint16_t IsTerminalClock()
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
            ::);
    time = TCNT3;	
    result = time - 1;
    SREG = sreg;

    return result;
}

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
            "nop\n\t"
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
 * The timer T2 can be used for event management.
 * It is an 8-bit counter.
 *
 * @return the value of the timer T2
 * @sa StartTimerT2
 */
uint8_t ReadTimerT2()
{
    return TCNT2;	
}


/**
 * Starts the timer T2 using the internal clock CLK_IO.
 * The current setup is for an interrupt frequency f_t2_int = 976.5625 Hz.
 * That means that each value of the udpated counter represents 1.024 ms.
 * 
 * @sa ReadTimerT2
 */
void StartTimerT2()
{
    // We use this to generate an interrupt with the given frequency
    OCR2A = 16;                     // interrupt every 16 timer clocks
    TIMSK2 |= _BV(OCIE2A);

    TCNT2 = 0;
    TCCR2A = _BV(WGM21);			// CTC mode, No toggle on OC2X pins, no PWM
    TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);  // F_CLK_T2 = F_CLK_IO / 1024
}

/**
 * Stops the timer T2
 */
void StopTimerT2()
{
    TCCR2B = 0;
    TCCR2A = 0;
    TIMSK2 = 0;
    OCR2A = 0;
}

/**
 * This method increments the synchronization counter.
 * The sync counter can be used as a synchronization mechanism.
 * It is a 32-bit value, which should be updated regularly, e.g. by the timer T2.
 *
 * @sa ReadTimerT2
 */
//void IncrementCounter()
//{
//	syncCounter++;
//}

/**
 * This method returns the value of the sync counter.
 *
 * @return the value of the sync counter
 * @sa IncrementCounter
 */
//uint32_t GetCounter()
//{
//	return syncCounter;
//}

/**
 * This method resets to 0 the value of the sync counter.
 *
 * @sa IncrementCounter
 */
//void ResetCounter()
//{
//	syncCounter = 0;
//}

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
 * Waits (loops) for a number of nEtus based on the Terminal clock
 * 
 * @param nEtus the number of ETUs to loop
 * @return zero if completed, non-zero if there is no more terminal clock
 * 
 * Assumes the terminal clock counter is already started
 */
uint8_t LoopTerminalETU(uint32_t nEtus)
{
    uint32_t i, k;
    uint8_t done;

    Write16bitRegister(&OCR3A, ETU_TERMINAL);	// set ETU
    TCCR3A = 0x0C;								// set OC3C to 1
    Write16bitRegister(&TCNT3, 1);				// TCNT3 = 1	
    TIFR3 |= _BV(OCF3A);						// Reset OCR3A compare flag		

    for(i = 0; i < nEtus; i++)
    {
        done = 0;
        for(k = 0; k < MAX_WAIT_TERMINAL; k++)
        {
            if(bit_is_set(TIFR3, OCF3A))
            {
                done = 1;
                break;
            }
        }
        TIFR3 |= _BV(OCF3A);
        if(done == 0)
            return RET_TERMINAL_TIME_OUT;
    }

    return 0;
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

    // if there is a parity error try 4 times to resend
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
 * Receives a byte from the terminal without parity checking.
 * This function also checks if the terminal reset line is low.
 *  
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param r_byte contains the byte read on return
 * @param max_wait the maximum number of cycles to wait for the reset or the
 * IO line to become low. Give 0 to wait indefinitely.
 * @return zero if read was successful, RET_TERMINAL_RESET_LOW if the terminal
 * reset line was low while waiting, RET_TERMINAL_TIME_OUT if we did not
 * get any signal within the specified max_wait period, or RET_ERROR otherwise
 * 
 * Terminal clock counter must be already enabled
 */
uint8_t GetByteTerminalNoParity(
        uint8_t inverse_convention,
        uint8_t *r_byte,
        uint32_t max_wait)
{
    volatile uint8_t bit;
    volatile uint8_t tio, treset;
    uint8_t i, byte, parity;
    uint32_t cnt;

    TCCR3A = 0x0C;										// set OC3C because of chip behavior
    DDRC &= ~(_BV(PC4));								// Set PC4 (OC3C) as input	
    PORTC |= _BV(PC4);									// enable pull-up	

    // wait for reset or start bit
    cnt = 0;
    while(1)
    {
        cnt = cnt + 1;

        // check we have clock from terminal
        if(!IsTerminalClock())
            return RET_TERMINAL_NO_CLOCK;

        // check for terminal reset (reset low)
        treset = bit_is_clear(PIND, PD0);
        if(treset)
            return RET_TERMINAL_RESET_LOW;

        // check for start bit (Terminal I/O low)
        tio = bit_is_clear(PINC, PC4);
        if(tio)
            break;

        if(max_wait != 0 && cnt == max_wait)
            return RET_TERMINAL_TIME_OUT;
    }

    Write16bitRegister(&TCNT3, 1);						// TCNT3 = 1		
    Write16bitRegister(&OCR3A, ETU_HALF(ETU_TERMINAL));	// OCR3A approx. 0.5 ETU
    TIFR3 |= _BV(OCF3A);								// Reset OCR3A compare flag		

    // Wait until the timer/counter 3 reaches the value in OCR3A
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
 * @param max_wait the maximum number of cycles to wait for the reset or the
 * IO line to become low. Give 0 to wait indefinitely.
 * @return zero if read was successful, RET_TERMINAL_RESET_LOW if the terminal
 * reset line was low while waiting, RET_TERMINAL_TIME_OUT if we did not
 * get any signal within the specified max_wait period, or RET_ERROR otherwise
 * 
 * Terminal clock counter must be enabled before calling this function
 */
uint8_t GetByteTerminalParity(
        uint8_t inverse_convention,
        uint8_t *r_byte,
        uint32_t max_wait)
{
    uint8_t result;

    result = GetByteTerminalNoParity(inverse_convention, r_byte, max_wait);
    if(result == RET_ERROR)
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

/* SCD to ICC functions */

/**
 * Returns non-zero if ICC is inserted, zero otherwise
 */
uint8_t IsICCInserted()
{
#ifdef INVERT_ICC_SWITCH
    return bit_is_clear(PIND, PD1);
#else
    return bit_is_set(PIND, PD1);
#endif
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
    while(bit_is_set(PINB, PB6));	

    Write16bitRegister(&TCNT1, 1);					// TCNT1 = 1		
    Write16bitRegister(&OCR1A, ETU_HALF(ETU_ICC));	// OCR1A 0.5 ETU
    TIFR1 |= _BV(OCF1A);							// Reset OCR1A compare flag		

    while(bit_is_clear(TIFR1, OCF1A));
    TIFR1 |= _BV(OCF1A);

    // check result and set timer for next bit
    bit = bit_is_set(PINB, PB6);	
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
 * Sets the reset line of the ICC to the desired value
 *
 * @param high send 0 to put the reset line to low (0), or non-zero to
 * put the reset line to high (1)
 */
void SetICCResetLine(uint8_t high)
{
    if(high)
        PORTD |= _BV(PD4);		
    else
        PORTD &= ~(_BV(PD4));
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
#if ICC_CLK_OCR0A
        PORTB &= ~(_BV(PB7));
        DDRB |= _BV(PB7);	
#else
        // In the case of an external clock we don't want the MCU to receive input
        PORTB &= ~(_BV(PB7));
        DDRB &= ~(_BV(PB7));	
#endif
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
        OCR0A = ICC_CLK_OCR0A;			    // set F_TIMER0 = CLK_IO / (2 * (ICC_CLK_OCR0A + 1));
        TCNT0 = 0;
#if ICC_CLK_OCR0A
        TCCR0A = 0x42;						// toggle OC0A (PB7) on compare match, CTC mode
        TCCR0B = 0x01;						// Start timer 0, CLK = CLK_IO
#else
        TCCR0A = 0;						    // Timer 0 not used for external clock
        TCCR0B = 0;						    // Timer 0 not used for external clock
#endif

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

#if ICC_CLK_OCR0A
    // Set CLK line to low to be sure
    PORTB &= ~(_BV(PB7));
    DDRB |= _BV(PB7);	
#endif

    // Set I/O line to low
    PORTB &= ~(_BV(PB6));
    DDRB |= _BV(PB6);

    // Depower VCC
    PORTD |= _BV(PD7);
    DDRD |= _BV(PD7);	
}

/**
 * Enable the ICC insert interrupt on any edge trigger
 */
void EnableICCInsertInterrupt()
{
    EICRA |= _BV(ISC10);
    EICRA &= ~(_BV(ISC11));
    EIMSK |= _BV(INT1);
}

/**
 * Disable the ICC insert interrupt
 */
void DisableICCInsertInterrupt()
{
    EIMSK &= ~(_BV(INT1));
}


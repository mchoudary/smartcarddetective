/**
 * \file
 * \brief scd_io.h header file
 *
 * This file provides the function definitions for all micro-controller
 * I/O functions, including control of LCD, leds and buttons
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


#ifndef _SCD_IO_H_
#define _SCD_IO_H_

#include<stdio.h>
#include<avr/io.h>

/// Delay of LCD commands
#define LCD_COMMAND_DELAY 40

/// value for Button A in result from GetButton function
#define BUTTON_A 0x01

/// value for Button B in result from GetButton function
#define BUTTON_B 0x02

/// value for Button C in result from GetButton function
#define BUTTON_C 0x04

/// value for Button D in result from GetButton function
#define BUTTON_D 0x08

// this is needed for the delay on the new avr-libc-1.7.0
#ifndef __DELAY_BACKWARD_COMPATIBLE__
#define __DELAY_BACKWARD_COMPATIBLE__
#endif


/* Led functions */

/// Turn on Led1
void Led1On();

/// Turn on Led2
void Led2On();

/// Turn on Led3
void Led3On();

/// Turn on Led4
void Led4On();

/// Turn off Led1
void Led1Off();

/// Turn off Led2
void Led2Off();

/// Turn off Led3
void Led3Off();

/// Turn off Led4
void Led4Off();

/* Other signals */
void T_C4On();
void T_C8On();
void T_C4Off();
void T_C8Off();
void JTAG_P1_High();
void JTAG_P3_High();
void JTAG_P1_Low();
void JTAG_P3_Low();

/* Button functions */

/// Get status of button A
uint8_t GetButtonA();

/// Get status of button B
uint8_t GetButtonB();

/// Get status of button C
uint8_t GetButtonC();

/// Get status of button D
uint8_t GetButtonD();

/// Returs a 1-hot encoded list of buttons pressed
uint8_t GetButton();

/* LCD functions */

///Return the state (on/off) of the LCD
uint8_t GetLCDState();

/// Set the state of the LCD
void SetLCDState(uint8_t state);

/// Initialize LCD
void InitLCD();

/// Display a string on LCD
void WriteStringLCD(char *string, uint8_t len);

/// Send character to the LCD display
int LcdPutchar(char c, FILE *unused);

/// Check if the LCD is working properly
uint8_t CheckLCD();

/// Switch LCD off
void LCDOff();

/// Switch LCD on
void LCDOn();


/* EEPROM stuff */

/// Write a single byte to EEPROM
void WriteSingleByteEEPROM(uint16_t addr, uint8_t data);

/// Read a single byte from EEPROM
uint8_t ReadSingleByteEEPROM(uint16_t addr);

/// Write multiple bytes to EEPROM
void WriteBytesEEPROM(uint16_t addr, uint8_t *data, uint16_t len);

/// Read multiple bytes from EEPROM
uint8_t* ReadBytesEEPROM(uint16_t addr, uint16_t len);

/// Read multiple bytes from EEPROM
uint16_t Read16bitRegister(volatile uint16_t *reg);

/// Initialise USART
void InitUSART(uint16_t baudUBRR);

/// Disable USART
void DisableUSART();

// Send a character through the USART
void SendCharUSART(char data);

// Get a character from the USART
char GetCharUSART(void);

// Flush the USART receive buffer
void FlushUSART(void);

// Receives a line (ended CR LF) from the USART
char* GetLineUSART();

// Send a string data to the USART
void SendLineUSART(const char *data);

#endif // _SCD_IO_H_

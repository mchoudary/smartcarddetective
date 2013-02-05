/**
 * \file
 * \brief scd_io.c source file
 *
 * This file implements the functions for all micro-controller
 * I/O functions, including control of LCD, leds and buttons
 *
 * Copyright (C) 2012 Omar Choudary (omar.choudary@cl.cam.ac.uk)
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


/// Frequency of CPU, used for _delay_XX functions
#define F_CPU 16000000UL  

// this is needed for the delay on the new avr-libc-1.7.0
#ifndef __DELAY_BACKWARD_COMPATIBLE__
#define __DELAY_BACKWARD_COMPATIBLE__
#endif

#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>

#include "scd_io.h"    

// static vars
static uint8_t lcd_count; // used by LCD functions
static uint8_t lcd_state; // 0 if LcdOff, non-zero otherwise


//---------------------------------------------------------------

/* Led functions */

void Led1On()
{
    DDRE |= _BV(PE7);
    PORTE |= _BV(PE7);	
}

void Led2On()
{
    DDRE |= _BV(PE6);
    PORTE |= _BV(PE6);	
}

void Led3On()
{
    DDRE |= _BV(PE5);
    PORTE |= _BV(PE5);	
}

void Led4On()
{
    DDRE |= _BV(PE4);
    PORTE |= _BV(PE4);	
}

void Led1Off()
{
    //DDRE |= _BV(PE7);
    //PORTE &= ~(_BV(PE7));

    DDRE &= ~(_BV(PE7));
    PORTE &= ~(_BV(PE7));
}

void Led2Off()
{
    //DDRE |= _BV(PE6);
    //PORTE &= ~(_BV(PE6));

    DDRE &= ~(_BV(PE6));
    PORTE &= ~(_BV(PE6));
}

void Led3Off()
{
    //DDRE |= _BV(PE5);
    //PORTE &= ~(_BV(PE5));

    DDRE &= ~(_BV(PE5));
    PORTE &= ~(_BV(PE5));
}

void Led4Off()
{
    //DDRE |= _BV(PE4);
    //PORTE &= ~(_BV(PE4));

    DDRE &= ~(_BV(PE4));
    PORTE &= ~(_BV(PE4));
}

// Ohter signals

void T_C4On()
{
    DDRB |= _BV(PB4);
    PORTB |= _BV(PB4);
}

void T_C8On()
{
    DDRB |= _BV(PB5);
    PORTB |= _BV(PB5);
}

void T_C4Off()
{
    DDRB &= ~(_BV(PB4));
    PORTB &= ~(_BV(PB4));
}

void T_C8Off()
{
    DDRB &= ~(_BV(PB5));
    PORTB &= ~(_BV(PB5));
}

void JTAG_P1_High()
{
    DDRF |= _BV(PF4);
    PORTF |= _BV(PF4);
}

void JTAG_P1_Low()
{
    DDRF |= _BV(PF4);
    PORTF &= ~(_BV(PF4));
}

void JTAG_P3_High()
{
    DDRF |= _BV(PF6);
    PORTF |= _BV(PF6);
}

void JTAG_P3_Low()
{
    DDRF |= _BV(PF6);
    PORTF &= ~(_BV(PF6));
}

//---------------------------------------------------------------

/* Button functions */

/**
 * Get the status of button A
 *
 * @return 0 if button is pressed, non-zero otherwise
 */
uint8_t GetButtonA()
{
    DDRF &= ~(_BV(PF3));
    return bit_is_set(PINF, PF3);
}

/**
 * Get the status of button B
 *
 * @return 0 if button is pressed, non-zero otherwise
 */
uint8_t GetButtonB()
{
    DDRF &= ~(_BV(PF2));
    return bit_is_set(PINF, PF2);
}


/**
 * Get the status of button C
 *
 * @return 0 if button is pressed, non-zero otherwise
 */
uint8_t GetButtonC()
{
    DDRF &= ~(_BV(PF1));
    return bit_is_set(PINF, PF1);
}


/**
 * Get the status of button D
 *
 * @return 0 if button is pressed, non-zero otherwise
 */
uint8_t GetButtonD()
{
    DDRF &= ~(_BV(PF0));
    return bit_is_set(PINF, PF0);
}

/**
 * This function checks all the buttons for being pressed and
 * returns a 1-hot encoded byte representing each pressed button.
 *
 * @return 1-hot encoded byte representing the buttons that are
 * pressed (1 = pressed, 0 = not pressed). If there is no button
 * pressed the function returns 0. The define directives
 * (BUTTON_A, BUTTON_B, etc.) provide the correspondence between each
 * bit of the result and the button it refers to. *
 */
uint8_t GetButton()
{
    uint8_t result = 0;

    if(bit_is_clear(PINF, PF3)) result |= BUTTON_A;
    if(bit_is_clear(PINF, PF2)) result |= BUTTON_B;
    if(bit_is_clear(PINF, PF1)) result |= BUTTON_C;
    if(bit_is_clear(PINF, PF0)) result |= BUTTON_D;

    return result;
}


//---------------------------------------------------------------

/* LCD functions */

// RS = PC0
// R/W = PC1
// E = PC2
// D0-D7 = PA0-7

// Returns a byte = BF | AC6 ... AC0 containing the busy flag and address counter
uint8_t GetLCDStatus()
{
    uint8_t status;

    PORTC &= 0xF8;
    DDRC |= 0x07;
    DDRA = 0;
    PORTC |= _BV(PC1);
    PORTC |= _BV(PC2);
    _delay_us(10);
    status = PINA;
    PORTC &= ~(_BV(PC2));
    DDRC &= 0xF8;

    return status;
}

/**
 * Return the state (on/off) of the LCD
 */
uint8_t GetLCDState()
{
    return lcd_state;
}

/*
 * Set the state of the LCD
 * Useful when resetting the device
 */
void SetLCDState(uint8_t state)
{
    lcd_state = state;
}

// Sends a command with a given delay (if needed) and returns
// the value of the D0-D7 pins
uint8_t SendLCDCommand(uint8_t RS, uint8_t RW, uint8_t data, uint16_t delay_us)
{
    uint8_t result, busy;

    do{
        result = GetLCDStatus();
        busy = result & 0x80;
    }while(busy);

    DDRC |= 0x07;
    if(RS) PORTC |= _BV(PC0);
    else PORTC &= ~(_BV(PC0));

    if(RW)
    {
        PORTC |= _BV(PC1);
        DDRA = 0x00;
    }
    else 
    {
        PORTC &= ~(_BV(PC1));
        DDRA = 0xFF;
        PORTA = data;
    }

    PORTC |= _BV(PC2);
    _delay_us(delay_us);
    data = PINA;
    PORTC &= ~(_BV(PC2));
    DDRC &= 0xF8;

    return data;
}

void FillScreen()
{
    uint8_t i;
    char *str = "12";

    // clear display
    SendLCDCommand(0, 0, 0x01, LCD_COMMAND_DELAY);

    // send data for first line
    for(i = 0; i < 40; i++)
        SendLCDCommand(1, 0, str[0], LCD_COMMAND_DELAY);	

    // change address to second line
    SendLCDCommand(0, 0, 0xc0, 37);

    for(i = 0; i < 40; i++)
        SendLCDCommand(1, 0, str[1], LCD_COMMAND_DELAY);	
}

/**
 * Write a string to the LCD
 *
 * @param string string to be written
 * @param len length of the string
 */
void WriteStringLCD(char *string, uint8_t len)
{
    uint8_t i;

    // clear display
    SendLCDCommand(0, 0, 0x01, LCD_COMMAND_DELAY);

    i = 0;
    while(i < len && i < 8)
    {
        // write byte to DDRAM
        SendLCDCommand(1, 0, string[i], LCD_COMMAND_DELAY);
        i++;
    }	

    if(len > 8)
    {
        // change address to second line
        SendLCDCommand(0, 0, 0xc0, LCD_COMMAND_DELAY);

        while(i < len && i < 16)
        {
            // write byte to DDRAM
            SendLCDCommand(1, 0, string[i], LCD_COMMAND_DELAY);
            i++;
        }	
    }

    lcd_count = 0;
}

/* 
 * Send character c to the LCD display.  After  8 characters, next
 * character is displayed on second line. After a new line ('\n')
 * the next char clears the display.
 *
 * This function is meant to be used with printf/fprintf commands,
 * by sending this function as parameter to FDEV_SETUP_STREAM.
 *
 * @param c character to be sent to LCD
 * @param unused unused parameter FILE
 * @return 0 if successful, non-zero otherwise
 */
int LcdPutchar(char c, FILE *unused)
{
    static uint8_t nl_seen;

    if (nl_seen && c != '\n')
    {
        //First character after newline, clear display and home cursor.       
        SendLCDCommand(0, 0, 0x01, LCD_COMMAND_DELAY);
        nl_seen = 0;
        lcd_count = 0;
    }

    if(c == '\n')
    {
        nl_seen = 1;
    }
    else
    {
        // write character
        SendLCDCommand(1, 0, c, LCD_COMMAND_DELAY);
    }

    lcd_count++;
    if(lcd_count == 8)
        SendLCDCommand(0, 0, 0xc0, LCD_COMMAND_DELAY);  // go to 2nd line
    else if(lcd_count == 16)
        SendLCDCommand(0, 0, 0x02, LCD_COMMAND_DELAY);  // return home

    return 0;
}


/**
 * This function is used to initialize the LCD. It should be called
 * before any other operation on the LCD.
 */
void InitLCD()
{	
    // power to V0 (contrast variable resistor output)
    DDRC |= _BV(PC5);
    PORTC |= _BV(PC5);

    // send function set command - 1 line, 8 bits data
    //SendLCDCommand(0, 0, 0x30, LCD_COMMAND_DELAY);

    // send function set command - 2 lines, 8 bits data
    SendLCDCommand(0, 0, 0x38, LCD_COMMAND_DELAY);

    // display on, cursor enabled, no blinking
    SendLCDCommand(0, 0, 0x0E, LCD_COMMAND_DELAY);

    // sets cursor move dir
    SendLCDCommand(0, 0, 0x06, LCD_COMMAND_DELAY);

    // clear display
    SendLCDCommand(0, 0, 0x01, LCD_COMMAND_DELAY);	

    lcd_state = 1;

    // The following shift commands are only needed if a
    // display messes with position

    // shift to left
    //SendLCDCommand(0, 0, 0x18, LCD_COMMAND_DELAY);

    // shift to right
    //SendLCDCommand(0, 0, 0x1C, LCD_COMMAND_DELAY);
}

/**
 * This function checks if the LCD is working properly
 *
 * @return 0 if LCD works fine, non-zero if there is a problem
 */
uint8_t CheckLCD()
{
    // RS = PC0
    // R/W = PC1
    // E = PC2
    // D0-D7 = PA0-7
    uint8_t tmp;	

    // put a random value (i.e. 0xAA) on PORTA. If LCD works, on GetSTATUS
    // we should get something different	
    DDRA = 0xFF;
    PORTA = 0xAA;	
    tmp = GetLCDStatus();	

    if(tmp == 0xAA) return 1;

    return 0;	
}

/**
 * This function turns off the LCD display
 */
void LCDOff()
{
    SendLCDCommand(0, 0, 0x08, LCD_COMMAND_DELAY);
    DDRC &= ~(_BV(PC5));
    PORTC &= ~(_BV(PC5));
    lcd_state = 0;
}

/**
 * This function turns on the LCD display
 */
void LCDOn()
{
    DDRC |= _BV(PC5);
    PORTC |= _BV(PC5);
    SendLCDCommand(0, 0, 0x0E, LCD_COMMAND_DELAY);
    lcd_state = 1;
}


//---------------------------------------------------------------

/* EEPROM stuff */

/** 
 * This function writes a sigle byte to the EEPROM.
 * Its code is mainly taken from the datasheet.
 * This method does not change the interrupt vector so
 * it should be handled with care. It is better
 * to use WriteBytesEEPROM as it takes care of interrupts
 * and writes multiple bytes at once.
 *
 * @param addr address of byte to be written
 * @param data byte to be written
 * @sa WriteBytesEEPROM
 */
void WriteSingleByteEEPROM(uint16_t addr, uint8_t data)
{
    // wait completion of previous write
    while(EECR & _BV(EEPE));

    // set up address and data
    EEAR = addr;
    EEDR = data;

    // make the write
    EECR |= _BV(EEMPE);
    EECR |= _BV(EEPE);
}

/** 
 * This function reads a sigle byte from the EEPROM.
 * Its code is mainly taken from the datasheet.
 * This method does not change the interrupt vector so
 * it should be handled with care. It is better
 * to use ReadBytesEEPROM as it takes care of interrupts
 * and reads multiple bytes at once.
 *
 * @param addr address of byte to be read
 * @return data byte read
 * @sa ReadBytesEEPROM
 */
uint8_t ReadSingleByteEEPROM(uint16_t addr)
{
    // wait for completion of previous write
    while(EECR & _BV(EEPE));

    // set up address and start reading
    EEAR = addr;
    EECR |= _BV(EERE);

    return EEDR;
}


/** 
 * This function writes multiple bytes to the EEPROM.
 * This method stops interrupts while writing the bytes.
 *
 * @param addr address of first byte to be written
 * @param data bytes to be written
 * @param len number of bytes to be written
 * @sa WriteSingleByteEEPROM
 */
void WriteBytesEEPROM(uint16_t addr, uint8_t *data, uint16_t len)
{
    uint8_t i, sreg;

    if(data == NULL || len > 4000) return;

    sreg = SREG;
    cli();

    for(i = 0; i < len; i++)
        WriteSingleByteEEPROM(addr++, data[i]);

    // wait completion of last write
    while(EECR & _BV(EEPE));

    SREG = sreg;
}

/** 
 * This function reads multiple bytes from the EEPROM.
 * This method stops interrupts while reading the bytes.
 *
 * @param addr address of first byte to be read
 * @param len number of bytes to be read
 * @return the bytes read from the EEPROM. This method allocates
 * the necessary memory to store the bytes. The caller is responsible
 * for eliberating this memory after use. If this method is
 * unsuccessful it will return NULL.
 * @sa ReadSingleByteEEPROM
 */
uint8_t* ReadBytesEEPROM(uint16_t addr, uint16_t len)
{
    uint8_t i, sreg;
    uint8_t *data;

    if(len > 4000) return NULL;
    data = (uint8_t*)malloc(len*sizeof(uint8_t));
    if(data == NULL) return NULL;

    sreg = SREG;
    cli();

    for(i = 0; i < len; i++)
        data[i] = ReadSingleByteEEPROM(addr++);

    SREG = sreg;

    return data;
}


/**
 * Initiualise the USART port
 *
 * @param baudUBRR the baud UBRR parameter as given in table 18-12 of
 * the datasheet, page 203. The formula is: baud = FCLK / (16 * (baudUBRR + 1)).
 * So for FCLK = 16 MHz and desired BAUD = 9600 bps => baudUBRR = 103.
 */
void InitUSART(uint16_t baudUBRR)
{
    uint8_t sreg = SREG;
    cli();

    // Set baud
    UBRR1H = (uint8_t) (baudUBRR >> 8);
    UBRR1L = (uint8_t) (baudUBRR & 0xFF);

    // Enable receiver and transmitter
    UCSR1B = (1 << RXEN1) || (1 << TXEN1);

    // Set frame format: 8 data, 1 stop bit
    UCSR1C = (3 << UCSZ10);
    SREG = sreg;
}

void DisableUSART()
{
    uint8_t sreg = SREG;
    cli();
    UCSR1B = 0;
    SREG = sreg;
}

/**
 * Transmit a character throught the USART. Method as in the datasheet.
 *
 * @param data the character to be sent
 */
void SendCharUSART(char data)
{
    // Wait for empty buffer
    while(!(UCSR1A & (1 << UDRE1)));

    // Put and send the data
    UDR1 = data;
}


/**
 * Get a character from the USART. Method from the datasheet.
 */
char GetCharUSART(void)
{
    // wait for data to be received
    while(!(UCSR1A & (1 << RXC1)));

    // return the character
    return UDR1;
}

/**
 * Flush the receive buffer, useful in case of errors.
 */
void FlushUSART(void)
{
    char dummy;
    while(UCSR1A & (1 << RXC1)) dummy = UDR1;
}

/**
 * This method receives a line (ended CR LF) from the USART
 *
 * @return the line contents, removing the trailing CR LF and
 * appending the NUL ('\0') character. The caller is responsible
 * for eliberating the memory occupied by the returned string.
 */
char* GetLineUSART()
{
    char buf[256];
    uint8_t i = 0;

    memset(buf, 0, 256);

    while(i < 256)
    {
        fprintf(stderr, "Getting char\n");
        _delay_ms(200);
        buf[i] = GetCharUSART();
        fprintf(stderr, "Char: %c\n", buf[i]);
        _delay_ms(200);

        if(i == 0 && (buf[i] == '\n' || buf[i] == '\r'))
            continue;
        else if(buf[i] == '\n')
        {
            buf[i] = 0;
            if(buf[i - 1] == '\r')
                buf[i - 1] = 0;
            return strdup(buf);
        }

        i = i + 1;
    }

    return NULL;
}

/**
 * This method sends a data string (without adding CR LF) to the USART
 *
 * @param data the string to be transmitted, ended with the NUL ('\0') character.
 */
void SendLineUSART(const char *data)
{
    if(data == NULL) return;

    while(data && *data)
        SendCharUSART(*data++);
}


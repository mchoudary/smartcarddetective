/** \file
 *	\brief	serial.c source file
 *
 *  This file implements the methods for serial communication between
 *  the SCD and a host.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *
 */

#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>

#include "apps.h"
#include "serial.h"
#include "scd_io.h"
#include "scd_values.h"
#include "VirtualSerial.h"


///size of SCD's EEPROM
#define EEPROM_SIZE 4096

/** AT command and response strings **/
static const char strAT_CRST[] = "AT+CRST";
static const char strAT_CTERM[] = "AT+CTERM";
static const char strAT_CLET[] = "AT+CLET";
static const char strAT_CGEE[] = "AT+CGEE";
static const char strAT_CEEE[] = "AT+CEEE";
static const char strAT_CGBM[] = "AT+CGBM";

static const char strAT_RBAD[] = "AT BAD\r\n";
static const char strAT_ROK[] = "AT OK\r\n";

/**
 * This method handles the data received from the serial or virtual serial port.
 *
 * The SCD should make any necessary processing and then return a response
 * data. However sending a command (e.g. CTERM or CRST) that will reset the
 * SCD will have the obvious consequence that no response will be received.
 * Even more the communication to the host will be interrupted so the host
 * should take care of this.
 *
 * @param data a NUL ('\0') terminated string representing the data to be
 * processed by the SCD, as sent by the serial host
 * @return a NUL ('\0') terminated string representing the response of this
 * method if success, or NULL if any error occurs. The caller is responsible for
 * eliberating the space ocuppied by the response.
 */
char* ProcessSerialData(const char* data)
{   
    uint8_t tmp;
    char *atparams = NULL;
    AT_CMD atcmd;
    
    //fprintf(stderr, "%s\n", data);
    //_delay_ms(500);
    
    tmp = ParseATCommand(data, &atcmd, atparams);
    if(tmp != 0)
        return strdup(strAT_RBAD);

    if(atcmd == AT_NONE)
    {
        fprintf(stderr, "AT_NONE\n");
        _delay_ms(500);
    }
    else if(atcmd == AT_CRST)
    {
        // Reset the SCD within 1S so that host can reset connection
        wdt_enable(WDTO_1S);
        while(1);
    }
    else if(atcmd == AT_CTERM)
    {
        Terminal();
    }
    else if(atcmd == AT_CLET)
    {
        ForwardData();
    }
    else if(atcmd == AT_CGEE)
    {
        // Return EEPROM contents in Intel HEX format
        SendEEPROMHexVSerial();
    }
    else if(atcmd == AT_CEEE)
    {
        EraseEEPROM();
    }
    
    return strdup(strAT_ROK);
} 

/**
 * This method parses a data stream that should correspond to an AT command
 * and returns the type of command and any parameters.
 *
 * @param data a NUL ('\0') terminated string representing the data received
 * from the host
 * @param atcmd stores the type of command in data if parsing is successful.
 * The caller is responsible for allocating space for this.
 * @param atparams points to the place in data where the parameters of the
 * command are located or is NULL if there are no parameters
 * @return 0 if success, non-zero otherwise
 */
uint8_t ParseATCommand(const char *data, AT_CMD *atcmd, char *atparams)
{
    uint8_t len;

    atparams = NULL;
    *atcmd = AT_NONE;

    if(data == NULL || data[0] != 'A' || data[1] != 'T')
        return RET_ERR_PARAM;

    len = strlen(data);
    if(len < 3)
        return RET_ERR_PARAM;

    if(data[2] == '+')
    {
        if(strstr(data, strAT_CRST) == data)
        {
            *atcmd = AT_CRST;
            return 0;
        }
        else if(strstr(data, strAT_CTERM) == data)
        {
            *atcmd = AT_CTERM;
            return 0;
        }
        else if(strstr(data, strAT_CLET) == data)
        {
            *atcmd = AT_CLET;
            return 0;
        }
        else if(strstr(data, strAT_CGEE) == data)
        {
            *atcmd = AT_CGEE;
            return 0;
        }
        else if(strstr(data, strAT_CEEE) == data)
        {
            *atcmd = AT_CEEE;
            return 0;
        }
        else if(strstr(data, strAT_CGBM) == data)
        {
            *atcmd = AT_CGBM;
            return 0;
        }
    }

    return 0;
}


/**
 * This method reads the content of the EEPROM and transmits it in Intel
 * Hex format to the Virtual Serial port. It is the responsibility of the
 * caller to make sure the virtual serial port is availble.
 *
 * @return zero if success, non-zero otherwise
 */
uint8_t SendEEPROMHexVSerial()
{
    uint8_t eedata[32];
    uint16_t eeaddr;
    char eestr[78];
    uint8_t eesum;
    uint8_t i, k, t;

    eeaddr = 0;
    memset(eestr, 0, 78);
    eestr[0] = ':';
    eestr[1] = '2';
    eestr[2] = '0';
    eestr[7] = '0';
    eestr[8] = '0';
    eestr[75] = '\r';
    eestr[76] = '\n';

    for(k = 0; k < EEPROM_SIZE / 32; k++)
    {
        eeprom_read_block(eedata, (void*)eeaddr, 32);
        eesum = 32 + ((eeaddr >> 8) & 0xFF) + (eeaddr & 0xFF);
        t = (eeaddr >> 12) & 0x0F;
        eestr[3] = (t < 0x0A) ? (t + '0') : (t + '7');
        t = (eeaddr >> 8) & 0x0F;
        eestr[4] = (t < 0x0A) ? (t + '0') : (t + '7');
        t = (eeaddr >> 4) & 0x0F;
        eestr[5] = (t < 0x0A) ? (t + '0') : (t + '7');
        t = eeaddr & 0x0F;
        eestr[6] = (t < 0x0A) ? (t + '0') : (t + '7');

        for(i = 0; i < 32; i++)
        {
            eesum = eesum + eedata[i];
            t = (eedata[i] >> 4) & 0x0F;
            eestr[9 + i * 2] = (t < 0x0A) ? (t + '0') : (t + '7');
            t = eedata[i] & 0x0F;
            eestr[10 + i * 2] = (t < 0x0A) ? (t + '0') : (t + '7');
        }
        
        eesum = (uint8_t)((eesum ^ 0xFF) + 1);
        t = (eesum >> 4) & 0x0F;
        eestr[73] = (t < 0x0A) ? (t + '0') : (t + '7');
        t = eesum & 0x0F;
        eestr[74] = (t < 0x0A) ? (t + '0') : (t + '7');

        if(SendHostData(eestr))
            return RET_ERROR;

        eeaddr = eeaddr + 32;
    }

    memset(eestr, 0, 78);
    eestr[0] = ':';
    eestr[1] = '0';
    eestr[2] = '0';
    eestr[3] = '0';
    eestr[4] = '0';
    eestr[5] = '0';
    eestr[6] = '0';
    eestr[7] = '0';
    eestr[8] = '1';
    eestr[9] = 'F';
    eestr[10] = 'F';
    eestr[11] = '\r';
    eestr[12] = '\n';
    if(SendHostData(eestr))
        return RET_ERROR;
    
    return 0;
}


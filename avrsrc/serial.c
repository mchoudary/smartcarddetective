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
#include "emv.h"
#include "terminal.h"
#include "scd_hal.h"
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
static const char strAT_CCINIT[] = "AT+CCINIT";
static const char strAT_CCAPDU[] = "AT+CCAPDU";
static const char strAT_CCEND[] = "AT+CCEND";
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
    
    tmp = ParseATCommand(data, &atcmd, &atparams);
    if(tmp != 0)
        return strdup(strAT_RBAD);

    if(atcmd == AT_CRST)
    {
        // Reset the SCD within 1S so that host can reset connection
        wdt_enable(WDTO_1S);
        while(1);
    }
    else if(atcmd == AT_CTERM)
    {
        Terminal(1);
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
    else if(atcmd == AT_CGBM)
    {
        RunBootloader();
    }
    else if(atcmd == AT_CCINIT)
    {
        TerminalVSerial();
    }
    else
    {
        return strdup(strAT_RBAD);
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
uint8_t ParseATCommand(const char *data, AT_CMD *atcmd, char **atparams)
{
    uint8_t len, pos;

    *atparams = NULL;
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
        else if(strstr(data, strAT_CCINIT) == data)
        {
            *atcmd = AT_CCINIT;
            return 0;
        }
        else if(strstr(data, strAT_CCAPDU) == data)
        {
            *atcmd = AT_CCAPDU;
            pos = strlen(strAT_CCAPDU);
            if((strlen(data) > pos + 1) && data[pos] == '=')
                *atparams = &data[pos + 1];
            return 0;
        }
        else if(strstr(data, strAT_CCEND) == data)
        {
            *atcmd = AT_CCEND;
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


/**
 * This method implements a virtual serial terminal application.
 *
 * The SCD receives CAPDUs from the Virtual Serial host and transmits
 * back the RAPDUs received from the card. This method should be called
 * upon reciving the AT+CINIT serial command.
 *
 * This function never returns, after completion it will restart the SCD.
 *
 */
void TerminalVSerial()
{
    uint8_t convention, proto, TC1, TA3, TB3;
    uint8_t tmp, i, lparams, ldata;
    char *buf, *atparams = NULL;
    char reply[512];
    uint8_t data[256];
    AT_CMD atcmd;
    RAPDU *response = NULL;
    CAPDU *command = NULL;

    // First thing is to respond to the VS host since this method
    // was presumably called based on an AT+CINIT command
    if(!IsICCInserted())
    {
        SendHostData(strAT_RBAD);
        wdt_enable(WDTO_60MS);
        while(1);
    }

    if(ResetICC(0, &convention, &proto, &TC1, &TA3, &TB3))
    {
        SendHostData(strAT_RBAD);
        wdt_enable(WDTO_60MS);
        while(1);
    }
    if(proto != 0) // Not implemented yet, perhaps someone will...
    {
        SendHostData(strAT_RBAD);
        wdt_enable(WDTO_60MS);
        while(1);
    }

    // If all is well so far announce the host
    SendHostData(strAT_ROK);

    // Loop continuously until the host ends the transaction or
    // we get an error
    while(1)
    {
        buf = GetHostData(255);
        if(buf == NULL)
        {
            _delay_ms(100);
            continue;
        }

        tmp = ParseATCommand(buf, &atcmd, &atparams);
        lparams = strlen(atparams);

        if(atcmd == AT_CCEND)
        {
            SendHostData(strAT_ROK);
            wdt_enable(WDTO_60MS);
            while(1);
        }
        else if(atcmd != AT_CCAPDU || atparams == NULL || lparams < 10 || (lparams % 2) != 0)
        {
            SendHostData(strAT_RBAD);
            free(buf);
            continue;
        }

        memset(data, 0, 256);
        for(i = 0; i < lparams/2; i++)
        {
            data[i] = hexCharsToByte(atparams[2*i], atparams[2*i + 1]);
        }
        free(buf);

        ldata = (lparams - 10) / 2;
        command = MakeCommand(
                data[0], data[1], data[2], data[3], data[4],
                &data[5], ldata);
        if(command == NULL)
        {
            SendHostData(strAT_RBAD);
            continue;
        }

        // Send the command
        response = TerminalSendT0Command(command, convention, TC1);
        FreeCAPDU(command);
        if(response == NULL)
        {
            FreeCAPDU(command);
            SendHostData(strAT_RBAD);
            continue;
        }

        memset(reply, 0, 512);
        reply[0] = nibbleToHexChar(response->repStatus->sw1, 1);
        reply[1] = nibbleToHexChar(response->repStatus->sw1, 0);
        reply[2] = nibbleToHexChar(response->repStatus->sw2, 1);
        reply[3] = nibbleToHexChar(response->repStatus->sw2, 0);
        for(i = 0; i < response->lenData; i++)
        {
            reply[4 + i*2] = nibbleToHexChar(response->repData[i], 1);
            reply[5 + i*2] = nibbleToHexChar(response->repData[i], 0);
        }
        reply[4 + i] = '\r';
        reply[5 + i] = '\n';
        FreeRAPDU(response);
        SendHostData(reply);
    } // end while(1)
}


/**
 * Convert 2 hexadecimal characters ('0' to '9', 'A' to 'F') into its
 * binary/hexa representation
 *
 * @param c1 the most significant hexa character
 * @param c2 the least significant hexa character
 * @return a byte containing the binary representation
 */
uint8_t hexCharsToByte(char c1, char c2)
{
    uint8_t result = 0;

    if(c1 >= '0' && c1 <= '9')
        result = c1 - '0';
    else if(c1 >= 'A' && c1 <= 'F')
        result = c1 - '7';
    else if(c1 >= 'a' && c1 <= 'f')
        result = c1 - 'W';
    else
        return 0;

    result = result << 4;

    if(c2 >= '0' && c2 <= '9')
        result |= c2 - '0';
    else if(c2 >= 'A' && c2 <= 'F')
        result |= c2 - '7';
    else if(c2 >= 'a' && c2 <= 'f')
        result |= c2 - 'W';
    else
        return 0;

    return result;
}

/**
 * Convert a nibble into a hexadecimal character ('0' to '9', 'A' to 'F')
 *
 * @param b the byte containing the binary representation
 * @param high set to 1 if high nibble should be converted, zero if the low
 * nibble is desired. Example: byteToHexChar(0xF3, 1) returns "F".
 * @return the character hexa representation of the given nibble
 */
char nibbleToHexChar(uint8_t b, uint8_t high)
{
    char result = '0';
    
    if(high)
        b = (b & 0xF0) >> 4;
    else
        b = b & 0x0F;

    if(b < 10)
        result = b + '0';
    else if(b < 16)
        result = b + '7';

    return result;
}


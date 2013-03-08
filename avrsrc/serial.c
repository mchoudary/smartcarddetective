/**
 * \file
 * \brief serial.c source file
 *
 * This file implements the methods for serial communication between
 * the SCD and a host.
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
#include "utils.h"
#include "VirtualSerial.h"


///size of SCD's EEPROM
#define EEPROM_SIZE 4096

/** AT command and response strings **/
static const char strAT_CRST[] = "AT+CRST";
static const char strAT_CTERM[] = "AT+CTERM";
static const char strAT_CTUSB[] = "AT+CTUSB";
static const char strAT_CLET[] = "AT+CLET";
static const char strAT_CDPIN[] = "AT+CDPIN";
static const char strAT_CGEE[] = "AT+CGEE";
static const char strAT_CEEE[] = "AT+CEEE";
static const char strAT_CGBM[] = "AT+CGBM";
static const char strAT_CCINIT[] = "AT+CCINIT";
static const char strAT_CCAPDU[] = "AT+CCAPDU";
static const char strAT_UDATA[] = "AT+UDATA";
static const char strAT_CCEND[] = "AT+CCEND";
static const char strAT_CTWAIT[] = "AT+CTWAIT";
static const char strAT_RBAD[] = "AT BAD\r\n";
static const char strAT_ROK[] = "AT OK\r\n";
static const char strAT_RTRESET[] = "AT TRESET\r\n";


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
 * @param logger the log structure or NULL if no log is desired
 * @return a NUL ('\0') terminated string representing the response of this
 * method if success, or NULL if any error occurs. The caller is responsible for
 * eliberating the space ocuppied by the response.
 */
char* ProcessSerialData(const char* data, log_struct_t *logger)
{   
  char *atparams = NULL;
  AT_CMD atcmd;
  uint8_t result = 0;
  char *str_ret = NULL;

  result = ParseATCommand(data, &atcmd, &atparams);
  if(result != 0)
    return strdup(strAT_RBAD);

  if(atcmd == AT_CRST)
  {
    // Reset the SCD within 1S so that host can reset connection
    StopUSBHardware();
    wdt_enable(WDTO_1S);
    while(1);
  }
  else if(atcmd == AT_CTERM)
  {
    result = Terminal(logger);
    if (result == 0)
      str_ret = strdup(strAT_ROK);
    else
      str_ret = strdup(strAT_RBAD);
  }
  else if(atcmd == AT_CTUSB)
  {
    result = TerminalUSB(logger);
    if (result == 0)
      str_ret = strdup(strAT_ROK);
    else
      str_ret = strdup(strAT_RBAD);
  }
  else if(atcmd == AT_CLET)
  {
    result = ForwardData(logger);
    if (result == 0)
      str_ret = strdup(strAT_ROK);
    else
      str_ret = strdup(strAT_RBAD);
  }
  else if(atcmd == AT_CDPIN)
  {
    result = DummyPIN(logger);
    if (result == 0)
      str_ret = strdup(strAT_ROK);
    else
      str_ret = strdup(strAT_RBAD);
  }
  else if(atcmd == AT_CGEE)
  {
    // Return EEPROM contents in Intel HEX format
    SendEEPROMHexVSerial();
    str_ret = strdup(strAT_ROK);
  }
  else if(atcmd == AT_CEEE)
  {
    ResetEEPROM();
    str_ret = strdup(strAT_ROK);
  }
  else if(atcmd == AT_CGBM)
  {
    RunBootloader();
    str_ret = strdup(strAT_ROK);
  }
  else if(atcmd == AT_CCINIT)
  {
    result = TerminalVSerial(logger);
    if (result == 0)
      str_ret = strdup(strAT_ROK);
    else
      str_ret = strdup(strAT_RBAD);
  }
  else
  {
    str_ret = strdup(strAT_RBAD);
  }

  return str_ret;
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
    else if(strstr(data, strAT_CTUSB) == data)
    {
      *atcmd = AT_CTUSB;
      return 0;
    }
    else if(strstr(data, strAT_CLET) == data)
    {
      *atcmd = AT_CLET;
      return 0;
    }
    else if(strstr(data, strAT_CDPIN) == data)
    {
      *atcmd = AT_CDPIN;
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
    else if(strstr(data, strAT_UDATA) == data)
    {
      *atcmd = AT_UDATA;
      pos = strlen(strAT_UDATA);
      if((strlen(data) > pos + 1) && data[pos] == '=')
        *atparams = &data[pos + 1];
      return 0;
    }
    else if(strstr(data, strAT_CCEND) == data)
    {
      *atcmd = AT_CCEND;
      return 0;
    }
    else if(strstr(data, strAT_CTWAIT) == data)
    {
      *atcmd = AT_CTWAIT;
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

/***
 * Method to convert data bytes into hex characters
 *
 * @param dest the allocated buffer with the result string containing the coverted chars.
 * The returned string is 0 terminated.
 * @param data a vector of bytes
 * @param len the length of the data bytes
 */
void BytesToHexChars(char* dest, uint8_t *data, uint32_t len)
{
  uint32_t i;

  for(i = 0; i < len; i++)
  {
    dest[2*i] = nibbleToHexChar(data[i], 1);
    dest[2*i + 1] = nibbleToHexChar(data[i], 0);
  }
  dest[2*len] = 0;
}


/*uint8_t SendHostBigData(char *data)
  {
  uint32_t len;
  uint8_t pos = 0;
  char c;

  len = strlen(data);

  while(len > 255)
  {
  c = data[255];
  data[255] = 0;


  return 0;
  }*/

/**
 * This method implements communication between USB and Terminal, which can be
 * used to emulate a card with data from a host PC.
 *
 * The SCD receives instructions from the Virtual Serial host and communicates
 * with a Terminal. This can be used to forward the data from the terminal
 * over a host and then getting back RAPDUs that are sent to the terminal.
 * This method should be called upon reciving the AT+CCINIT serial command.
 *
 * @param logger the log structure or NULL if a log is not desired
 * @return zero if success, non-zero otherwise
 */
uint8_t TerminalUSB(log_struct_t *logger)
{
  //uint8_t convention, proto, TC1, TA3, TB3;
  uint8_t t_inverse = 0, t_TC1 = 0;
  uint8_t tmp, i, lparams, error;
  char *buf = NULL;
  char *atparams = NULL;
  char reply[USB_BUF_SIZE];
  uint8_t *data;
  uint32_t len;
  AT_CMD atcmd;
  RAPDU *response = NULL;
  CAPDU *command = NULL;

  // Send OK to host to get first ATR
  SendHostData(strAT_ROK);

  // Now wait for start of transaction from Terminal
  if(lcdAvailable)
    fprintf(stderr, "Connect terminal\n");
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_FROM_USB, 0);
  while(GetTerminalResetLine() != 0);
  if(logger)
    LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);
  StartCounterTerminal();	
  if(lcdAvailable)
    fprintf(stderr, "Working...\n");

  // Loop until there is no clock from terminal or a timeout occurs.
  // This allows to cope with transactions where the reader might reset the
  // communication several times (e.g. warm reset).
  while(1) // external loop
  {
    error = InitEMVTerminal(logger);
    if(error)
      goto enderror;

    // Send the TS byte now
    if(t_inverse)
    {
      SendByteTerminalNoParity(0x3F, t_inverse);
      if(logger)
        LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, 0x3F);
    }
    else
    {
      SendByteTerminalNoParity(0x3B, t_inverse);
      if(logger)
        LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, 0x3B);
    }

    // Get the next ATR from host
    buf = GetHostData(USB_BUF_SIZE);
    if(buf == NULL)
    {
      error = RET_ERROR;
      goto enderror;
    }

    error = ParseATCommand(buf, &atcmd, &atparams);
    if(error != 0)
      goto enderror;
    len = strlen(atparams);
    if(atcmd != AT_UDATA)
    {
      error = RET_ERROR;
      goto enderror;
    }

    // Send the rest of ATR to the terminal
    for(i = 0; i < len/2; i++)
    {
      tmp = hexCharsToByte(atparams[2*i], atparams[2*i + 1]);
      SendByteTerminalNoParity(tmp, t_inverse);
      if(logger)
        LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, tmp);
      LoopTerminalETU(2);
    }
    free(buf); buf = NULL;

    // update transaction counter
    nCounter++;

    // Continually exchange commands until a terminal reset or timeout or until
    // the USB host decides to stop
    while(1) // internal while
    {
      // receive command from terminal
      command = ReceiveT0Command(t_inverse, t_TC1, logger);
      if(command == NULL)
      {
        // we assume a timeout due to restart, and signal this to USB host, who
        // should be sending back a new ATR
        SendHostData(strAT_RTRESET);

        // restart external loop
        break;
      }

      // send command to USB host
      data = SerializeCommand(command, &len);
      FreeCAPDU(command);
      if(data == NULL)
        break;
      BytesToHexChars(reply, data, len);
      free(data); data = NULL;
      reply[2*len] = '\r';
      reply[2*len + 1] = '\n';
      reply[2*len + 2] = 0;
      SendHostData(reply);

askhost:
      // receive response from USB
      buf = GetHostData(USB_BUF_SIZE);
      if(buf == NULL)
      {
        error = RET_USB_ERR_RECEIVE;
        if(logger)
        {
          LogCurrentTime(logger);
          LogByte1(logger, LOG_USB_ERROR_RECEIVE, 0);
        }
        goto enderror;
      }

      error = ParseATCommand(buf, &atcmd, &atparams);
      if(error)
        goto enderror;
      lparams = strlen(atparams);
      if(atcmd == AT_CCEND)
      {
        if(logger)
          LogByte1(logger, LOG_BYTE_CCEND_FROM_USB, 0);
        goto endgood;
      }
      else if(atcmd == AT_CTWAIT)
      {
        SendByteTerminalNoParity(0x60, t_inverse);
        if(logger)
          LogByte1(logger, LOG_TERMINAL_MORE_TIME, 0x60);
        free(buf); buf = NULL;
        goto askhost;
      }
      else if(atcmd != AT_UDATA)
      {
        error = RET_ERROR;
        goto enderror;
      }

      // Send response to terminal
      for(i = 0; i < lparams/2; i++)
      {
        tmp = hexCharsToByte(atparams[2*i], atparams[2*i + 1]);
        error = SendByteTerminalParity(tmp, t_inverse);
        if(error)
        {
          if(logger)
          {
            LogCurrentTime(logger);
            LogByte1(logger, LOG_TERMINAL_ERROR_SEND, response->repData[i]);
          }
          goto enderror;
        }
        if(logger)
          LogByte1(logger, LOG_BYTE_TO_TERMINAL, tmp);
        LoopTerminalETU(2);
      }
      free(buf); buf = NULL;
    } // end internal loop
  } // end external loop

endgood:
  error = 0;

enderror:
  DeactivateICC();
  free(buf); buf = NULL;
  if((error == RET_TERMINAL_TIME_OUT) || (error == RET_TERMINAL_NO_CLOCK))
  {
    // these errors are logged and used as a signal to stop
    error = 0;
  }
  if(logger)
  {
    LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
    if(lcdAvailable)
      fprintf(stderr, "Writing Log\n");
    WriteLogEEPROM(logger);
    ResetLogger(logger);
  }

  return error;
}

/**
 * This method implements a virtual serial terminal application.
 *
 * The SCD receives CAPDUs from the Virtual Serial host and transmits
 * back the RAPDUs received from the card. This method should be called
 * upon reciving the AT+CCINIT serial command.
 *
 * This function never returns, after completion it will restart the SCD.
 *
 * @param logger the log structure or NULL if a log is not desired
 * @return zero if success, non-zero otherwise
 */
uint8_t TerminalVSerial(log_struct_t *logger)
{
  uint8_t convention, proto, TC1, TA3, TB3;
  uint8_t tmp, i, lparams, ldata, result;
  char *buf, *atparams = NULL;
  char reply[512];
  uint8_t data[256];
  AT_CMD atcmd;
  RAPDU *response = NULL;
  CAPDU *command = NULL;

  // First request ICC
  fprintf(stderr, "Insert  ICC...\n");
  while(!IsICCInserted());
  fprintf(stderr, "Working...\n");

  result = ResetICC(0, &convention, &proto, &TC1, &TA3, &TB3, logger);
  if(result)
  {
    fprintf(stderr, "ICC reset failed\n");
    _delay_ms(500);
    fprintf(stderr, "result: %2X\n", result);
    _delay_ms(500);
    goto enderror;
  }

  if(proto != 0) // Not implemented yet ...
  {
    fprintf(stderr, "bad ICC proto\n");
    _delay_ms(500);
    goto enderror;
  }

  // If all is well so far announce the host so we get more data
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
      result = 0;
      break;
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
    response = TerminalSendT0Command(command, convention, TC1, logger);
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

enderror:
  DeactivateICC();
  if(logger)
  {
    LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
    if(lcdAvailable)
      fprintf(stderr, "Writing Log\n");
    WriteLogEEPROM(logger);
    ResetLogger(logger);
  }

  return result;
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



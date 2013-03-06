/**
 * \file
 * \brief emv.c source file
 *
 * Contains the implementation of functions
 * used to implement the EMV standard
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


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "counter.h"
#include "emv.h"
#include "emv_values.h"
#include "scd_hal.h"
#include "scd_io.h"
#include "scd_values.h"
#include "utils.h"

#define DEBUG 1   // Set DEBUG to 1 to enable debug code


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
 * @param logger a pointer to a log structure or NULL if no log is desired.
 * @return zero if successful, non-zero otherwise
 */
uint8_t ResetICC(
    uint8_t warm,
    uint8_t *inverse_convention,
    uint8_t *proto,
    uint8_t *TC1,
    uint8_t *TA3,
    uint8_t *TB3,
    log_struct_t *logger)
{
  uint16_t atr_selection;
  uint8_t atr_bytes[32];
  uint8_t atr_tck;
  uint8_t icc_T0, icc_TS;
  uint8_t error;

  // Activate the ICC
  error = ActivateICC(warm);
  if(error)
  {
    error = RET_ICC_INIT_ACTIVATE;
    goto enderror;
  }
  if(logger)
  {
    LogCurrentTime(logger);
    LogByte1(logger, LOG_ICC_ACTIVATED, 0);
  }

  // Wait for approx 42000 ICC clocks = 112 ETUs
  LoopICCETU(112);

  // Set RST to high
  SetICCResetLine(1);
  if(logger)
    LogByte1(logger, LOG_ICC_RST_HIGH, 0);

  // Wait for ATR from ICC for a maximum of 42000 ICC clock cycles + 40 ms
  if(WaitForICCData(ICC_RST_WAIT))
  {
    if(warm == 0)
      return ResetICC(1, inverse_convention, proto, TC1, TA3, TB3, logger);

    error = RET_ICC_INIT_RESPONSE;
    goto enderror;
  }

  // Get ATR
  error = GetATRICC(
      inverse_convention, proto, &icc_TS, &icc_T0,
      &atr_selection, atr_bytes, &atr_tck, logger);
  if(error)
  {
    if(warm == 0)
      return ResetICC(1, inverse_convention, proto, TC1, TA3, TB3, logger);
    goto enderror;
  }
  *TC1 = atr_bytes[2];
  *TA3 = atr_bytes[8];
  *TB3 = atr_bytes[9];

  return 0;

enderror:
  DeactivateICC();
  if(logger)
    LogByte1(logger, LOG_ICC_DEACTIVATED, 0);

  return error;
}


/* T=0 protocol functions */
/* All commands are received from the terminal and sent to the ICC */
/* All responses are received from the ICC and sent to the terminal */

/**
 * Sends default ATR for T=0 to terminal
 *
 * @param inverse_convention specifies if direct (0) or inverse
 * convention (non-zero) is to be used. Only direct convention should
 * be used for future applications.
 * @param TC1 specifies the TC1 byte of the ATR. This should be as
 * small as possible in order to limit the latency of communication,
 * or large if a large timeout between bytes is desired.
 * @param logger a pointer to a log structure or NULL if no log is desired.
 */
void SendT0ATRTerminal(
    uint8_t inverse_convention,
    uint8_t TC1,
    log_struct_t *logger)
{
  if(inverse_convention)
  {
    SendByteTerminalNoParity(0x3F, inverse_convention);
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, 0x3F);
  }
  else
  {
    SendByteTerminalNoParity(0x3B, inverse_convention);
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, 0x3B);
  }

  LoopTerminalETU(250);
  SendByteTerminalNoParity(0x60, inverse_convention);
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, 0x60);
  LoopTerminalETU(2);
  SendByteTerminalNoParity(0x00, inverse_convention);
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, 0x00);
  LoopTerminalETU(2);
  SendByteTerminalNoParity(TC1, inverse_convention);
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, TC1);
  LoopTerminalETU(2);
}

/**
 * Receives the ATR from ICC after a successful activation
 * 
 * @param inverse_convention non-zero if inverse convention
 * is to be used
 * @param proto 0 for T=0 and non-zero for T=1
 * @param TS is the TS byte of the ATR
 * @param T0 is the T0 byte of the ATR
 * @param selection is a 16-bit value, which will contain a 1-hot
 * encoded list of which ATR bytes have been received as follows:
 *   -> (selection & (1 << b)) == 1 if byte 'b' has been received, 0 otherwise.
 *   -> The order of bytes is:
 *      TA1, TB1, TC1, TD1, TA2, TB2, ..., TA4, TB4, TC4, TD4 (total of 16 bits)
 *      TA1 corresponds to the most significant bit of 'selection'.
 *   See ISO 7816-3 or EMV Book 1 section ATR
 * @param bytes a user supplied vector which will contain the values of:
 *   -> TA1, TB1, ..., TA4, TB4, TC4 and TD4 in bytes [0 ... 15] (16 values)
 *   -> historic bytes in bytes [16 ... 31] (16 values). See last nibble of T0
 *   for the number of historic bytes (i.e. T0 & 0x0F).
 * @param tck the TCK byte of the ATR if T=1 is used. A storage for this value
 * must be supplied by the caller even if the value is not used.
 * @param logger a pointer to a log structure or NULL if no log is desired.
 * @return zero if successful, non-zero otherwise
 *  
 * This implementation is compliant with EMV 4.2 Book 1
 */ 
uint8_t GetATRICC(
    uint8_t *inverse_convention,
    uint8_t *proto,
    uint8_t *TS,
    uint8_t *T0,
    uint16_t *selection,
    uint8_t bytes[32],
    uint8_t *tck,
    log_struct_t *logger)
{   
  uint8_t history, i, ta, tb, tc, td, nb;
  uint8_t check = 0; // used only for T=1
  uint8_t error, index;

  if(inverse_convention == NULL ||
      proto == NULL || TS == NULL || T0 == NULL ||
      selection == NULL || bytes == NULL || tck == NULL)
  {
    error = RET_ERR_PARAM;
    goto enderror;
  }

  *selection = 0;
  memset(bytes, 0, 32);

  // Get TS
  GetByteICCNoParity(0, TS);
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, *TS);
  if(*TS == 0x3B) *inverse_convention = 0;
  else if(*TS == 0x03) *inverse_convention = 1;
  else
  {
    error = RET_ICC_INIT_ATR_TS;
    goto enderror;
  }

  // Get T0
  error = GetByteICCNoParity(*inverse_convention, T0);
  if(error)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, *T0);
  check ^= *T0;
  history = *T0 & 0x0F;
  ta = *T0 & 0x10;
  tb = *T0 & 0x20;
  tc = *T0 & 0x40;
  td = *T0 & 0x80;
  if(tb == 0)
  {
    error = RET_ICC_INIT_ATR_T0;
    goto enderror;
  }

  index = 0;
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
    error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
    if(error)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
    check ^= bytes[index];
    *selection |= (1 << (15-index));
  }
  index++;

  // Get TB1
  error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
  if(error)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
  check ^= bytes[index];
  *selection |= (1 << (15-index));
  if(bytes[index] != 0)
  {
    error = RET_ICC_INIT_ATR_TB1;
    goto enderror;
  }
  index++;

  // Get TC1
  if(tc)
  {
    error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
    if(error)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
    check ^= bytes[index];
    *selection |= (1 << (15-index));
  }
  index++;

  if(td){
    // Get TD1
    error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
    if(error)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
    check ^= bytes[index];
    *selection |= (1 << (15-index));
    nb = bytes[index] & 0x0F;
    ta = bytes[index] & 0x10;
    tb = bytes[index] & 0x20;
    tc = bytes[index] & 0x40;
    td = bytes[index] & 0x80;
    if(nb == 0x01) *proto = 1;
    else if(nb == 0x00) *proto = 0;
    else
    {
      error = RET_ICC_INIT_ATR_TD1;
      goto enderror;
    }
    index++;

    // The SCD does not currently support specific modes of operation.
    // Perhaps we can trigger a PTS selection or reset in the future.
    if(ta)
    {
      error = RET_ICC_INIT_ATR_TA2;
      goto enderror;
    }
    index++;

    if(tb)
    {
      error = RET_ICC_INIT_ATR_TB2;
      goto enderror;
    }
    index++;

    if(tc){
      // Get TC2
      error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
      if(error)
        goto enderror;
      if(logger)
        LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
      check ^= bytes[index];
      *selection |= (1 << (15-index));
      if(bytes[index] != 0x0A)
      {
        error = RET_ICC_INIT_ATR_TC2;
        goto enderror;
      }
    }
    index++;

    if(td){
      // Get TD2
      error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
      if(error)
        goto enderror;
      if(logger)
        LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
      check ^= bytes[index];
      *selection |= (1 << (15-index));
      nb = bytes[index] & 0x0F;
      ta = bytes[index] & 0x10;
      tb = bytes[index] & 0x20;
      tc = bytes[index] & 0x40;
      td = bytes[index] & 0x80;
      index++;
      // we allow any value of nb although EMV restricts to some values
      // these values could be used if we implement PTS

      if(ta)
      {
        // Get TA3
        error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
        if(error)
          goto enderror;
        if(logger)
          LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
        check ^= bytes[index];
        *selection |= (1 << (15-index));
        if(bytes[index] < 0x0F || bytes[index] == 0xFF)
        {
          error = RET_ICC_INIT_ATR_TA3;
          goto enderror;
        }
      }
      else
        bytes[index] = 0x20;
      index++;

      if(*proto == 1 && tb == 0)
      {
        error = RET_ICC_INIT_ATR_TB3;
        goto enderror;
      }

      if(tb)
      {
        // Get TB3
        error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
        if(logger)
          LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
        check ^= bytes[index];
        *selection |= (1 << (15-index));
        nb = bytes[index] & 0x0F;
        if(nb > 5)
        {
          error = RET_ICC_INIT_ATR_TB3;
          goto enderror;
        }
        nb = bytes[index] & 0xF0;
        if(nb > 64)
        {
          error = RET_ICC_INIT_ATR_TB3;
          goto enderror;
        }
      }
      index++;

      if(*proto == 0 && tc != 0)
      {
        error = RET_ICC_INIT_ATR_TC3;
        goto enderror;
      }
      if(tc)
      {
        // Get TC3
        error = GetByteICCNoParity(*inverse_convention, &bytes[index]);
        if(error)
          goto enderror;
        if(logger)
          LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index]);
        check ^= bytes[index];
        *selection |= (1 << (15-index));
        if(bytes[index] != 0)
        {
          error = RET_ICC_INIT_ATR_TC3;
          goto enderror;
        }
      }
      index++;
    }
  }
  else
    *proto = 0;

  // Get historical bytes
  index = 16;
  for(i = 0; i < history; i++)
  {
    error = GetByteICCNoParity(*inverse_convention, &bytes[index + i]);
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, bytes[index + i]);
    check ^= bytes[index + i];
  }

  // get TCK if T=1 is used
  if(*proto == 1)
  {
    error = GetByteICCNoParity(*inverse_convention, tck);
    if(error)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_FROM_ICC, *tck);
    check ^= *tck;
    if(check != 0)
    {
      error = RET_ICC_INIT_ATR_T1_CHECK;
      goto enderror;
    }
  }

  error = 0;

enderror:
  return error;
}




/**
 * Makes a command header for T=0 protocol
 * 
 * @param cla byte CLA
 * @param ins byte INS
 * @param p1 byte P1
 * @param p2 byte P2
 * @param p3 byte P3
 * @return the structure representing the command header. This function
 * allocates memory for the command header. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 */
EMVCommandHeader* MakeCommandHeader(uint8_t cla, uint8_t ins, uint8_t p1, 
    uint8_t p2, uint8_t p3)
{
  EMVCommandHeader *cmd = (EMVCommandHeader*)malloc(sizeof(EMVCommandHeader));
  if(cmd == NULL) return NULL;

  cmd->cla = cla;
  cmd->ins = ins;
  cmd->p1 = p1;
  cmd->p2 = p2;
  cmd->p3 = p3;

  return cmd;
}

/**
 * Makes a command header for a given command. This function populates
 * the command bytes with default values, where P3 is always 0. Calling
 * functions should make sure to modify these bytes to the correct values.
 * 
 * @param command type of command requested (see EMV_CMD)
 * @return the structure representing the command header. This function
 * allocates memory for the command header. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 * @sa MakeCommandHeader
 */
EMVCommandHeader* MakeCommandHeaderC(EMV_CMD command)
{
  EMVCommandHeader *cmd = (EMVCommandHeader*)malloc(sizeof(EMVCommandHeader));
  if(cmd == NULL) return NULL;

  // the default case, modified below where needed
  cmd->cla = 0;
  cmd->ins = 0;  
  cmd->p1 = 0;
  cmd->p2 = 0;  
  cmd->p3 = 0;  

  switch(command)
  {
    case CMD_SELECT:
      cmd->ins = 0xA4;
      cmd->p1 = 0x04;
      break;

    case CMD_GET_RESPONSE:
      cmd->ins = 0xC0;
      break;

    case CMD_READ_RECORD:
      cmd->ins = 0xB2;
      cmd->p1 = 0x01;
      break;

    case CMD_GET_PROCESSING_OPTS:
      cmd->cla = 0x80;
      cmd->ins = 0xA8;
      break;

    case CMD_VERIFY:
      cmd->ins = 0x20;
      cmd->p2 = 0x80;
      break;

    case CMD_GENERATE_AC:
      cmd->cla = 0x80;
      cmd->ins = 0xAE;
      break;

    case CMD_GET_DATA:
      cmd->cla = 0x80;
      cmd->ins = 0xCA;
      cmd->p1 = 0x9F;
      cmd->p2 = 0x17;
      break;

    case CMD_INTERNAL_AUTHENTICATE:
      cmd->ins = 0x88;
      break;

    case CMD_PIN_CHANGE_UNBLOCK:
      cmd->cla = 0x8C;
      cmd->ins = 0x24;
      break;
  }

  return cmd;
}


/**
 * Makes a command APDU (CAPDU) for T=0 protocol
 * 
 * @param cla byte CLA
 * @param ins byte INS
 * @param p1 byte P1
 * @param p2 byte P2
 * @param p3 byte P3. This value is not modified automatically
 * to the value of lenData even if cmdData is not NULL. The caller
 * needs to take care of this or use MakeCommandC instead
 * @param cmdData command data. The data pointed by cmdData will
 * be copied into the new CAPDU so the caller is responsible for handling
 * the original cmdData
 * @param lenData length in bytes of cmdData
 * @return the structure representing the CAPDU. This function
 * allocates memory for the command header. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 * @sa MakeCommandC
 */
CAPDU* MakeCommand(uint8_t cla, uint8_t ins, uint8_t p1,
    uint8_t p2, uint8_t p3, const uint8_t cmdData[], uint8_t lenData)
{
  CAPDU *cmd = (CAPDU*)malloc(sizeof(CAPDU));
  if(cmd == NULL) return NULL;

  cmd->cmdHeader = MakeCommandHeader(cla, ins, p1, p2, p3);
  if(cmd->cmdHeader == NULL)
  {
    free(cmd);
    return NULL;
  }

  if(cmdData != NULL && lenData != 0)
  {
    cmd->cmdData = (uint8_t*)malloc(lenData * sizeof(uint8_t));
    if(cmd->cmdData == NULL)
    {
      FreeCAPDU(cmd);
      return NULL;
    }
    memcpy(cmd->cmdData, cmdData, lenData);
    cmd->lenData = lenData;
  }
  else
  {
    cmd->cmdData = NULL;
    cmd->lenData = 0;
  }

  return cmd;
}

/**
 * Makes a command APDU (CAPDU) for T=0 protocol. The 
 * difference to MakeCommand is that it takes a pointer to
 * an EMVCommandHeader structure instead of values
 * 
 * @param cmdHdr command header. The data pointed by cmdHdr will
 * be copied into the new CAPDU so the caller is responsible for handling
 * the original cmdHdr
 * @param cmdData command data. The data pointed by cmdData will
 * be copied into the new CAPDU so the caller is responsible for handling
 * the original cmdData
 * @param lenData length in bytes of cmdData
 * @return the structure representing the CAPDU. This function
 * allocates memory for the command header. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 * @sa MakeCommand
 */
CAPDU* MakeCommandP(const EMVCommandHeader *cmdHdr, const uint8_t cmdData[],
    uint8_t lenData)
{
  if(cmdHdr == NULL) return NULL;

  CAPDU *cmd = (CAPDU*)malloc(sizeof(CAPDU));
  if(cmd == NULL) return NULL;

  cmd->cmdHeader = MakeCommandHeader(cmdHdr->cla, cmdHdr->ins,
      cmdHdr->p1, cmdHdr->p2, cmdHdr->p3);
  if(cmd->cmdHeader == NULL)
  {
    free(cmd);
    return NULL;
  }

  if(cmdData != NULL && lenData != 0)
  {
    cmd->cmdData = (uint8_t*)malloc(lenData * sizeof(uint8_t));
    if(cmd->cmdData == NULL)
    {
      FreeCAPDU(cmd);
      return NULL;
    }
    memcpy(cmd->cmdData, cmdData, lenData);
    cmd->lenData = lenData;
  }
  else
  {
    cmd->cmdData = NULL;
    cmd->lenData = 0;
  }

  return cmd;
}

/**
 * Makes a command APDU (CAPDU) for T=0 protocol. This
 * method creates sets the default values based on the command
 * type (see EMV_CMD). Also, the parameter p3 of the command
 * header is given the value of lenData if cmdData is not NULL.
 * 
 * @param command The speified command using the enum EMV_CMD.
 * This function will create the default command header based on this.
 * @param cmdData command data. The data pointed by cmdData will
 * be copied into the new CAPDU so the caller is responsible for handling
 * the original cmdData
 * @param lenData length in bytes of cmdData
 * @return the structure representing the CAPDU. This function
 * allocates memory for the command header. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 * @sa MakeCommand
 */
CAPDU* MakeCommandC(EMV_CMD command, const uint8_t cmdData[],
    uint8_t lenData)
{
  CAPDU *cmd = (CAPDU*)malloc(sizeof(CAPDU));
  if(cmd == NULL) return NULL;

  cmd->cmdHeader = MakeCommandHeaderC(command);
  if(cmd->cmdHeader == NULL)
  {
    free(cmd);
    return NULL;
  }

  if(cmdData != NULL && lenData != 0)
  {
    cmd->cmdData = (uint8_t*)malloc(lenData * sizeof(uint8_t));
    if(cmd->cmdData == NULL)
    {
      FreeCAPDU(cmd);
      return NULL;
    }
    memcpy(cmd->cmdData, cmdData, lenData);
    cmd->lenData = lenData;
    cmd->cmdHeader->p3 = lenData;
  }
  else
  {
    cmd->cmdData = NULL;
    cmd->lenData = 0;
  }

  return cmd;
}

/**
 * This function is used to establish the communication between
 * the terminal and the SCD and between the SCD and the ICC at
 * the same time.
 *
 * The ATR from the card is replicated to the terminal, with the exception
 * of the first byte (TS) which is dependent on the given parameter (t_inverse),
 * since this will be sent before retrieving the corresponding ICC value.
 *
 * If the function returs 0 (success) then both the terminal
 * and the ICC should be in a good state, where the terminal
 * is about to send the first command and the ICC is waiting
 * for a command
 *
 * This function assumes that the ICC was placed in the ICC
 * holder before being called and it will loop until the
 * terminal provides clock
 *
 * @param t_inverse specifies if direct(0) or inverse(non-zero)
 * convention should be used in the communication with the terminal.
 * Only direct convention should be used as specified in the standard.
 * @param t_TC1 specifies the TC1 byte of the ATR sent to the terminal.
 * This should be as small as possible in order to limit the latency
 * of communication or large if a specific timeout between bytes is desired.
 * @param inverse_convention direct (0) or inverse convention (1) is
 * used by the ICC, as returned in the ATR
 * @param proto protocol (T=0 or T=1) as returned by the ICC in the ATR
 * @param TC1 as returned by the ICC in the ATR
 * @param TA3 as returned by the ICC in the ATR
 * @param TB3 as returned by the ICC in the ATR
 * @param logger the log structure or NULL if no log is desired
 * @return 0 if success, non-zero otherwise.
 * @sa GetATRICC
 */
uint8_t InitSCDTransaction(uint8_t t_inverse, uint8_t t_TC1, 
    uint8_t *inverse_convention, uint8_t *proto, uint8_t *TC1, 
    uint8_t *TA3, uint8_t *TB3, log_struct_t *logger)
{
  uint8_t tmp;
  int8_t tmpi;
  uint16_t atr_selection;
  uint8_t atr_bytes[32];
  uint8_t atr_tck;
  uint8_t icc_T0, icc_TS;
  uint8_t error;
  uint8_t index;
  uint8_t history;
  uint8_t done = 0;
  uint32_t i;


  // start timer for terminal
  StartCounterTerminal();	

  // wait for terminal CLK enough to allow for consecutive resets that may
  // happen with large delay while logging some particular application.
  // However, don't wait too much (e.g. more than 10-15 s) as that would be
  // annoying.
  // Note that we can get some clock at this point during the card deactivation
  // procedure, for which we compensate on the next check.
  error = WaitTerminalClock(MAX_WAIT_TERMINAL_CLK);
  if(error)
  {
#if DEBUG
    LogByte1(logger, LOG_DEBUG_TEST1, 0);
#endif
    if(logger)
    {
      LogCurrentTime(logger);
      LogByte1(logger, LOG_TERMINAL_NO_CLOCK, 0);
    }
    goto enderror;
  }

  // Wait enough time for terminal reset to go high,
  // in order to allow logging transactions with a large delay
  // between consecutive resets from same transactions
  error = WaitTerminalResetHigh(MAX_WAIT_TERMINAL_RESET);
  if(error)
  {
#if DEBUG
    LogByte1(logger, LOG_DEBUG_TEST2, 0);
#endif
    if(logger)
    {
      LogCurrentTime(logger);
      LogByte1(logger, LOG_TERMINAL_TIME_OUT, 0);
    }
    goto enderror;
  }

  // Check we still have clock from terminal, in case the reset high is due to
  // SCD pull-ups and physically disconnected terminal.
  if(IsTerminalClock() == 0)
  {
#if DEBUG
    LogByte1(logger, LOG_DEBUG_TEST3, 0);
#endif
    if(logger)
    {
      LogCurrentTime(logger);
      LogByte1(logger, LOG_TERMINAL_NO_CLOCK, 0);
    }
    error = RET_TERMINAL_NO_CLOCK;
    goto enderror;
  }

  if(logger)
  {
    LogCurrentTime(logger);
    LogByte1(logger, LOG_TERMINAL_RST_HIGH, 0);
  }

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

  // activate ICC after sending TS
  if(ActivateICC(0))
  {
    error = RET_ERROR;
    goto enderror;
  }
  if(logger)
    LogByte1(logger, LOG_ICC_ACTIVATED, 0);

  // Wait for 40000 ICC clocks and put reset to high, then get ATR
  LoopICCETU(41000 / 372);
  PORTD |= _BV(PD4);		
  if(logger)
    LogByte1(logger, LOG_ICC_RST_HIGH, 0);

  // Wait for ATR from ICC for a maximum of 42000 clock cycles + 40 ms
  // this number is based on the assembler of this function
  if(WaitForICCData(50000))	
  {
    error = RET_ERROR; 				// May be changed with a warm reset
    DeactivateICC();
    if(logger)
      LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
    goto enderror;
  }

  error = GetATRICC(
      inverse_convention, proto, &icc_TS, &icc_T0,
      &atr_selection, atr_bytes, &atr_tck, logger);
  if(error)
  {
    DeactivateICC();
    if(logger)
      LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
    goto enderror;
  }
  *TC1 = atr_bytes[2];
  *TA3 = atr_bytes[8];
  *TB3 = atr_bytes[9];
  history = icc_T0 & 0x0F;

  // Send the rest of the ATR to the terminal
  SendByteTerminalNoParity(icc_T0, t_inverse);
  if(logger)
    LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, icc_T0);
  LoopTerminalETU(2);

  for(index = 0; index < 16; index++)
  {
    if(atr_selection & (1 << (15-index)))
    {
      SendByteTerminalNoParity(atr_bytes[index], t_inverse);
      if(logger)
        LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, atr_bytes[index]);
      LoopTerminalETU(2);
    }
  }

  for(index = 0; index < history; index++)
  {
    SendByteTerminalNoParity(atr_bytes[16 + index], t_inverse);
    if(logger)
      LogByte1(logger, LOG_BYTE_ATR_TO_TERMINAL, atr_bytes[16 + index]);
    LoopTerminalETU(2);
  }

  error = 0;

enderror:
  return error;	
}


/**
 * Returns the case of an EMV command based on the header
 *
 * The significance of the case is given by this table:
 *
 * case		|	command data	|	response data
 * 	1		|	absent			|	absent
 * 	2		|	absent			|	present
 * 	3		|	present			|	absent
 * 	4		|	present			|	present
 * 
 * @param cla byte CLA
 * @param ins byte INS
 * @return the command case (1, 2, 3 or 4) based on the
 * command CLA and INS bytes. This function returns 0
 * if the command is unknown.
 */
uint8_t GetCommandCase(uint8_t cla, uint8_t ins)
{
  switch(cla)
  {
    case 0:
      {
        switch(ins)
        {
          case 0xC0: // GET RESPONSE
            return 2;
            break;

          case 0xB2: // READ RECORD
            return 2;
            break;

          case 0xA4: // SELECT
            return 4;
            break;

          case 0x82: // EXTERNAL AUTHENTICATE
            return 3;
            break;

          case 0x84: // GET CHALLENGE
            return 2;
            break;

          case 0x88: // INTERNAL AUTHENTICATE
            return 4;
            break;

          case 0x20: // VERIFY
            return 3;
            break;				

          default: return 0;
        }
      }
      break;

    case 0x8C:
    case 0x84:
      {
        switch(ins)
        {
          case 0x1E: // APPLICATION BLOCK
            return 3;
            break;

          case 0x18: // APPLICATION UNBLOCK
            return 3;
            break;

          case 0x16: // CARD BLOCK
            return 3;
            break;

          case 0x24: // PIN CHANGE/UNBLOCK
            return 3;
            break;

          default: return 0;
        }
      }
      break;

    case 0x80:
      {
        switch(ins)
        {
          case 0xAE: // GENERATE AC
            return 4;
            break;

          case 0xCA: // GET DATA
            return 2;
            break;

          case 0xA8: // GET PROCESSING OPTS
            return 4;
            break;

          default: return 0;
        }			
      }
      break;

    default: return 0;
  }

  return 0;
}


/**
 * Receive a command header from terminal for protocol T = 0
 *
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return command header to be received if successful. This function
 * allocates memory (5 bytes) for the command header. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 */
EMVCommandHeader* ReceiveT0CmdHeader(
    uint8_t inverse_convention,
    uint8_t TC1,
    log_struct_t *logger)
{
  uint8_t tdelay, result;
  EMVCommandHeader *cmdHeader;

  cmdHeader = (EMVCommandHeader*)malloc(sizeof(EMVCommandHeader));
  if(cmdHeader == NULL)
  {
    if(logger)
      LogByte1(logger, LOG_ERROR_MEMORY, 0);
    return NULL;
  }

  tdelay = 1 + TC1;

  result = GetByteTerminalParity(
      inverse_convention, &(cmdHeader->cla), MAX_WAIT_TERMINAL_CMD);
  if(result != 0)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_TERMINAL, cmdHeader->cla);
  LoopTerminalETU(tdelay);	

  result = GetByteTerminalParity(
      inverse_convention, &(cmdHeader->ins), MAX_WAIT_TERMINAL_CMD);
  if(result != 0)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_TERMINAL, cmdHeader->ins);
  LoopTerminalETU(tdelay);	

  result = GetByteTerminalParity(
      inverse_convention, &(cmdHeader->p1), MAX_WAIT_TERMINAL_CMD);
  if(result != 0)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_TERMINAL, cmdHeader->p1);
  LoopTerminalETU(tdelay);	

  result = GetByteTerminalParity(
      inverse_convention, &(cmdHeader->p2), MAX_WAIT_TERMINAL_CMD);
  if(result != 0)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_TERMINAL, cmdHeader->p2);
  LoopTerminalETU(tdelay);	

  result = GetByteTerminalParity(
      inverse_convention, &(cmdHeader->p3), MAX_WAIT_TERMINAL_CMD);
  if(result != 0)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_TERMINAL, cmdHeader->p3);

  return cmdHeader;

enderror:
  free(cmdHeader);
  if(logger)
  {
    LogCurrentTime(logger);

    if(result == RET_TERMINAL_RESET_LOW)
    {
      LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);
    }
    else if(result == RET_TERMINAL_TIME_OUT)
    {
      LogByte1(logger, LOG_TERMINAL_TIME_OUT, 0);
    }
    else if(result == RET_TERMINAL_NO_CLOCK)
    {
      LogByte1(logger, LOG_TERMINAL_NO_CLOCK, 0);
    }
    else if(result == RET_ERROR)
    {
      LogByte1(logger, LOG_TERMINAL_ERROR_RECEIVE, 0);
    }
  }
  return NULL;
}

/**
 * Receive a command data from terminal for protocol T = 0
 *
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @return cmdData command data to be received. The caller must
 * ensure that enough memory is already allocated for cmdData.
 * @param len lenght in bytes of command data expected
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return command data to be received if successful. This function
 * allocates memory for the command data. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 */
uint8_t* ReceiveT0CmdData(
    uint8_t inverse_convention,
    uint8_t TC1,
    uint8_t len,
    log_struct_t *logger)
{
  uint8_t tdelay, i, result;
  uint8_t *cmdData;

  cmdData = (uint8_t*)malloc(len*sizeof(uint8_t));
  if(cmdData == NULL)
  {
    if(logger)
      LogByte1(logger, LOG_ERROR_MEMORY, 0);
    return NULL;
  }

  tdelay = 1 + TC1;

  for(i = 0; i < len - 1; i++)
  {
    result = GetByteTerminalParity(
        inverse_convention, &(cmdData[i]), MAX_WAIT_TERMINAL_CMD);
    if(result != 0)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_TERMINAL, cmdData[i]);
    LoopTerminalETU(tdelay);	
  }

  // Do not add a delay after the last byte
  result = GetByteTerminalParity(
      inverse_convention, &(cmdData[i]), MAX_WAIT_TERMINAL_CMD);
  if(result != 0)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_TERMINAL, cmdData[i]);

  return cmdData;	

enderror:
  free(cmdData);
  if(logger)
  {
    if(result == RET_TERMINAL_RESET_LOW)
    {
      LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);
    }
    else if(result == RET_TERMINAL_TIME_OUT)
    {
      LogByte1(logger, LOG_TERMINAL_TIME_OUT, 0);
    }
    else if(result == RET_ERROR)
    {
      LogByte1(logger, LOG_TERMINAL_ERROR_RECEIVE, 0);
    }
  }
  return NULL;
}

/**
 * Receive a command (including data) from terminal for protocol T = 0.
 * For command cases 3 and 4 a procedure byte is sent back to the
 * terminal to obtain the command data
 * 
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return command to be received. This function will allocate
 * the necessary memory for this structure, which should contain
 * valid data if the method returs success. If the method is not
 * successful then it will return NULL  
 */
CAPDU* ReceiveT0Command(
    uint8_t inverse_convention, 
    uint8_t TC1,
    log_struct_t *logger)
{
  uint8_t tdelay, tmp;
  CAPDU *cmd;

  tdelay = 1 + TC1;

  cmd = (CAPDU*)malloc(sizeof(CAPDU));
  if(cmd == NULL)
  {
    if(logger)
      LogByte1(logger, LOG_ERROR_MEMORY, 0);
    return NULL;
  }
  cmd->cmdHeader = NULL;
  cmd->cmdData = NULL;
  cmd->lenData = 0;

  cmd->cmdHeader = ReceiveT0CmdHeader(inverse_convention, TC1, logger);
  if(cmd->cmdHeader == NULL)
  {
    free(cmd);		
    return NULL;
  }	
  tmp = GetCommandCase(cmd->cmdHeader->cla, cmd->cmdHeader->ins);
  if(tmp == 0)
  {
    FreeCAPDU(cmd);
    return NULL;
  }

  // for case 1 and case 2 commands there is no command data to receive
  if(tmp == 1 || tmp == 2)
    return cmd;

  // for other cases (3, 4) receive command data
  // wait for terminal to be ready to accept the byte
  LoopTerminalETU(6);
  if(SendByteTerminalParity(cmd->cmdHeader->ins, inverse_convention))
  {
    free(cmd->cmdHeader);
    cmd->cmdHeader = NULL;
    free(cmd);		
    if(logger)
      LogByte1(logger, LOG_TERMINAL_ERROR_SEND, 0);
    return NULL;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_TERMINAL, cmd->cmdHeader->ins);

  LoopTerminalETU(tdelay);	
  cmd->lenData = cmd->cmdHeader->p3;
  cmd->cmdData = ReceiveT0CmdData(
      inverse_convention, TC1, cmd->lenData, logger);
  if(cmd->cmdData == NULL)
  {
    free(cmd->cmdHeader);
    cmd->cmdHeader = NULL;
    free(cmd);		
    return NULL;	
  }

  return cmd;	
}


/**
 * Send a command header to the ICC for protocol T = 0
 *
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @param cmdHeader command header to be sent
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0CmdHeader(
    uint8_t inverse_convention,
    uint8_t TC1,
    EMVCommandHeader *cmdHeader,
    log_struct_t *logger)
{
  uint8_t tdelay;

  if(cmdHeader == NULL) return RET_ERROR;

  tdelay = 1 + TC1;

  if(SendByteICCParity(cmdHeader->cla, inverse_convention))
  {
    if(logger)
      LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
    return RET_ERROR;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_ICC, cmdHeader->cla);
  LoopICCETU(tdelay);	

  if(SendByteICCParity(cmdHeader->ins, inverse_convention))
  {
    if(logger)
      LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
    return RET_ERROR;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_ICC, cmdHeader->ins);
  LoopICCETU(tdelay);	

  if(SendByteICCParity(cmdHeader->p1, inverse_convention))
  {
    if(logger)
      LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
    return RET_ERROR;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_ICC, cmdHeader->p1);
  LoopICCETU(tdelay);	

  if(SendByteICCParity(cmdHeader->p2, inverse_convention))
  {
    if(logger)
      LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
    return RET_ERROR;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_ICC, cmdHeader->p2);
  LoopICCETU(tdelay);	

  if(SendByteICCParity(cmdHeader->p3, inverse_convention))
  {
    if(logger)
      LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
    return RET_ERROR;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_ICC, cmdHeader->p3);

  return 0;
}


/**
 * Send a command data to the ICC for protocol T = 0
 *
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @param cmdData command data to be sent
 * @param len lenght in bytes of command data to be sent
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0CmdData(
    uint8_t inverse_convention,
    uint8_t TC1,
    uint8_t *cmdData,
    uint8_t len,
    log_struct_t *logger)
{
  uint8_t tdelay, i;

  if(cmdData == NULL) return RET_ERROR;

  tdelay = 1 + TC1;

  for(i = 0; i < len - 1; i++)
  {
    if(SendByteICCParity(cmdData[i], inverse_convention))
    {
      if(logger)
        LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
      return RET_ERROR;	
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_TO_ICC, cmdData[i]);
    LoopICCETU(tdelay);	
  }

  // Do not add a delay after the last byte
  if(SendByteICCParity(cmdData[i], inverse_convention))
  {
    if(logger)
      LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
    return RET_ERROR;	
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_ICC, cmdData[i]);

  return 0;
}


/**
 * Send a command (including data) to the ICC for protocol T = 0.
 * For command cases 3 and 4 a procedure byte(s) is expected back
 * before sending the data
 * 
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @param cmd command to be sent
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0Command(
    uint8_t inverse_convention,
    uint8_t TC1,
    CAPDU *cmd,
    log_struct_t *logger)
{
  uint8_t tdelay, tmp, tmp2, i;	

  if(cmd == NULL) return RET_ERROR;
  tdelay = 1 + TC1;
  LogCurrentTime(logger);

  tmp = GetCommandCase(cmd->cmdHeader->cla, cmd->cmdHeader->ins);	
  if(tmp == 0)
    return RET_ERROR;
  if(SendT0CmdHeader(inverse_convention, TC1, cmd->cmdHeader, logger))
    return RET_ERROR;

  // for case 1 and case 2 commands there is no command data to send
  if(tmp == 1 || tmp == 2)
    return 0;

  // for other cases (3, 4) get procedure byte and send command data
  LoopICCETU(6);

  // Get firs byte (can be INS, ~INS, 60, 61, 6C or other in case of error)
  if(GetByteICCParity(inverse_convention, &tmp))
  {
    if(logger)
      LogByte1(logger, LOG_ICC_ERROR_RECEIVE, 0);
    return RET_ERROR;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_ICC, tmp);

  while(tmp == SW1_MORE_TIME)
  {
    LoopICCETU(1);
    if(GetByteICCParity(inverse_convention, &tmp))
    {
      if(logger)
        LogByte1(logger, LOG_ICC_ERROR_RECEIVE, 0);
      return RET_ERROR;
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, tmp);
  }

  // if we don't get INS or ~INS then
  // get another byte and then exit, operation unexpected
  if((tmp != cmd->cmdHeader->ins) && (tmp != ~(cmd->cmdHeader->ins)))
  {
    if(GetByteICCParity(inverse_convention, &tmp2))
    {
      if(logger)
        LogByte1(logger, LOG_ICC_ERROR_RECEIVE, 0);
      return RET_ERROR;
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, tmp2);
    return RET_ERR_CHECK; 
  }

  // Wait for card to be ready before sending any bytes
  LoopICCETU(6);

  i = 0;
  // Send first byte if sending byte by byte
  if(tmp != cmd->cmdHeader->ins)
  {
    if(SendByteICCParity(cmd->cmdData[i++], inverse_convention))
    {
      if(logger)
        LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
      return RET_ERROR;
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_TO_ICC, cmd->cmdData[i-1]);
    if(i < cmd->lenData)
      LoopICCETU(6);
  }

  // send byte after byte in case we receive !INS instead of INS
  while(tmp != cmd->cmdHeader->ins && i < cmd->lenData)
  {
    if(GetByteICCParity(inverse_convention, &tmp))
    {
      if(logger)
        LogByte1(logger, LOG_ICC_ERROR_RECEIVE, 0);
      return RET_ERROR;
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, tmp);
    LoopICCETU(6);

    if(tmp != cmd->cmdHeader->ins)
    {
      if(SendByteICCParity(cmd->cmdData[i++], inverse_convention))
      {
        if(logger)
          LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
        return RET_ERROR;
      }
      if(logger)
        LogByte1(logger, LOG_BYTE_TO_ICC, cmd->cmdData[i-1]);
      if(i < cmd->lenData)
        LoopICCETU(6);
    }
  }

  // send remaining of bytes, if any
  for(; i < cmd->lenData - 1; i++)
  {
    if(SendByteICCParity(cmd->cmdData[i], inverse_convention))
    {
      if(logger)
        LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
      return RET_ERROR;
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_TO_ICC, cmd->cmdData[i]);
    LoopICCETU(tdelay);
  }
  if(i == cmd->lenData - 1)
  {
    if(SendByteICCParity(cmd->cmdData[i], inverse_convention))
    {
      if(logger)
        LogByte1(logger, LOG_ICC_ERROR_SEND, 0);
      return RET_ERROR;
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_TO_ICC, cmd->cmdData[i]);
  }

  return 0;
}


/**
 * Receive a command from the terminal and then send it to the ICC
 *
 * @param tInverse different than 0 if inverse convention is to be used
 * with the terminal
 * @param cInverse different than 0 if inverse convention is to be used
 * with the ICC
 * @param tTC1 the N parameter from byte TC1 of ATR used with terminal
 * @param cTC1 the N parameter from byte TC1 of ATR received from ICC
 * @param log_dir specifies the direction of the log
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the command that has been forwarded if successful. If this
 * method is not successful then it will return NULL
 */
CAPDU* ForwardCommand(
    uint8_t tInverse,
    uint8_t cInverse,
    uint8_t tTC1,
    uint8_t cTC1,
    uint8_t log_dir,
    log_struct_t *logger)
{
  CAPDU* cmd;
  uint8_t err;

  if((log_dir & LOG_DIR_TERMINAL) > 0)
    cmd = ReceiveT0Command(tInverse, tTC1, logger);
  else
    cmd = ReceiveT0Command(tInverse, tTC1, NULL);
  if(cmd == NULL) return NULL;

  if((log_dir & LOG_DIR_ICC) > 0)
    err = SendT0Command(cInverse, cTC1, cmd, logger);
  else
    err = SendT0Command(cInverse, cTC1, cmd, NULL);

  if(err != 0)
  {
    FreeCAPDU(cmd);
    return NULL;
  }

  return cmd;
}


/**
 * This function serializes (converts to a sequence of bytes) a CAPDU
 * structure.
 *
 * @param cmd command to be serialized
 * @param len length of the resulted byte stream
 * @return byte stream representing the serialized structure. This
 * method will allocate the necessary space and will write into len
 * the length of the stream. The method returns NULL if unsuccessful. 
 */
uint8_t* SerializeCommand(CAPDU *cmd, uint32_t *len)
{
  uint8_t *stream, i = 0;

  if(cmd == NULL || len == NULL || cmd->cmdHeader == NULL) return NULL;
  if(cmd->lenData > 0 && cmd->cmdData == NULL) return NULL;

  *len = 5 + cmd->lenData;
  stream = (uint8_t*)malloc((*len)*sizeof(uint8_t));
  if(stream == NULL)
  {
    *len = 0;
    return NULL;
  }

  stream[i++] = cmd->cmdHeader->cla;
  stream[i++] = cmd->cmdHeader->ins;
  stream[i++] = cmd->cmdHeader->p1;
  stream[i++] = cmd->cmdHeader->p2;
  stream[i++] = cmd->cmdHeader->p3;

  while(i < cmd->lenData)
  {
    stream[i] = cmd->cmdData[i];
    i++;
  }

  return stream;
}


/**
 * This method receives a response from ICC for protocol T = 0.
 * If [SW1,SW2] != [0x90,0] then the response is not complete.
 * Either another command (e.g. get response) is expected, or
 * the previous command with different lc, or an error has occurred.
 * If [SW1,SW2] returned are '9000' then the command is successful and
 * it will also contain data if this was expected.
 * Different codes for the return codes can be found in EMV Book 1
 * and Book 3.
 *
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param cmdHeader the header of the command for which response is expected
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return response APDU if the method is successful. In the case this
 * method is unsuccessful (unrelated to SW1, SW2) then it will return NULL 
 */
RAPDU* ReceiveT0Response(
    uint8_t inverse_convention,
    EMVCommandHeader *cmdHeader,
    log_struct_t *logger)
{
  uint8_t tmp, i, result;
  RAPDU* rapdu;

  if(cmdHeader == NULL) return NULL;

  rapdu = (RAPDU*)malloc(sizeof(RAPDU));
  if(rapdu == NULL)
  {
    result = RET_ERR_MEMORY;
    goto enderror;
  }
  rapdu->repStatus = NULL;
  rapdu->repData = NULL;
  rapdu->lenData = 0;
  result = GetCommandCase(cmdHeader->cla, cmdHeader->ins);		
  if(result == 0)
  {
    result = RET_ERROR;
    goto enderror;
  }


  // for case 1 and case 3 there is no data expected, just status
  if(tmp == 1 || tmp == 3)
  {
    rapdu->repStatus = (EMVStatus*)malloc(sizeof(EMVStatus));
    if(rapdu->repStatus == NULL)
    {
      result = RET_ERR_MEMORY;
      goto enderror;
    }

    result = GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw1));
    if(result != 0)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, rapdu->repStatus->sw1);

    if(rapdu->repStatus->sw1 == 0x60)
    {
      // requested more time, recall
      FreeRAPDU(rapdu);
      return ReceiveT0Response(inverse_convention, cmdHeader, logger);
    }

    result = GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw2));
    if(result != 0)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, rapdu->repStatus->sw2);

    return rapdu;
  }

  // for case 2 and 4, we might get data based on first byte of response	
  result = GetByteICCParity(inverse_convention, &tmp);
  if(result != 0)
    goto enderror;
  if(logger)
    LogByte1(logger, LOG_BYTE_FROM_ICC, tmp);

  if(tmp == 0x60)
  {
    // requested more time, recall
    FreeRAPDU(rapdu);
    return ReceiveT0Response(inverse_convention, cmdHeader, logger);
  }

  if(tmp == cmdHeader->ins || tmp == ~cmdHeader->ins)	// get data
  {
    if(tmp == cmdHeader->ins)
      rapdu->lenData = cmdHeader->p3;
    else
      rapdu->lenData = 1;

    rapdu->repData = (uint8_t*)malloc(rapdu->lenData*sizeof(uint8_t));
    if(rapdu->repData == NULL)
    {
      result = RET_ERR_MEMORY;
      goto enderror;
    }

    for(i = 0; i < rapdu->lenData; i++)
    {
      result = GetByteICCParity(inverse_convention, &(rapdu->repData[i]));
      if(result != 0)
        goto enderror;
      if(logger)
        LogByte1(logger, LOG_BYTE_FROM_ICC, rapdu->repData[i]);
    }		

    rapdu->repStatus = (EMVStatus*)malloc(sizeof(EMVStatus));
    if(rapdu->repStatus == NULL)
    {
      result = RET_ERR_MEMORY;
      goto enderror;
    }

    result = GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw1));
    if(result != 0)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, rapdu->repStatus->sw1);

    result = GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw2));
    if(result != 0)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, rapdu->repStatus->sw2);

  }	
  else	// get second byte of response (no data)
  {
    rapdu->repStatus = (EMVStatus*)malloc(sizeof(EMVStatus));
    if(rapdu->repStatus == NULL)
    {			
      result = RET_ERR_MEMORY;
      goto enderror;
    }

    rapdu->repStatus->sw1 = tmp;
    result = GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw2));
    if(result != 0)
      goto enderror;
    if(logger)
      LogByte1(logger, LOG_BYTE_FROM_ICC, rapdu->repStatus->sw2);
  }

  return rapdu;

enderror:
  FreeRAPDU(rapdu);
  if(logger)
  {
    LogCurrentTime(logger);

    if(result == RET_ERR_MEMORY)
    {
      LogByte1(logger, LOG_ERROR_MEMORY, 0);
    }
    else if(result == RET_ERROR)
    {
      LogByte1(logger, LOG_ICC_ERROR_RECEIVE, 0);
    }
  }
  return NULL;
}


/**
 * Send a response (including data) to the terminal for protocol T = 0.
 * Data (if available) is sent with a procedure byte prepended
 * 
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param cmdHeader header of command for which response is being sent
 * @param response RAPDU containing the response to be sent
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0Response(
    uint8_t inverse_convention,
    EMVCommandHeader *cmdHeader,
    RAPDU *response,
    log_struct_t *logger)
{
  uint8_t i;	
  uint8_t result;

  if(cmdHeader == NULL || response == NULL || response->repStatus == NULL)
    return RET_ERR_PARAM;	

  if(response->lenData > 0 && response->repData != NULL)
  {
    result = SendByteTerminalParity(cmdHeader->ins, inverse_convention);
    if(result != 0)
    {
      if(logger)
      {
        LogCurrentTime(logger);
        LogByte1(logger, LOG_TERMINAL_ERROR_SEND, cmdHeader->ins);
      }
      goto enderror;
    }
    if(logger)
      LogByte1(logger, LOG_BYTE_TO_TERMINAL, cmdHeader->ins);
    LoopTerminalETU(2);

    for(i = 0; i < response->lenData; i++)
    {			
      result = SendByteTerminalParity(response->repData[i], inverse_convention);
      if(result != 0)
      {
        if(logger)
        {
          LogCurrentTime(logger);
          LogByte1(logger, LOG_TERMINAL_ERROR_SEND, response->repData[i]);
        }
        goto enderror;
      }
      if(logger)
        LogByte1(logger, LOG_BYTE_TO_TERMINAL, response->repData[i]);
      LoopTerminalETU(2);
    }
  }

  result = SendByteTerminalParity(response->repStatus->sw1, inverse_convention);
  if(result != 0)
  {
    if(logger)
    {
      LogCurrentTime(logger);
      LogByte1(logger, LOG_TERMINAL_ERROR_SEND, response->repStatus->sw1);
    }
    goto enderror;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_TERMINAL, response->repStatus->sw1);
  LoopTerminalETU(2);

  result = SendByteTerminalParity(response->repStatus->sw2, inverse_convention);
  if(result != 0)
  {
    if(logger)
    {
      LogCurrentTime(logger);
      LogByte1(logger, LOG_TERMINAL_ERROR_SEND, response->repStatus->sw2);
    }
    goto enderror;
  }
  if(logger)
    LogByte1(logger, LOG_BYTE_TO_TERMINAL, response->repStatus->sw2);
  LoopTerminalETU(2);

  return 0;

enderror:
  return RET_ERROR;
}

/**
 * Receive a response from the terminal and then send it to the terminal
 *
 * @param tInverse different than 0 if inverse convention is to be used
 * with the terminal
 * @param cInverse different than 0 if inverse convention is to be used
 * with the ICC
 * @param cmdHeader the header of the command for which response is expected
 * @param log_dir specifies the direction of the log
 * @param logger a pointer to a log structure or NULL if no log is desired.
 * @return the response that has been forwarded if successful. If this
 * method is not successful then it will return NULL
 */
RAPDU* ForwardResponse(
    uint8_t tInverse,
    uint8_t cInverse,
    EMVCommandHeader *cmdHeader,
    uint8_t log_dir,
    log_struct_t *logger)
{
  RAPDU* response;
  uint8_t err;

  if(cmdHeader == NULL)
    return NULL;

  if((log_dir & LOG_DIR_ICC) > 0)
    response = ReceiveT0Response(cInverse, cmdHeader, logger);
  else
    response = ReceiveT0Response(cInverse, cmdHeader, NULL);
  if(response == NULL)
    return NULL;

  if((log_dir & LOG_DIR_TERMINAL) > 0)
    err = SendT0Response(tInverse, cmdHeader, response, logger);
  else 
    err = SendT0Response(tInverse, cmdHeader, response, NULL);

  if(err)
  {
    FreeRAPDU(response);		
    return NULL;
  }

  return response;
}

/**
 * This function serializes (converts to a sequence of bytes) a RAPDU
 * structure.
 *
 * @param response response to be serialized
 * @param len length of the resulted byte stream
 * @return byte stream representing the serialized structure. This
 * method will allocate the necessary space and will write into len
 * the length of the stream. The method returns NULL if unsuccessful. 
 */
uint8_t* SerializeResponse(RAPDU *response, uint8_t *len)
{
  uint8_t *stream, i = 0;

  if(response == NULL || len == NULL || response->repStatus == NULL)
    return NULL;
  if(response->lenData > 0 && response->repData == NULL) return NULL;

  *len = 2 + response->lenData;
  stream = (uint8_t*)malloc((*len)*sizeof(uint8_t));
  if(stream == NULL)
  {
    *len = 0;
    return NULL;
  }

  stream[i++] = response->repStatus->sw1;
  stream[i++] = response->repStatus->sw2;	

  while(i < response->lenData)
  {
    stream[i] = response->repData[i];
    i++;
  }

  return stream;
}


/**
 * This method sends a command from the terminal to the ICC and also
 * returns to the terminal the answer from the ICC. Both the command
 * and the response are returned to the caller.
 *
 * @param tInverse different than 0 if inverse convention is to be used
 * with the terminal
 * @param cInverse different than 0 if inverse convention is to be used
 * with the ICC
 * @param tTC1 byte TC1 of ATR used with terminal
 * @param cTC1 byte TC1 of ATR received from ICC
 * @param log_dir specifies which part to log
 * @param logger a pointer to a log structure or NULL if no log is desired.
 * @return the command and response pair if successful. If this method
 * is not successful then it will return NULL
 * @sa ExchangeCompleteData
 */
CRP* ExchangeData(
    uint8_t tInverse,
    uint8_t cInverse,
    uint8_t tTC1,
    uint8_t cTC1,
    uint8_t log_dir,
    log_struct_t *logger)
{
  CRP* data;

  data = (CRP*)malloc(sizeof(CRP));
  if(data == NULL)
  {
    if(logger)
      LogByte1(logger, LOG_ERROR_MEMORY, 0);
    return NULL;
  }

  data->cmd = ForwardCommand(tInverse, cInverse, tTC1, cTC1, log_dir, logger);
  if(data->cmd == NULL)
  {
    free(data);
    return NULL;
  }

  data->response = ForwardResponse(
      tInverse, cInverse, data->cmd->cmdHeader, log_dir, logger);
  if(data->response == NULL)
  {
    FreeCAPDU(data->cmd);
    free(data);
    return NULL;
  }

  return data;
}

/**
 * This method exchange command-response pairs between terminal and ICC,
 * similar to ExchangeData. However this method forwards data repeatedly
 * until the response contains a success or error. The method returns
 * the initial command and the final response. Intermediate stages
 * are removed. If you need those stages use ExchangeData instead.
 *
 * @param tInverse different than 0 if inverse convention is to be used
 * with the terminal
 * @param cInverse different than 0 if inverse convention is to be used
 * with the ICC
 * @param tTC1 byte TC1 of ATR used with terminal
 * @param cTC1 byte TC1 of ATR received from ICC
 * @param log_dir specifies which part to log
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the command and response pair if successful. If this method
 * is not successful then it will return NULL. The caller is responsible
 * for the allocated memory of the CRP structure.
 * @sa ExchangeData
 */
CRP* ExchangeCompleteData(
    uint8_t tInverse,
    uint8_t cInverse,
    uint8_t tTC1,
    uint8_t cTC1,
    uint8_t log_dir,
    log_struct_t *logger)
{
  CRP *data, *tmp;
  uint8_t cont;

  data = (CRP*)malloc(sizeof(CRP));
  if(data == NULL)
  {
    if(logger)
      LogByte1(logger, LOG_ERROR_MEMORY, 0);
    return NULL;
  }
  data->cmd = NULL;
  data->response = NULL;

  // store command from first exchange
  tmp = ExchangeData(tInverse, cInverse, tTC1, cTC1, log_dir, logger);
  if(tmp == NULL)
  {
    FreeCRP(data);
    return NULL;
  }
  data->cmd = tmp->cmd;
  tmp->cmd = NULL;	

  cont = (tmp->response->repStatus->sw1 == 0x61 ||
      tmp->response->repStatus->sw1 == 0x6C); 
  if(cont) FreeCRP(tmp);

  while(cont)
  {
    tmp = ExchangeData(tInverse, cInverse, tTC1, cTC1, log_dir, logger);
    if(tmp == NULL)
    {
      FreeCRP(data);
      return NULL;
    }

    cont = (tmp->response->repStatus->sw1 == 0x61 ||
        tmp->response->repStatus->sw1 == 0x6C); 

    if(cont) FreeCRP(tmp);
  }

  data->response = tmp->response;
  tmp->response = NULL;
  FreeCRP(tmp);	

  return data;
}

/**
 * Create a ByteArray structure. This method just links
 * the data to the ByteArray structure which means that
 * eliberating the data (calling free) of the ByteArray
 * will have an effect on the data passed to this method.
 *
 * @param data the bytes to be linked to the structure
 * @param len the length of the byte array
 * @return the created ByteArray or NULL if no memory is
 * available
 */
ByteArray* MakeByteArray(uint8_t *data, uint8_t len)
{
  ByteArray *stream = (ByteArray*)malloc(sizeof(ByteArray));
  if(stream == NULL) return NULL;
  stream->bytes = data;
  stream->len = len;

  return stream;
}

/**
 * Create a ByteArray structure. This method creates
 * the structure based on the values directly passed
 * to this method.
 *
 * @param nargs the number of values passed to this method
 * @param ... the variable number of values given
 * @return the created ByteArray or NULL if no memory is
 * available
 */
ByteArray* MakeByteArrayV(uint8_t nargs, ...)
{
  ByteArray *ba;
  va_list ap;
  uint8_t i;

  ba = (ByteArray*)malloc(sizeof(ByteArray));
  if(ba == NULL) return NULL;
  ba->len = nargs;
  ba->bytes = (uint8_t*)malloc(ba->len * sizeof(uint8_t));
  if(ba->bytes == NULL)
  {
    free(ba);
    return NULL;
  }

  va_start(ap, nargs);
  // according to avr-libc documentation, variable arguments
  // are passed on the stack as ints
  for(i = 0; i < nargs; i++)
    ba->bytes[i] = (uint8_t)va_arg(ap, int); 
  va_end(ap);

  return ba;
}

/**
 * Create a ByteArray structure. This method copies
 * the data to the ByteArray structure which means that
 * the caller is responsible to eliberate the memory 
 *
 * @param data the bytes to be copied to the structure
 * @param len the length of the byte array
 * @return the created ByteArray or NULL if no memory is
 * available
 */
ByteArray* CopyByteArray(const uint8_t *data, uint8_t len)
{
  ByteArray *stream = (ByteArray*)malloc(sizeof(ByteArray));
  if(stream == NULL) return NULL;
  stream->bytes = NULL;
  stream->len = 0;

  if(data != NULL && len > 0)
  {
    stream->bytes = (uint8_t*)malloc(len * sizeof(uint8_t));
    if(stream->bytes == NULL)
    {
      free(stream);
      return NULL;
    }
    memcpy(stream->bytes, data, len);
    stream->len = len;
  }

  return stream;
}

/**
 * Eliberates the memory used by a ByteArray
 *
 * @param data the ByteArray structure to be erased
 */
void FreeByteArray(ByteArray* data)
{
  if(data == NULL) return;

  if(data->bytes != NULL)
  {
    free(data->bytes);
    data->bytes = NULL;
  }
  free(data);
}

/**
 * Eliberates the memory used by a CAPDU
 *
 * @param cmd the CAPDU structure to be erased
 */
void FreeCAPDU(CAPDU* cmd)
{
  if(cmd == NULL) return;

  if(cmd->cmdHeader != NULL)
  {
    free(cmd->cmdHeader);
    cmd->cmdHeader = NULL;		
  }

  if(cmd->cmdData != NULL)
  {		
    free(cmd->cmdData);
    cmd->cmdData = NULL;
  }
  free(cmd);
}

/**
 * This method makes a copy of a CAPDU, allocating
 * the necessary memory
 *
 * @param cmd Command APDU to be copied
 */
CAPDU* CopyCAPDU(CAPDU* cmd)
{
  CAPDU *command;

  if(cmd == NULL || cmd->cmdHeader == NULL) return NULL;

  command = (CAPDU*)malloc(sizeof(CAPDU));
  if(command == NULL) return NULL;
  command->cmdHeader = (EMVCommandHeader*)malloc(sizeof(EMVCommandHeader));
  if(command->cmdHeader == NULL)
  {
    free(command);
    return NULL;
  }
  memcpy(command->cmdHeader, cmd->cmdHeader, sizeof(EMVCommandHeader));
  if(cmd->cmdData != NULL && cmd->lenData != 0)
  {
    command->cmdData = (uint8_t*)malloc(cmd->lenData * sizeof(uint8_t));
    if(command->cmdData == NULL)
    {
      FreeCAPDU(command);
      return NULL;
    }
    memcpy(command->cmdData, cmd->cmdData, cmd->lenData);
    command->lenData = cmd->lenData;
  }
  else
  {
    command->cmdData = NULL;
    command->lenData = 0;
  }

  return command;
}

/**
 * Eliberates the memory used by a RAPDU
 *
 * @param response the RAPDU structure to be erased
 */
void FreeRAPDU(RAPDU* response)
{
  if(response == NULL) return;

  if(response->repStatus != NULL)
  {
    free(response->repStatus);
    response->repStatus = NULL;		
  }

  if(response->repData != NULL)
  {		
    free(response->repData);
    response->repData = NULL;
  }
  free(response);
}

/**
 * This method makes a copy of a RAPDU, allocating
 * the necessary memory
 *
 * @param resp Response APDU to be copied
 */
RAPDU* CopyRAPDU(RAPDU* resp)
{
  RAPDU *response;

  if(resp == NULL || resp->repStatus == NULL) return NULL;

  response = (RAPDU*)malloc(sizeof(RAPDU));
  if(response == NULL) return NULL;
  response->repStatus = (EMVStatus*)malloc(sizeof(EMVStatus));
  if(response->repStatus == NULL)
  {
    free(response);
    return NULL;
  }
  memcpy(response->repStatus, resp->repStatus, sizeof(EMVStatus));
  if(resp->repData != NULL && resp->lenData != 0)
  {
    response->repData = (uint8_t*)malloc(resp->lenData * sizeof(uint8_t));
    if(response->repData == NULL)
    {
      FreeRAPDU(response);
      return NULL;
    }
    memcpy(response->repData, resp->repData, resp->lenData);
    response->lenData = resp->lenData;
  }
  else
  {
    response->repData = NULL;
    response->lenData = 0;
  }

  return response;
}

/**
 * Eliberates the memory used by a CRP
 *
 * @param data the CRP structure to be erased
 */
void FreeCRP(CRP* data)
{
  if(data == NULL) return;

  if(data->cmd != NULL)
  {
    FreeCAPDU(data->cmd);
    data->cmd = NULL;		
  }

  if(data->response != NULL)
  {		
    FreeRAPDU(data->response);
    data->response = NULL;
  }
  free(data);
}



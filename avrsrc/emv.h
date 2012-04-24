/**
 * \file
 * \brief emv.h Header file
 *
 * Contains definitions of functions used to implement the EMV standard
 * including low-level ISO716-3 based as well as application-level protocol
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

#ifndef _EMV_H_
#define _EMV_H_

#include "scd_logger.h"

//------------------------------------------------------------------------
// Constant values used in EMV
#define EMV_MORE_TAGS_MASK 0x1F
#define EMV_EXTRA_LENGTH_BYTE 0x81

//------------------------------------------------------------------------
// EMV data structures

/**
 * Structure defining an array of bytes
 */
typedef struct {
   uint8_t len;
   uint8_t *bytes;
} ByteArray;

/**
 * Structure defining an EMV header command 
 */
typedef struct {
   uint8_t cla;
   uint8_t ins;
   uint8_t p1;
   uint8_t p2;
   uint8_t p3;          
} EMVCommandHeader;


/**
 * Structure defining a command APDU
 */
typedef struct {
	uint8_t lenData;
	EMVCommandHeader *cmdHeader;
	uint8_t *cmdData;
} CAPDU;

/**
 * Structure defining the status bytes
 */
typedef struct {
	uint8_t sw1;
	uint8_t sw2;
} EMVStatus;


/**
 * Structure defining the response APDU
 */
typedef struct {
	uint8_t lenData;
	EMVStatus *repStatus;
	uint8_t *repData;
} RAPDU;

/**
 * Structure defining a command-response pair
 */
typedef struct {
	CAPDU *cmd;
	RAPDU *response;	
} CRP;

/**
 * Enum defining the different types of commands supported
 */
typedef enum {
   CMD_SELECT = 0,
   CMD_GET_RESPONSE,
   CMD_READ_RECORD,
   CMD_GET_PROCESSING_OPTS,
   CMD_VERIFY,
   CMD_GENERATE_AC,
   CMD_GET_DATA,
   CMD_INTERNAL_AUTHENTICATE,
   CMD_PIN_CHANGE_UNBLOCK
}EMV_CMD;


/// Starts a cold or warm reset for ICC
uint8_t ResetICC(
        uint8_t warm,
        uint8_t *inverse_convention,
        uint8_t *proto,
        uint8_t *TC1,
        uint8_t *TA3,
        uint8_t *TB3,
        log_struct_t *logger);

//------------------------------------------------------------------------
// T=0 protocol functions

/// Sends default ATR for T=0 to terminal
void SendT0ATRTerminal(
    uint8_t inverse_convention,
    uint8_t TC1,
    log_struct_t *logger);

/// Receives the ATR from ICC after a successful activation
uint8_t GetATRICC(
    uint8_t *inverse_convention,
    uint8_t *proto,
    uint8_t *TS,
    uint8_t *T0,
    uint16_t *selection,
    uint8_t bytes[32],
    uint8_t *tck,
    log_struct_t *logger);

/// This function will return a command header structure
EMVCommandHeader* MakeCommandHeader(uint8_t cla, uint8_t ins, uint8_t p1, 
						uint8_t p2, uint8_t p3);

/// This function will return a command header structure
EMVCommandHeader* MakeCommandHeaderC(EMV_CMD command);

/// This function will return a command APDU (CAPDU) structure
CAPDU* MakeCommand(uint8_t cla, uint8_t ins, uint8_t p1, 
      uint8_t p2, uint8_t p3, const uint8_t cmdData[], uint8_t lenData);

/// This function will return a command APDU (CAPDU) structure
CAPDU* MakeCommandP(const EMVCommandHeader *cmdHdr, const uint8_t cmdData[],
      uint8_t lenData);

/// This function will return a command APDU (CAPDU) structure
CAPDU* MakeCommandC(EMV_CMD command, const uint8_t cmdData[],
      uint8_t lenData);

/// Initiates the communication with both the ICC and terminal for T=0
uint8_t InitSCDTransaction(uint8_t t_inverse, uint8_t t_TC1, 
	uint8_t *inverse_convention, uint8_t *proto, uint8_t *TC1, 
	uint8_t *TA3, uint8_t *TB3, log_struct_t *logger);

/// Returns the command case from the command header
uint8_t GetCommandCase(uint8_t cla, uint8_t ins);

/// Receive a command header from the terminal using protocol T=0
EMVCommandHeader* ReceiveT0CmdHeader(
        uint8_t inverse_convention,
        uint8_t TC1,
        log_struct_t *logger);

/// Receive a command data from the terminal using protocol T=0
uint8_t* ReceiveT0CmdData(
        uint8_t inverse_convention,
        uint8_t TC1,
        uint8_t len,
        log_struct_t *logger);

/// Receive a command (including data) from the terminal using protocol T=0
CAPDU* ReceiveT0Command(
        uint8_t inverse_convention,
        uint8_t TC1,
        log_struct_t *logger);

/// Send a command header to the ICC using protocol T=0
uint8_t SendT0CmdHeader(
        uint8_t inverse_convention,
        uint8_t TC1,
        EMVCommandHeader *cmdHeader,
        log_struct_t *logger);

/// Send a command data to the ICC using protocol T=0
uint8_t SendT0CmdData(
        uint8_t inverse_convention,
        uint8_t TC1,
        uint8_t *cmdData,
        uint8_t len,
        log_struct_t *logger);

/// Send a command (including data) to the ICC using protocol T=0
uint8_t SendT0Command(
        uint8_t inverse_convention,
        uint8_t TC1,
        CAPDU *cmd,
        log_struct_t *logger);

/// Forwards a command from the terminal to the ICC for T=0
CAPDU* ForwardCommand(
        uint8_t tInverse,
        uint8_t cInverse,
        uint8_t tTC1,
        uint8_t cTC1,
        log_struct_t *logger);

/// Serialize a CAPDU structure
uint8_t* SerializeCommand(CAPDU *cmd, uint8_t *len);

/// Receive response from ICC for T=0
RAPDU* ReceiveT0Response(
        uint8_t inverse_convention,
        EMVCommandHeader *cmdHeader,
        log_struct_t *logger);

/// Send a response to the terminal
uint8_t SendT0Response(
        uint8_t inverse_convention,
        EMVCommandHeader *cmdHeader,
        RAPDU *response,
        log_struct_t *logger);

/// Forwards a response from the ICC to the terminal
RAPDU* ForwardResponse(
        uint8_t tInverse,
        uint8_t cInverse,
        EMVCommandHeader *cmdHeader,
        log_struct_t *logger);

/// Serialize a RAPDU structure
uint8_t* SerializeResponse(RAPDU *response, uint8_t *len);

/// Makes a command-response exchange between terminal and ICC
CRP* ExchangeData(
        uint8_t tInverse,
        uint8_t cInverse,
        uint8_t tTC1,
        uint8_t cTC1,
        log_struct_t *logger);

/// Makes a complete command-response exchange between terminal and ICC
CRP* ExchangeCompleteData(
        uint8_t tInverse,
        uint8_t cInverse,
        uint8_t tTC1,
        uint8_t cTC1,
        log_struct_t *logger);

/// Encapsulates data in a ByteArray structure
ByteArray* MakeByteArray(uint8_t *data, uint8_t len);

/// Creates a ByteArray structure from values
ByteArray* MakeByteArrayV(uint8_t nargs, ...);

/// Copies data into a ByteArray structure
ByteArray* CopyByteArray(const uint8_t *data, uint8_t len);

/// Eliberates the memory used by a ByteArray
void FreeByteArray(ByteArray* data);

/// Eliberates the memory used by a CAPDU
void FreeCAPDU(CAPDU* cmd);

/// Makes a copy of a CAPDU
CAPDU* CopyCAPDU(CAPDU* cmd);

/// Eliberates the memory used by a RAPDU
void FreeRAPDU(RAPDU* response);

/// Makes a copy of a RAPDU
RAPDU* CopyRAPDU(RAPDU* resp);

/// Eliberates the memory used by a CRP
void FreeCRP(CRP* data);

#endif // _EMV_H_


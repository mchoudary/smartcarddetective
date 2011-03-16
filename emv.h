/** \file
 *	\brief emv.h Header file
 *
 *  Contains definitions of functions used to implement the EMV standard
 *  including low-level ISO716-3 based as well as application-level protocol
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
   CMD_INTERNAL_AUTHENTICATE
}EMV_CMD;


//------------------------------------------------------------------------
// T=0 protocol functions


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
	uint8_t *TA3, uint8_t *TB3);

/// Returns the command case from the command header
uint8_t GetCommandCase(uint8_t cla, uint8_t ins);

/// Receive a command header from the terminal using protocol T=0
EMVCommandHeader* ReceiveT0CmdHeader(uint8_t inverse_convention, uint8_t TC1);

/// Receive a command data from the terminal using protocol T=0
uint8_t* ReceiveT0CmdData(uint8_t inverse_convention, uint8_t TC1,
	uint8_t len);

/// Receive a command (including data) from the terminal using protocol T=0
CAPDU* ReceiveT0Command(uint8_t inverse_convention, uint8_t TC1);

/// Send a command header to the ICC using protocol T=0
uint8_t SendT0CmdHeader(uint8_t inverse_convention, uint8_t TC1,
	EMVCommandHeader *cmdHeader);

/// Send a command data to the ICC using protocol T=0
uint8_t SendT0CmdData(uint8_t inverse_convention, uint8_t TC1,
	uint8_t *cmdData, uint8_t len);

/// Send a command (including data) to the ICC using protocol T=0
uint8_t SendT0Command(uint8_t inverse_convention, uint8_t TC1, CAPDU *cmd);

/// Forwards a command from the terminal to the ICC for T=0
CAPDU* ForwardCommand(uint8_t tInverse, uint8_t cInverse, 
	uint8_t tTC1, uint8_t cTC1);

/// Serialize a CAPDU structure
uint8_t* SerializeCommand(CAPDU *cmd, uint8_t *len);

/// Receive response from ICC for T=0
RAPDU* ReceiveT0Response(uint8_t inverse_convention, 
	EMVCommandHeader *cmdHeader);

/// Send a response to the terminal
uint8_t SendT0Response(uint8_t inverse_convention, 
	EMVCommandHeader *cmdHeader, RAPDU *response);

/// Forwards a response from the ICC to the terminal
RAPDU* ForwardResponse(uint8_t tInverse, uint8_t cInverse,
	EMVCommandHeader *cmdHeader);

/// Serialize a RAPDU structure
uint8_t* SerializeResponse(RAPDU *response, uint8_t *len);

/// Makes a command-response exchange between terminal and ICC
CRP* ExchangeData(uint8_t tInverse, uint8_t cInverse, 
	uint8_t tTC1, uint8_t cTC1);

/// Makes a complete command-response exchange between terminal and ICC
CRP* ExchangeCompleteData(uint8_t tInverse, uint8_t cInverse, 
	uint8_t tTC1, uint8_t cTC1);

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



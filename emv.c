/** \file
 *	\brief emv.c source file
 *
 *  Contains the implementation of functions
 *  used to implement the EMV standard
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "emv.h"
#include "scd_hal.h"
#include "emv_values.h"
#include "scd_values.h"

#define DEBUG 0   // Set DEBUG to 1 to enable debug code


/* T=0 protocol functions */
/* All commands are received from the terminal and sent to the ICC */
/* All responses are received from the ICC and sent to the terminal */


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
 * This function is used as a starting point to start
 * the communication between the terminal and the SCD
 * and between the SCD and the ICC at the same time
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
 * @return 0 if success, non-zero otherwise.
 * @sa GetATRICC
 */
uint8_t InitSCDTransaction(uint8_t t_inverse, uint8_t t_TC1, 
	uint8_t *inverse_convention, uint8_t *proto, uint8_t *TC1, 
	uint8_t *TA3, uint8_t *TB3)
{
	uint8_t tmp;
	uint16_t tfreq, tdelay;
	int8_t tmpi;

	// start timer for terminal
	StartCounterTerminal();	
	
	// wait for terminal CLK
	while(ReadCounterTerminal() < 10); // this will be T0

	// get the terminal frequency
	tfreq = GetTerminalFreq();
	tdelay = 10 * tfreq;
	
	// activate ICC after (60000 - 10500*tfreq) terminal clocks
	// so that we rise ICC RST to high just after sending the 
	// TS byte to the terminal
	tmp = (uint8_t)((60000 - tdelay) / 372);
	LoopTerminalETU(tmp);
	if(ActivateICC(0)) return RET_ERROR;	// takes about 1 ETU
	
	// Send TS to terminal when RST line should be high	
	tmpi = (int8_t)((tdelay - 10000) / 372);
	if(tmpi < 0) tmpi = 0;
	LoopTerminalETU(tmpi);
	if(t_inverse)
		SendByteTerminalNoParity(0x3F, t_inverse);
	else
		SendByteTerminalNoParity(0x3B, t_inverse);

	// Set ICC RST line to high and receive ATR from ICC
	LoopTerminalETU(12);
	PORTD |= _BV(PD4);		
	
	// Wait for ATR from ICC for a maximum of 42000 clock cycles + 40 ms
	// this number is based on the assembler of this function
	if(WaitForICCData(50000))	
	{
		DeactivateICC();
		return RET_ERROR; 				// May be changed with a warm reset
	}
	
	if(GetATRICC(inverse_convention, proto, TC1, TA3, TB3))
	{
		DeactivateICC();
		return RET_ERROR;
	}

	// Send the rest of the ATR to the terminal
	SendByteTerminalNoParity(0x60, t_inverse);
	LoopTerminalETU(2);
	SendByteTerminalNoParity(0x00, t_inverse);
	LoopTerminalETU(2);
	SendByteTerminalNoParity(t_TC1, t_inverse);
	LoopTerminalETU(2);

	return 0;	
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
 * Receive a response from ICC for protocol T = 0
 *
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @return command header to be received if successful. This function
 * allocates memory (5 bytes) for the command header. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 */
EMVCommandHeader* ReceiveT0CmdHeader(uint8_t inverse_convention, uint8_t TC1)
{
	uint8_t tdelay;
	EMVCommandHeader *cmdHeader;

	cmdHeader = (EMVCommandHeader*)malloc(sizeof(EMVCommandHeader));
	if(cmdHeader == NULL) return NULL;

	tdelay = 1 + TC1;

	if(GetByteTerminalParity(inverse_convention, &(cmdHeader->cla))) 
	{
		free(cmdHeader);
		return NULL;
	}
	LoopTerminalETU(tdelay);	

	if(GetByteTerminalParity(inverse_convention, &(cmdHeader->ins)))
	{
		free(cmdHeader);
		return NULL;
	}
	LoopTerminalETU(tdelay);	

	if(GetByteTerminalParity(inverse_convention, &(cmdHeader->p1)))
	{
		free(cmdHeader);
		return NULL;
	}
	LoopTerminalETU(tdelay);	

	if(GetByteTerminalParity(inverse_convention, &(cmdHeader->p2)))
	{
		free(cmdHeader);
		return NULL;
	}
	LoopTerminalETU(tdelay);	

	if(GetByteTerminalParity(inverse_convention, &(cmdHeader->p3)))
	{
		free(cmdHeader);
		return NULL;
	}

	return cmdHeader;
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
 * @return command data to be received if successful. This function
 * allocates memory for the command data. The caller
 * is responsible for free-ing this memory after use. If the
 * method is not successful it returns NULL
 */
uint8_t* ReceiveT0CmdData(uint8_t inverse_convention, uint8_t TC1,
	uint8_t len)
{
	uint8_t tdelay, i;
	uint8_t *cmdData;

	cmdData = (uint8_t*)malloc(len*sizeof(uint8_t));
	if(cmdData == NULL) return NULL;

	tdelay = 1 + TC1;

	for(i = 0; i < len - 1; i++)
	{
		if(GetByteTerminalParity(inverse_convention, &(cmdData[i])))
		{
			free(cmdData);
			return NULL;	
		}
		LoopTerminalETU(tdelay);	
	}

	// Do not add a delay after the last byte
	if(GetByteTerminalParity(inverse_convention, &(cmdData[i])))
	{
		free(cmdData);
		return NULL;	
	}
	
	return cmdData;	
}

/**
 * Receive a command (including data) from terminal for protocol T = 0.
 * For command cases 3 and 4 a procedure byte is sent back to the
 * terminal to obtain the command data
 * 
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param TC1 the N parameter received in byte TC1 of ATR
 * @return command to be received. This function will allocate
 * the necessary memory for this structure, which should contain
 * valid data if the method returs success. If the method is not
 * successful then it will return NULL  
 */
CAPDU* ReceiveT0Command(uint8_t inverse_convention, uint8_t TC1)
{
	uint8_t tdelay, tmp;
	CAPDU *cmd;

	tdelay = 1 + TC1;

	cmd = (CAPDU*)malloc(sizeof(CAPDU));
	if(cmd == NULL) return NULL;
	cmd->cmdHeader = NULL;
	cmd->cmdData = NULL;
	cmd->lenData = 0;

	cmd->cmdHeader = ReceiveT0CmdHeader(inverse_convention, TC1);
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
	LoopTerminalETU(6);
	if(SendByteTerminalParity(cmd->cmdHeader->ins, inverse_convention))
	{
		free(cmd->cmdHeader);
		cmd->cmdHeader = NULL;
		free(cmd);		
		return NULL;
	}

	LoopTerminalETU(tdelay);	
	cmd->lenData = cmd->cmdHeader->p3;
	cmd->cmdData = ReceiveT0CmdData(inverse_convention, TC1, cmd->lenData);
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
 *
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0CmdHeader(uint8_t inverse_convention, uint8_t TC1,
	EMVCommandHeader *cmdHeader)
{
	uint8_t tdelay;

	if(cmdHeader == NULL) return RET_ERROR;

	tdelay = 1 + TC1;

	if(SendByteICCParity(cmdHeader->cla, inverse_convention)) return RET_ERROR;
	LoopICCETU(tdelay);	

	if(SendByteICCParity(cmdHeader->ins, inverse_convention)) return RET_ERROR;
	LoopICCETU(tdelay);	

	if(SendByteICCParity(cmdHeader->p1, inverse_convention)) return RET_ERROR;
	LoopICCETU(tdelay);	

	if(SendByteICCParity(cmdHeader->p2, inverse_convention)) return RET_ERROR;
	LoopICCETU(tdelay);	

	if(SendByteICCParity(cmdHeader->p3, inverse_convention)) return RET_ERROR;	

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
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0CmdData(uint8_t inverse_convention, uint8_t TC1,
	uint8_t *cmdData, uint8_t len)
{
	uint8_t tdelay, i;

	if(cmdData == NULL) return RET_ERROR;

	tdelay = 1 + TC1;

	for(i = 0; i < len - 1; i++)
	{
		if(SendByteICCParity(cmdData[i], inverse_convention))
			return RET_ERROR;	
		LoopICCETU(tdelay);	
	}

	// Do not add a delay after the last byte
	if(SendByteICCParity(cmdData[i], inverse_convention))
		return RET_ERROR;	
	
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
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0Command(uint8_t inverse_convention, uint8_t TC1, CAPDU *cmd)
{
   uint8_t tdelay, tmp, tmp2, i;	

   if(cmd == NULL) return RET_ERROR;
   tdelay = 1 + TC1;

   tmp = GetCommandCase(cmd->cmdHeader->cla, cmd->cmdHeader->ins);	
   if(tmp == 0)
      return RET_ERROR;
   if(SendT0CmdHeader(inverse_convention, TC1, cmd->cmdHeader))
      return RET_ERROR;

   // for case 1 and case 2 commands there is no command data to send
   if(tmp == 1 || tmp == 2)
      return 0;

   // for other cases (3, 4) get procedure byte and send command data
   LoopICCETU(6);

   // Get firs byte (can be INS, ~INS, 60, 61, 6C or other in case of error)
   if(GetByteICCParity(inverse_convention, &tmp)) return RET_ERROR;

   while(tmp == SW1_MORE_TIME)
   {
      LoopICCETU(1);
      if(GetByteICCParity(inverse_convention, &tmp)) return RET_ERROR;
   }

   // if we don't get INS or ~INS then
   // get another byte and then exit, operation unexpected
   if((tmp != cmd->cmdHeader->ins) && (tmp != ~(cmd->cmdHeader->ins)))
   {
      //LoopICCETU(1);
      GetByteICCParity(inverse_convention, &tmp2);
      return RET_ERR_CHECK; 
   }

   // Wait for card to be ready before sending any bytes
   LoopICCETU(6);

   i = 0;
   // Send first byte if sending byte by byte
   if(tmp != cmd->cmdHeader->ins)
   {
      if(SendByteICCParity(cmd->cmdData[i++], inverse_convention))
         return RET_ERROR;
      if(i < cmd->lenData)
         LoopICCETU(6);
   }

   // send byte after byte in case we receive !INS instead of INS
   while(tmp != cmd->cmdHeader->ins && i < cmd->lenData)
   {
      if(GetByteICCParity(inverse_convention, &tmp)) return RET_ERROR;
      LoopICCETU(6);

      if(tmp != cmd->cmdHeader->ins)
      {
         if(SendByteICCParity(cmd->cmdData[i++], inverse_convention))
	    return RET_ERROR;
         if(i < cmd->lenData)
	    LoopICCETU(6);
      }
   }
			
   // send remaining of bytes, if any
   for(; i < cmd->lenData - 1; i++)
   {
      if(SendByteICCParity(cmd->cmdData[i], inverse_convention))
	return RET_ERROR;
      LoopICCETU(tdelay);
   }
   if(i == cmd->lenData - 1)
   {
      if(SendByteICCParity(cmd->cmdData[i], inverse_convention))
         return RET_ERROR;
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
 * @return the command that has been forwarded if successful. If this
 * method is not successful then it will return NULL
 */
CAPDU* ForwardCommand(uint8_t tInverse, uint8_t cInverse, 
	uint8_t tTC1, uint8_t cTC1)
{
	CAPDU* cmd;

	cmd = ReceiveT0Command(tInverse, tTC1);
	if(cmd == NULL) return NULL;

	if(SendT0Command(cInverse, cTC1, cmd))
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
uint8_t* SerializeCommand(CAPDU *cmd, uint8_t *len)
{
	uint8_t *stream, i;
	
	if(cmd == NULL || len == NULL || cmd->cmdHeader == NULL) return NULL;
	if(cmd->lenData > 0 && cmd->cmdData == NULL) return NULL;
	
	*len = 5 + cmd->lenData;
	stream = (uint8_t*)malloc((*len)*sizeof(uint8_t));
	if(stream == NULL)
	{
		*len = 0;
		return NULL;
	}

	stream[0] = cmd->cmdHeader->cla;
	stream[1] = cmd->cmdHeader->ins;
	stream[2] = cmd->cmdHeader->p1;
	stream[3] = cmd->cmdHeader->p2;
	stream[4] = cmd->cmdHeader->p3;

	for(i = 0; i < cmd->lenData; i++)
		stream[i+5] = cmd->cmdData[i];

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
 * @return response APDU if the method is successful. In the case this
 * method is unsuccessful (unrelated to SW1, SW2) then it will return NULL 
 */
RAPDU* ReceiveT0Response(uint8_t inverse_convention, 
	EMVCommandHeader *cmdHeader)
{
	uint8_t tmp, i;
	RAPDU* rapdu;

	if(cmdHeader == NULL) return NULL;

	rapdu = (RAPDU*)malloc(sizeof(RAPDU));
	if(rapdu == NULL) return 0;
	rapdu->repStatus = NULL;
	rapdu->repData = NULL;
	rapdu->lenData = 0;
	tmp = GetCommandCase(cmdHeader->cla, cmdHeader->ins);		
	if(tmp == 0)
	{
		free(rapdu);
		return NULL;
	}

	
	// for case 1 and case 3 there is no data expected, just status
	if(tmp == 1 || tmp == 3)
	{
		rapdu->repStatus = (EMVStatus*)malloc(sizeof(EMVStatus));
		if(rapdu->repStatus == NULL)
		{
			free(rapdu);
			return NULL;
		}

		if(GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw1)))
		{
			free(rapdu->repStatus);
			rapdu->repStatus = NULL;
			free(rapdu);
			return NULL;
		}

		if(rapdu->repStatus->sw1 == 0x60)
		{
			// requested more time, recall
			free(rapdu->repStatus);
			rapdu->repStatus = NULL;
			free(rapdu);
			
			return ReceiveT0Response(inverse_convention, cmdHeader);

		}

		if(GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw2)))
		{
			free(rapdu->repStatus);
			rapdu->repStatus = NULL;
			free(rapdu);
			return NULL;
		}

		return rapdu;
	}

	// for case 2 and 4, we might get data based on first byte of response	
	if(GetByteICCParity(inverse_convention, &tmp))
	{
		free(rapdu);
		return NULL;
	}

	if(tmp == 0x60)
	{
		// requested more time, recall
		free(rapdu);
		
		return ReceiveT0Response(inverse_convention, cmdHeader);
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
			free(rapdu);
			return NULL;
		}

		for(i = 0; i < rapdu->lenData; i++)
		{
			if(GetByteICCParity(inverse_convention, &(rapdu->repData[i])))
			{
				free(rapdu->repData);
				rapdu->repData = NULL;
				free(rapdu);
				return NULL;
			}
		}		

		rapdu->repStatus = (EMVStatus*)malloc(sizeof(EMVStatus));
		if(rapdu->repStatus == NULL)
		{
			free(rapdu->repData);
			rapdu->repData = NULL;
			free(rapdu);
			return NULL;
		}

		if(GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw1)))
		{
			free(rapdu->repData);
			rapdu->repData = NULL;
			free(rapdu->repStatus);
			rapdu->repStatus = NULL;
			free(rapdu);
			return NULL;
		}

		if(GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw2)))
		{
			free(rapdu->repData);
			rapdu->repData = NULL;
			free(rapdu->repStatus);
			rapdu->repStatus = NULL;
			free(rapdu);
			return NULL;
		}

	}	
	else	// get second byte of response (no data)
	{
		rapdu->repStatus = (EMVStatus*)malloc(sizeof(EMVStatus));
		if(rapdu->repStatus == NULL)
		{			
			free(rapdu);
			return NULL;
		}

		rapdu->repStatus->sw1 = tmp;
		if(GetByteICCParity(inverse_convention, &(rapdu->repStatus->sw2)))
		{
			free(rapdu->repStatus);
			rapdu->repStatus = NULL;
			free(rapdu);
			return NULL;
		}
	}

	return rapdu;
}


/**
 * Send a response (including data) to the terminal for protocol T = 0.
 * Data (if available) is sent with a procedure byte prepended
 * 
 * @param inverse_convention different than 0 if inverse
 * convention is to be used
 * @param cmdHeader header of command for which response is being sent
 * @param response RAPDU containing the response to be sent
 * @return 0 if success, non-zero otherwise
 */
uint8_t SendT0Response(uint8_t inverse_convention, 
	EMVCommandHeader *cmdHeader, RAPDU *response)
{
	uint8_t i;	

	if(cmdHeader == NULL || response == NULL) return RET_ERROR;	

	if(response->lenData > 0 && response->repData != NULL)
	{
		if(SendByteTerminalParity(cmdHeader->ins, inverse_convention))
			return RET_ERROR;
		LoopTerminalETU(2);

		for(i = 0; i < response->lenData; i++)
		{			
			if(SendByteTerminalParity(response->repData[i],
				inverse_convention))
				return RET_ERROR;
			LoopTerminalETU(2);
		}
	}

	if(response->repStatus == NULL) return RET_ERROR;

	if(SendByteTerminalParity(response->repStatus->sw1, inverse_convention))
		return RET_ERROR;
	LoopTerminalETU(2);
	if(SendByteTerminalParity(response->repStatus->sw2, inverse_convention))
		return RET_ERROR;	

	return 0;
}

/**
 * Receive a response from the terminal and then send it to the terminal
 *
 * @param tInverse different than 0 if inverse convention is to be used
 * with the terminal
 * @param cInverse different than 0 if inverse convention is to be used
 * with the ICC
 * @param cmdHeader the header of the command for which response is expected
 * @return the response that has been forwarded if successful. If this
 * method is not successful then it will return NULL
 */
RAPDU* ForwardResponse(uint8_t tInverse, uint8_t cInverse,
	EMVCommandHeader *cmdHeader)
{
	RAPDU* response;

	if(cmdHeader == NULL) return NULL;

	response = ReceiveT0Response(cInverse, cmdHeader);	
	if(response == NULL) return NULL;

	if(SendT0Response(tInverse, cmdHeader, response))
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
	uint8_t *stream, i;
	
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

	stream[0] = response->repStatus->sw1;
	stream[1] = response->repStatus->sw2;	

	for(i = 0; i < response->lenData; i++)
		stream[i+2] = response->repData[i];

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
 * @return the command and response pair if successful. If this method
 * is not successful then it will return NULL
 * @sa ExchangeCompleteData
 */
CRP* ExchangeData(uint8_t tInverse, uint8_t cInverse, 
	uint8_t tTC1, uint8_t cTC1)
{
	CRP* data;

	data = (CRP*)malloc(sizeof(CRP));
	if(data == NULL) return NULL;

	data->cmd = ForwardCommand(tInverse, cInverse, tTC1, cTC1);
	if(data->cmd == NULL)
	{
		free(data);
		return NULL;
	}

	data->response = ForwardResponse (tInverse, cInverse, data->cmd->cmdHeader);
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
 * @return the command and response pair if successful. If this method
 * is not successful then it will return NULL
 * @sa ExchangeData
 */
CRP* ExchangeCompleteData(uint8_t tInverse, uint8_t cInverse, 
	uint8_t tTC1, uint8_t cTC1)
{
	CRP *data, *tmp;
	uint8_t cont;

	data = (CRP*)malloc(sizeof(CRP));
	if(data == NULL) return NULL;
	data->cmd = NULL;
	data->response = NULL;

	// store command from first exchange
	tmp = ExchangeData(tInverse, cInverse, tTC1, cTC1);
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
		tmp = ExchangeData(tInverse, cInverse, tTC1, cTC1);
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



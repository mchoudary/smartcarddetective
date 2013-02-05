/**
 * \file
 * \brief apps.c source file
 *
 * This file implements the applications available on the SCD
 *
 * These functions are not microcontroller dependent but they are intended
 * for the AVR 8-bit architecture
 *
 * Copyright (C) 2013 Omar Choudary (omar.choudary@cl.cam.ac.uk)
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

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

#include "apps.h"
#include "emv.h"
#include "emv_values.h"
#include "scd.h"
#include "scd_hal.h"
#include "scd_io.h"
#include "scd_logger.h"
#include "scd_values.h"
#include "serial.h"
#include "terminal.h"
#include "utils.h"
#include "VirtualSerial.h"

/// Set this to 1 to enable LCD functionality
#define LCD_ENABLED 1			

/// Set this to 1 to enable debug mode
#define DEBUG 0

/// size of SCD's EEPROM
#define EEPROM_SIZE 4096

/// address of bootloader section
#define BOOTLOADER_START_ADDRESS 0xF000

/// wait time for terminal reset or I/O lines to become low
#define TERMINAL_RESET_IO_WAIT (ETU_TERMINAL * 42000)

/* Static variables */
#if LCD_ENABLED
static char* strDone = "All     Done";
static char* strLog = "Writing Log";
static char* strScroll = "BC to   scroll";
static char* strDecide = "BA = yesBD = no";
static char* strInsertCard = "Insert  card";
static char* strCardInserted = "Card    inserted";
static char* strTerminalReset = "Terminalreset";
static char* strPINOK = "PIN OK";
static char* strPINBAD = "PIN BAD";
#endif

/**
 * Virtual Serial Port application
 *
 * @param logger the log structure.
 */
uint8_t VirtualSerial(log_struct_t *logger)
{
    char *buf;
    char *response = NULL;

    if(GetLCDState() == 0)
        InitLCD();
    fprintf(stderr, "\n");

    fprintf(stderr, "Set up  VS\n");
    _delay_ms(500);
    power_usb_enable();
    SetupHardware();
    sei();

    // Signal that VS is ready
    Led1On();
    Led2On();
    Led3On();
    Led4On();
    fprintf(stderr, "VS Ready\n");
    _delay_ms(100);

    for (;;)
    {
        buf = GetHostData(255);
        if(buf == NULL)
        {
            _delay_ms(100);
            continue;
        }

        response = (char*)ProcessSerialData(buf, logger);
        free(buf);

        if(response != NULL)
        {
            SendHostData(response);
            free(response);
            response = NULL;
        }

        // Need to switch back leds as some apps switch them off
        Led1On();
        Led2On();
        Led3On();
        Led4On();
        fprintf(stderr, "VS Ready\n");
    }
}

/**
 * Serial Port interface application
 *
 * @param baudUBRR the baud UBRR parameter as given in table 18-12 of
 * the datasheet, page 203. The formula is: baud = FCLK / (16 * (baudUBRR + 1)).
 * So for FCLK = 16 MHz and desired BAUD = 9600 bps => baudUBRR = 103.
 * @param logger the log structure or NULL if no log is desired
 */
uint8_t SerialInterface(uint16_t baudUBRR, log_struct_t *logger)
{
    char *buf;
    char *response = NULL;

    InitLCD();
    fprintf(stderr, "\n");

    fprintf(stderr, "Set up  Serial\n");
    _delay_ms(500);
    power_usart1_enable();
    _delay_ms(500);
    InitUSART(baudUBRR);

    fprintf(stderr, "Serial  Ready\n");
    _delay_ms(500);

    for (;;)
    {
        // Not working yet => resolder RX/TX and then try to enable/disable CTS/RTS signals
        fprintf(stderr, "Before  GetLine\n");
        _delay_ms(500);
        buf = GetLineUSART();
        if(buf == NULL)
        {
            _delay_ms(100);
            continue;
        }

        fprintf(stderr, "Got:%s\n", buf);
        _delay_ms(500);

        response = (char*)ProcessSerialData(buf, logger);
        free(buf);

        if(response != NULL)
        {
            SendLineUSART(response);
            free(response);
        }
    }
}

/** 
 * This function erases the entire contents of the EEPROM.
 * Interrupts are disabled during operation.
 */
void EraseEEPROM()
{
	uint8_t sreg, k;
    uint16_t eeaddr = 0;
	uint8_t eeclear[32] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	                       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	                       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	                       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	sreg = SREG;
	cli();

	// Write page by page using the block writing method
    for(k = 0; k < EEPROM_SIZE / 32; k++)
    {
        eeprom_update_block(eeclear, (void*)eeaddr, 32);
        eeaddr = eeaddr + 32;
    }

	SREG = sreg;
}

/** 
 * Reset the data in EEPROM to default values. This method
 * will first erase the EEPROM contents and then set the
 * default values.
 * Interrupts are disabled during operation.
 * 
 * @sa EraseEEPROM
 */
void ResetEEPROM()
{
    EraseEEPROM();

    eeprom_write_byte((uint8_t*)EEPROM_WARM_RESET, 0);
    eeprom_write_dword((uint32_t*)EEPROM_TIMER_T2, 0);
    eeprom_write_dword((uint32_t*)EEPROM_TEMP_1, 0);
    eeprom_write_dword((uint32_t*)EEPROM_TEMP_2, 0);
    eeprom_write_byte((uint8_t*)EEPROM_APPLICATION, 0);
    eeprom_write_byte((uint8_t*)EEPROM_COUNTER, 0);
    eeprom_write_byte(
            (uint8_t*)EEPROM_TLOG_POINTER_HI, (EEPROM_TLOG_DATA >> 8) & 0xFF);
    eeprom_write_byte(
            (uint8_t*)EEPROM_TLOG_POINTER_LO, EEPROM_TLOG_DATA & 0xFF);
}

/**
 * Jump into the Bootloader application, typically the DFU bootloader for
 * USB programming.
 *
 * Code taken from:
 * http://www.fourwalledcubicle.com/files/LUFA/Doc/100807/html/_page__software_bootloader_start.html
 */
void RunBootloader()
{
    bootkey = MAGIC_BOOT_KEY;
    EnableWDT(100);
    while(1);
}


/**
 * Test function to make multiple DDA transactions
 */
uint8_t TestDDA(uint8_t convention, uint8_t TC1)
{
   uint8_t status = 0;
   RAPDU *response = NULL;
   FCITemplate *fci = NULL;
   APPINFO *appInfo = NULL;
   RECORD *tData = NULL;
   ByteArray *offlineAuthData = NULL;
   ByteArray *ddata = NULL;

   EnableWDT(4000);

   // Select application
   //fci = ApplicationSelection(convention, TC1); // to use PSE first
   fci = SelectFromAID(convention, TC1, NULL, 0);
   if(fci == NULL)
   {
      fprintf(stderr, "Error\n");
      status = 1;
      goto endtransaction;
   }
   ResetWDT();

   // Start transaction by issuing Get Processing Opts command
   appInfo = InitializeTransaction(convention, TC1, fci, 0);
   if(appInfo == NULL)
   {
      fprintf(stderr, "Error\n");
      status = 1;
      goto endfci;
   }
   ResetWDT();

   // Get transaction data
   offlineAuthData = (ByteArray*)malloc(sizeof(ByteArray));
   tData = GetTransactionData(convention, TC1, appInfo, offlineAuthData, 0);
   if(tData == NULL)
   {
      fprintf(stderr, "Error\n");
      status = 1;
      goto endappinfo;
   }
   ResetWDT();

   // Send internal authenticate command
   ddata = MakeByteArrayV(4,
         0x05, 0x06, 0x07, 0x08
         );
   response = SignDynamicData(convention, TC1, ddata, 0);
   if(response == NULL)
   {
      fprintf(stderr, "Error\n");
      status = 1;
      goto endtdata;
   }

   FreeRAPDU(response);
   FreeByteArray(ddata);
endtdata:
   FreeRECORD(tData);
endappinfo:
   FreeAPPINFO(appInfo);
   if(offlineAuthData != NULL)
      FreeByteArray(offlineAuthData);
endfci:
   FreeFCITemplate(fci);
endtransaction:
   DeactivateICC();
   asm volatile("nop\n\t"::);
   _delay_ms(50);

   DisableWDT();
   return status;
}


/**
 * This method implements a terminal application with the basic steps
 * of an EMV transaction. This includes selection by AID, DDA signature,
 * PIN verification and transaction data authorization (Generate AC).
 *
 * @param logger the log structure or NULL if log is not desired
 * @return 0 if successful, non-zero otherwise
 */
uint8_t Terminal(log_struct_t *logger)
{
   uint8_t convention, proto, TC1, TA3, TB3;
   uint8_t error;
   uint8_t tmp;
   RAPDU *response = NULL;
   FCITemplate *fci = NULL;
   APPINFO *appInfo = NULL;
   RECORD *tData = NULL;
   ByteArray *offlineAuthData = NULL;
   ByteArray *pinTryCounter = NULL;
   ByteArray *pin = NULL;
   ByteArray *ddata = NULL;
   ByteArray *bdata = NULL;
   ByteArray *atcData = NULL;
   ByteArray *lastAtcData = NULL;
   GENERATE_AC_PARAMS acParams;
   const TLV *cdol = NULL;

    // Visual signal for this app
    Led1Off();
    Led2On();
    Led3Off();
    Led4Off();

   if(!lcdAvailable) 
   {
       Led2Off();
       _delay_ms(500);
       Led2On();
       _delay_ms(500);
       Led2Off();
       return RET_ERROR;
   }

   if(GetLCDState() == 0)
       InitLCD();
   fprintf(stderr, "\n");
   fprintf(stderr, "Terminal\n");
   _delay_ms(500);

   DisableWDT();
   DisableTerminalResetInterrupt();
   DisableICCInsertInterrupt();

   // Expect the card to be inserted first and then start
   fprintf(stderr, "%s\n", strInsertCard);
   while(IsICCInserted() == 0);
   fprintf(stderr, "%s\n", strCardInserted);
   if(logger)
       LogByte1(logger, LOG_ICC_INSERTED, 0);

   EnableWDT(4000);

   // Initialize card
   error = ResetICC(0, &convention, &proto, &TC1, &TA3, &TB3, logger);
   if(error)
   {
       fprintf(stderr, "Error:  %d\n", error);
       _delay_ms(1000);
       goto endtransaction;
   }
   if(proto != 0)
   {
       error = RET_ICC_BAD_PROTO;
       fprintf(stderr, "Error:  %d\n", error);
       _delay_ms(1000);
       goto endtransaction;
   }
   ResetWDT();

   // Select application. You can use one of the following options:
   //
   // Option 1: use the PSE first. Use the line below:
   // fci = ApplicationSelection(convention, TC1, logger);
   //
   // Option 2: use a specific Application ID (AID). Use the lines below:
   // bdata = MakeByteArrayV(
   //        7, 0xA0, 0, 0, 0, 0x29, 0x10, 0x10);
   // fci = SelectFromAID(convention, TC1, bdata, logger);
   //
   // Option 3: use a predefined list (see terminal.c) of AIDs. Use this line:
   // fci = SelectFromAID(convention, TC1, NULL, logger);
   fci = SelectFromAID(convention, TC1, NULL, logger);
   if(fci == NULL)
   {
       error = RET_EMV_SELECT;
       fprintf(stderr, "Error:  %d\n", error);
       _delay_ms(1000);
       goto endtransaction;
   }
   ResetWDT();

   // Start transaction by issuing Get Processing Opts command
   appInfo = InitializeTransaction(convention, TC1, fci, logger);
   if(appInfo == NULL)
   {
       error = RET_EMV_INIT_TRANSACTION;
       fprintf(stderr, "Error:  %d\n", error);
       _delay_ms(1000);
       goto endfci;
   }
   ResetWDT();

   // Get transaction data
   // Be careful about the amount of memory used. If you are using the logger
   // then bear in mind how much memory you are using and how much is left.
   // If your logger uses around 3K (see scd_logger.h) then you probably won't
   // have enough memory to actually store more transaction data such as the
   // offlineAuthData. Pass NULL to the offlineAuthData, to the logger or reduce
   // the logger size (see scd_logger.h).
   // offlineAuthData = (ByteArray*)malloc(sizeof(ByteArray));
   offlineAuthData = NULL;
   tData = GetTransactionData(convention, TC1, appInfo, offlineAuthData, logger);
   if(tData == NULL)
   {
       error = RET_EMV_READ_DATA;
       fprintf(stderr, "Error:  %d\n", error);
       _delay_ms(1000);
       goto endappinfo;
   }
   ResetWDT();

   // Get ATC
   atcData = GetDataObject(convention, TC1, PDO_ATC, logger);
   ResetWDT();

   // Get Last online ATC
   lastAtcData = GetDataObject(convention, TC1, PDO_LAST_ATC, logger);
   ResetWDT();

   if(atcData)
   {
       fprintf(stderr, "atc: %d\n", (atcData->bytes[0] << 8) | atcData->bytes[1]);
       _delay_ms(1000);
   }

   if(lastAtcData)
   {
       fprintf(stderr, "last onlatc: %d\n", (lastAtcData->bytes[0] << 8) | lastAtcData->bytes[1]);
       _delay_ms(1000);
   }

   // Send internal authenticate command (only for DDA cards supporting as per AIP)
   if((appInfo->aip[0] & 0x20) != 0)
   {
      ddata = MakeByteArrayV(4,
            0x01, 0x02, 0x03, 0x04
            );
      response = SignDynamicData(convention, TC1, ddata, logger);
      if(response == NULL)
      {
         error = RET_EMV_DDA;
         fprintf(stderr, "Error:  %d\n", error);
         goto endatcdata;
      }
      ResetWDT();
   }

   // Get PIN try counter
   pinTryCounter = GetDataObject(convention, TC1, PDO_PIN_TRY_COUNTER, logger);
   if(pinTryCounter == NULL)
   {
       error = RET_EMV_GET_DATA;
       fprintf(stderr, "Error:  %d\n", error);
       goto endatcdata;
   }
   if(pinTryCounter->bytes[0] == 0)
   {
       error = RET_EMV_PIN_TRY_EXCEEDED;
       fprintf(stderr, "Error:  %d\n", error);
       goto endpintry;
   }
   ResetWDT();

   fprintf(stderr, "pin try:%d\n", pinTryCounter->bytes[0]);
   _delay_ms(1000);
   ResetWDT();

   /*
   // Send PIN verification
   // Below just an example for PIN=1234. Implement some pin entry mechanism
   // with buttons if needed
   pin = MakeByteArrayV(8, 0x24, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
   if(pin == NULL)
   {
      error = RET_ERROR;
      fprintf(stderr, "Error:  %d\n", error);
      goto endpintry;
   }
   DisableWDT();
   tmp = VerifyPlaintextPIN(convention, TC1, pin, logger);
   if(tmp == 0)
      fprintf(stderr, "%s\n", strPINOK);
   else
   {
      fprintf(stderr, "%s\n", strPINBAD);
      goto endpin;
   }
   EnableWDT(4000);
   */

   // Send the first GENERATE_AC command (amount = 0)
   acParams.tvr[0] = 0x80;
   acParams.terminalCountryCode[0] = 0x08;
   acParams.terminalCountryCode[1] = 0x26;
   acParams.terminalCurrencyCode[0] = 0x08;
   acParams.terminalCurrencyCode[1] = 0x26;
   acParams.transactionDate[0] = 0x01;
   acParams.transactionDate[1] = 0x01;
   acParams.transactionDate[2] = 0x01;
   cdol = GetTLVFromRECORD(tData, 0x8C, 0);
   if(cdol == NULL)
   {
      error = RET_ERROR;
      fprintf(stderr, "Error:  %d\n", error);
      _delay_ms(500);
      goto endpin;
   }

   if(response != NULL) FreeRAPDU(response);
   response = SendGenerateAC(
           convention, TC1, AC_REQ_ARQC, cdol, &acParams, logger);
   if(response == NULL)
   {
       error = RET_EMV_GENERATE_AC;
       fprintf(stderr, "Error:  %d\n", error);
       _delay_ms(500);
       goto endpin;
   }

   fprintf(stderr, "%s\n", strDone);
   error = 0;
   FreeRAPDU(response);
endpin:
   //FreeByteArray(pin);
endpintry:
   FreeByteArray(pinTryCounter);
endatcdata:
   FreeByteArray(lastAtcData);
   FreeByteArray(atcData);
endtdata:
   FreeRECORD(tData);
endappinfo:
   FreeAPPINFO(appInfo);
   if(offlineAuthData != NULL)
      FreeByteArray(offlineAuthData);
endfci:
   FreeFCITemplate(fci);
endtransaction:
   DisableWDT();
   DeactivateICC();

   if(logger)
   {
       LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
       fprintf(stderr, "%s\n", strLog);
       WriteLogEEPROM(logger);
       ResetLogger(logger);
   }

   return error;
}


/**
 * This function initiates the communication between ICC and
 * terminal and then forwards the commands and responses
 * from terminal to ICC but blocks the Generate AC request
 * until the user accepts or denies the transaction.
 *
 * Transaction amount is displayed on LCD and thus the LCD is required
 * for this application. 
 *
 * If the terminal enables the reset line (either by warm reset or
 * by interrupting the transaction the INT0 interrupt routing will
 * be called
 *
 * @param logger the log structure or NULL if log is not desired
 * @return 0 if successful, non-zero otherwise
 */
uint8_t FilterGenerateAC(log_struct_t *logger)
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
	uint8_t tmp, error;
	uint8_t posCDOL1 = 0;
	uint8_t amount[12];	
	uint8_t gotGAC = 0;
	CAPDU *cmd;
	RAPDU *response;	
	RECORD *record;
	CRP *crp;

    if(!lcdAvailable) 
    {
       Led2On();
       _delay_ms(1000);
       Led2Off();
       return RET_ERROR;
    }

    InitLCD();
    fprintf(stderr, "\n");
    fprintf(stderr, "Filter  Gen AC\n");
    _delay_ms(1000);

    DisableWDT();
    DisableTerminalResetInterrupt();
    DisableICCInsertInterrupt();

    // Expect the card to be inserted first and then wait a for terminal reset
    fprintf(stderr, "%s\n", strInsertCard);
    while(IsICCInserted() == 0);
    fprintf(stderr, "%s\n", strCardInserted);
    if(logger)
       LogByte1(logger, LOG_ICC_INSERTED, 0);
    while(GetTerminalResetLine() != 0);
    fprintf(stderr, "%s\n", strTerminalReset);
    if(logger)
       LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);

    EnableWDT(4000);

	error = InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
            &cProto, &cTC1, &cTA3, &cTB3, logger);
	if(error)
		goto enderror;

	// forward commands until Read Record is received
	while(posCDOL1 == 0)
	{
		ResetWDT();
		cmd = ReceiveT0Command(t_inverse, t_TC1, logger);
		if(cmd == NULL)
        {
            error = RET_TERMINAL_GET_CMD;
            goto enderror;
        }

		if((cmd->cmdHeader->cla & 0xF0) == 0 && 
			cmd->cmdHeader->ins == 0xB2)
		{						
			// read record command
			if(SendT0Command(cInverse, cTC1, cmd, logger))
			{	
                error = RET_ICC_SEND_CMD;
				FreeCAPDU(cmd);
				goto enderror;
			}

			response = ReceiveT0Response(cInverse, cmd->cmdHeader, logger);
			if(response == NULL)
			{	
                error = RET_ICC_GET_RESPONSE;
				FreeCAPDU(cmd);
				goto enderror;
			}

			if(response->repData != NULL)
			{
				record = ParseRECORD(response->repData, response->lenData);
				if(record == NULL)
				{
                    error = RET_ERROR;
					FreeCAPDU(cmd);
					FreeRAPDU(response);
					goto enderror;
				}

				posCDOL1 = AmountPositionInCDOLRecord(record);
				FreeRECORD(record);
				record = NULL;
			}

			if(SendT0Response(t_inverse, cmd->cmdHeader, response, logger))
			{
                error = RET_TERMINAL_SEND_RESPONSE;
				FreeCAPDU(cmd);
				FreeRAPDU(response);
				goto enderror;
			}
		}
		else
		{
			// another command, just forward it
			if(SendT0Command(cInverse, cTC1, cmd, logger))
			{	
                error = RET_ICC_SEND_CMD;
				FreeCAPDU(cmd);
				goto enderror;
			}

			response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader, logger);
			if(response == NULL)
			{
                error = RET_ERROR;
				FreeCAPDU(cmd);
			    goto enderror;
			}

			// not interested in data
			FreeCAPDU(cmd);
			FreeRAPDU(response);
		}	
	} //end while(posCDOL1 == 0)

	// Disable WDT as VERIFY command will delay
	DisableWDT();

	// forward commands but block first Generate AC command
	while(gotGAC == 0)
	{
		cmd = ReceiveT0Command(t_inverse, t_TC1, logger);
		if(cmd == NULL)
        {
            error = RET_TERMINAL_GET_CMD;
            goto enderror;
        }

		if((cmd->cmdHeader->cla & 0xF0) == 0x80 && 
			cmd->cmdHeader->ins == 0xAE)
		{
			// Generate AC command received
			if(cmd->cmdData == NULL)
			{
                error = RET_ERROR;
				FreeCAPDU(cmd);
				goto enderror;
			}

			gotGAC = 1;

			posCDOL1--; // the value in posCDOL1 started at 1
			amount[0] = (cmd->cmdData[posCDOL1] & 0xF0) >> 4;
			amount[1] = cmd->cmdData[posCDOL1] & 0x0F;
			amount[2] = (cmd->cmdData[posCDOL1 + 1] & 0xF0) >> 4;
			amount[3] = cmd->cmdData[posCDOL1 + 1] & 0x0F;
			amount[4] = (cmd->cmdData[posCDOL1 + 2] & 0xF0) >> 4;
			amount[5] = cmd->cmdData[posCDOL1 + 2] & 0x0F;
			amount[6] = (cmd->cmdData[posCDOL1 + 3] & 0xF0) >> 4;
			amount[7] = cmd->cmdData[posCDOL1 + 3] & 0x0F;
			amount[8] = (cmd->cmdData[posCDOL1 + 4] & 0xF0) >> 4;
			amount[9] = cmd->cmdData[posCDOL1 + 4] & 0x0F;
			amount[10] = (cmd->cmdData[posCDOL1 + 5] & 0xF0) >> 4;
			amount[11] = cmd->cmdData[posCDOL1 + 5] & 0x0F;

			// block until user allows
			// 500 ms is approx 5200 ETUs at a frequency of 4MHz. Thus
			// do not use delays larger than 500 ms without requesting
			// more time from terminal (byte 0x60) as the default maximum
			// allowed response time is 9600 ETUs

			while(1){
				fprintf(stderr, "%s\n", strScroll);
				do{
					tmp = GetButton();
					_delay_ms(100);					
					if(SendByteTerminalParity(0x60, t_inverse))
					{
                        error = RET_TERMINAL_SEND_RESPONSE;
						FreeCAPDU(cmd);
						goto enderror;
					}
				}while((tmp & BUTTON_C) == 0);	
				_delay_ms(100);			

				fprintf(stderr, "Amt:%1X%1X%1X%1X%1X%1X%1X%1X%1X,%1X%1X\n",
					amount[1],
					amount[2],
					amount[3],
					amount[4],
					amount[5],
					amount[6],
					amount[7],
					amount[8],
					amount[9],
					amount[10],
					amount[11]);

				do{
					tmp = GetButton();
					_delay_ms(100);					
					if(SendByteTerminalParity(0x60, t_inverse))
					{
                        error = RET_TERMINAL_SEND_RESPONSE;
						FreeCAPDU(cmd);
						goto enderror;
					}
				}while((tmp & BUTTON_C) == 0);		
				_delay_ms(100);							

				fprintf(stderr, "%s\n", strDecide);
				do{
					tmp = GetButton();
					_delay_ms(100);					
					if(SendByteTerminalParity(0x60, t_inverse))
					{
                        error = RET_TERMINAL_SEND_RESPONSE;
						FreeCAPDU(cmd);
						goto enderror;
					}
				}while(((tmp & BUTTON_A) == 0) && 
						((tmp & BUTTON_C) == 0) &&
						((tmp & BUTTON_D) == 0));
				_delay_ms(100);

				if((tmp & BUTTON_D) != 0)
				{
					return RET_ERROR; // transaction cancelled					
				}

				if((tmp & BUTTON_A) != 0) break; // continue transaction
			} // while(1)

		} // if((cmd->cmdHeader->cla & 0xF0) == 0 ...	

		if(SendT0Command(cInverse, cTC1, cmd, logger))
		{
            error = RET_ICC_SEND_CMD;
			FreeCAPDU(cmd);
			goto enderror;
		}		

		response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader, logger);
		FreeCAPDU(cmd);	

		if(response == NULL)
		{
            error = RET_ERROR;
			FreeRAPDU(response);
			goto enderror;
		}
		FreeRAPDU(response);
	} //end while(gotGAC == 0)

    EnableWDT(4000);
	// continue rest of transaction until SCD is restarted by terminal reset
	while(1)
	{
		crp = ExchangeCompleteData(t_inverse, cInverse, t_TC1, cTC1, logger);
		if(crp == NULL)
        {
            error = RET_ERROR;
            goto enderror;
        }
		FreeCRP(crp);	
		ResetWDT();
	}	

	error = 0;

enderror:
    DisableWDT();
    DeactivateICC();
    if(logger)
    {
        LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
        WriteLogEEPROM(logger);
        fprintf(stderr, "%s\n", strLog);
        ResetLogger(logger);
    }

    return error;
}

/**
 * This function forwards commands between the terminal and ICC much like
 * the ForwardData method, but when it receives the Verify command
 * it stores the PIN entered by the user in the EEPROM. This is useful
 * in order to store the PIN once and then use it in further transactions
 * so that the original PIN is never revealed.
 *
 * This method returns immediately after receiving the verify command.
 * Thus PIN entry should be done for example using a CAP reader, where
 * the completion of this transaction is not essential.
 *
 * The PIN is stored in the EEPROM at the address mentioned by the
 * parameter EEPROM_PIN
 *
 * @param logger the log structure or NULL if log is not desired
 * @return This method returns zero (success) if the verify command
 * is sent with plaintext PIN. The method returns non-zero otherwise.
 */
uint8_t StorePIN(log_struct_t *logger)
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
	uint8_t tmp, len, error;
	CRP *crp;		

    if(lcdAvailable)
    {
        InitLCD();
        fprintf(stderr, "\n");
        fprintf(stderr, "Store   PIN\n");
        _delay_ms(1000);
    }

    DisableWDT();
    DisableTerminalResetInterrupt();
    DisableICCInsertInterrupt();

    // Expect the card to be inserted first and then wait a for terminal reset
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strInsertCard);
    while(IsICCInserted() == 0);
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strCardInserted);
    if(logger)
       LogByte1(logger, LOG_ICC_INSERTED, 0);
    while(GetTerminalResetLine() != 0);
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strTerminalReset);
    if(logger)
       LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);
	
	error = InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
            &cProto, &cTC1, &cTA3, &cTB3, logger);
	if(error)
		goto enderror;

	while(1)
	{
		crp = ExchangeCompleteData(t_inverse, cInverse, t_TC1, cTC1, logger);
		if(crp == NULL)
        {
            break;
        }

		// check for verify command
		if(crp->cmd->cmdHeader->cla == 0x00 &&
			 crp->cmd->cmdHeader->ins == 0x20)
		{
			// if PIN is not plaintext PIN then we abort
			if(crp->cmd->cmdHeader->p2 != 0x80 || 
				crp->cmd->cmdData == NULL)
			{				
				error = RET_TERMINAL_ENCRYPTED_PIN;
				if(lcdAvailable)
					fprintf(stderr, "Error:  %d\n", error);
                goto enderror;
			}
			
			tmp = crp->cmd->cmdData[0];
			len = crp->cmd->cmdHeader->p3;
			if((tmp & 0xF0) != 0x20 || len != crp->cmd->lenData)
			{
                error = RET_ERROR;
				if(lcdAvailable)
					fprintf(stderr, "Error:  %d\n", error);
				error = RET_ERROR;
                goto enderror;
			}

			// Write PIN command data to EEPROM
			cli();
            eeprom_write_byte((uint8_t*)EEPROM_PIN, len);
			eeprom_write_block(crp->cmd->cmdData, (void*)(EEPROM_PIN + 1), len);

			// All done
			if(lcdAvailable)
				fprintf(stderr, "PIN stored\n");
		}

		FreeCRP(crp);
	} // while(1)

    error = 0;

enderror:
    DeactivateICC();
    if(logger)
    {
        LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
        if(lcdAvailable)
            fprintf(stderr, "%s\n", strLog);
        WriteLogEEPROM(logger);
        ResetLogger(logger);
    }

	return error;
}

/**
 * This function is similar to ForwardData but it modifies the VERIFY
 * command. The command data of the VERIFY command is replaced with
 * stored data in EEPROM when the command is sent to the ICC. 
 * 
 * @param logger the log structure or NULL if log is not desired
 * @return 0 if successful, non-zero otherwise
 * @sa ForwardData
 */
uint8_t ForwardAndChangePIN(log_struct_t *logger)
{
	uint8_t t_inverse = 0, t_TC1 = 0, tdelay;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;	
	CAPDU *cmd, *tcmd = NULL;
	RAPDU *response;	
	uint8_t sreg, len;
	uint8_t *pin;
    uint8_t error;

    if(lcdAvailable)
    {
        InitLCD();
        fprintf(stderr, "\n");
        fprintf(stderr, "Change  PIN\n");
        _delay_ms(1000);
    }

    DisableWDT();
    DisableTerminalResetInterrupt();
    DisableICCInsertInterrupt();

    // Expect the card to be inserted first and then wait a for terminal reset
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strInsertCard);
    while(IsICCInserted() == 0);
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strCardInserted);
    if(logger)
       LogByte1(logger, LOG_ICC_INSERTED, 0);
    while(GetTerminalResetLine() != 0);
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strTerminalReset);
    if(logger)
       LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);

	// read EEPROM PIN data
	sreg = SREG;
	cli();
	len = eeprom_read_byte((uint8_t*)EEPROM_PIN);
	SREG = sreg;
    pin = (uint8_t*)malloc(len * sizeof(uint8_t));
	if(pin == NULL)
    {
        error = RET_ERROR;	
        goto enderror;
    }
    eeprom_read_block(pin, (void*)(EEPROM_PIN + 1), len);

	error = InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
            &cProto, &cTC1, &cTA3, &cTB3, logger);
	if(error)
    {
        if(lcdAvailable)
        {
            fprintf(stderr, "Error:  %d\n", error);
            _delay_ms(1000);
        }
		goto enderror;
    }

	// forward commands and change VERIFY
	while(1)
	{
		cmd = ReceiveT0Command(t_inverse, t_TC1, logger);
		if(cmd == NULL)
        {
            error = RET_ERROR;
            goto enderror;
        }

		// if PIN is plaintext then modify VERIFY command
		if(cmd->cmdHeader->cla == 0 && 
			cmd->cmdHeader->ins == 0x20 &&
			cmd->cmdHeader->p2 == 0x80 &&
			cmd->cmdData != NULL)
		{	
			// send modified VERIFY command
			tdelay = 1 + cTC1;
			tcmd = (CAPDU*)malloc(sizeof(CAPDU));
			tcmd->cmdHeader = (EMVCommandHeader*)malloc(sizeof(EMVCommandHeader));
			tcmd->cmdData = pin;
			tcmd->lenData = len;
			tcmd->cmdHeader->cla = cmd->cmdHeader->cla;
			tcmd->cmdHeader->ins = cmd->cmdHeader->ins;
			tcmd->cmdHeader->p1 = cmd->cmdHeader->p1;
			tcmd->cmdHeader->p2 = cmd->cmdHeader->p2;
			tcmd->cmdHeader->p3 = len;
			

			if(SendT0Command(cInverse, cTC1, tcmd, logger))
			{
                error = RET_ICC_SEND_CMD;
				FreeCAPDU(cmd);
				goto enderror;
			}		

			response = ForwardResponse(t_inverse, cInverse, tcmd->cmdHeader, logger);
			if(response == NULL)
			{
                error = RET_ERROR;
				FreeCAPDU(cmd);			
				FreeCAPDU(tcmd);
				goto enderror;
			}	

			FreeCAPDU(cmd);	
			FreeCAPDU(tcmd);		
			FreeRAPDU(response);			
		}
		else  		
		{
			if(SendT0Command(cInverse, cTC1, cmd, logger))
			{	
				error = RET_ICC_SEND_CMD;
				FreeCAPDU(cmd);
				goto enderror;
			}

			response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader, logger);
			if(response == NULL)
			{
				error = RET_ERROR;
				FreeCAPDU(cmd);							
				goto enderror;
			}
			
			FreeCAPDU(cmd);			
			FreeRAPDU(response);
		}					
		
	} //end while(1)

    error = 0;

enderror:
    DeactivateICC();
    if(logger)
    {
        LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
        if(lcdAvailable)
            fprintf(stderr, "%s\n", strLog);
        WriteLogEEPROM(logger);
        ResetLogger(logger);
    }

	return error;
}

/**
 * This function initiates the communication between ICC and
 * terminal and then forwards the commands and responses
 * from terminal to ICC and back until the terminal sends
 * a reset signal. 
 * 
 * When a reset signal is received the SCD restarts. The reset
 * signal can be used to copy the log data into EEPROM
 * as in the current implementation. The EEPROM data can then
 * be retrieved using any programmer.
 *
 * @param logger the log structure or NULL if log is not desired
 * @return 0 if successful, non-zero otherwise. See scd_values.h for details.
 */
uint8_t ForwardData(log_struct_t *logger)
{
	uint8_t t_inverse = 0, t_TC1 = 0, error = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
    CRP *crp = NULL;

    // Visual signal for this app
    Led1On();
    Led2Off();
    Led3Off();
    Led4Off();

    if(lcdAvailable)
    {
        if(GetLCDState() == 0)
            InitLCD();
        fprintf(stderr, "\n");
		fprintf(stderr, "Forward data\n");
        _delay_ms(500);
    }

    DisableWDT();
    DisableTerminalResetInterrupt();
    DisableICCInsertInterrupt();

    // Expect the card to be inserted first and then wait a for terminal reset
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strInsertCard);
    while(IsICCInserted() == 0);
    if(lcdAvailable)
        fprintf(stderr, "Connect terminal\n");
    if(logger)
       LogByte1(logger, LOG_ICC_INSERTED, 0);
    while(GetTerminalResetLine() != 0);
    if(lcdAvailable)
		fprintf(stderr, "Working ...\n");
    if(logger)
       LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);

    while(1) {
    error = InitSCDTransaction(t_inverse, t_TC1, &cInverse,
            &cProto, &cTC1, &cTA3, &cTB3, logger);
	if(error)
    {
        if(lcdAvailable)
        {
            fprintf(stderr, "Error:  %d\n", error);
            _delay_ms(1000);
        }
        goto enderror;
    }

	// update transaction counter
	nCounter++;

	while(1)
	{
		crp = ExchangeCompleteData(
                t_inverse, cInverse, t_TC1, cTC1, logger);
		if(crp == NULL)
            break;
        FreeCRP(crp);
	}
}
    error = 0;

enderror:
    DeactivateICC();
    if(logger)
    {
        LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
        if(lcdAvailable)
            fprintf(stderr, "%s\n", strLog);
        WriteLogEEPROM(logger);
        ResetLogger(logger);
    }

    if(error == RET_TERMINAL_TIME_OUT)
       error = 0;  // Just for now, check if needs to be changed

    return error;
}

/**
 * This function passively monitors a transaction,
 * similarly to ForwardData, but it only logs the
 * messages starting from the first Generate AC command in
 * order to save space in EEPROM.
 *
 * @param logger the log structure or NULL if log is not desired
 * @return 0 if successful, non-zero otherwise
 * @sa ForwardData
 */
uint8_t ForwardDataLogAC(log_struct_t *logger)
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
	uint8_t gotAC = 0, error = 0;
	CAPDU *cmd;
	RAPDU *response;	
	CRP *crp;

    if(lcdAvailable)
    {
        InitLCD();
        fprintf(stderr, "\n");
		fprintf(stderr, "Forward Data2\n");
        _delay_ms(1000);
    }

    DisableWDT();
    DisableTerminalResetInterrupt();
    DisableICCInsertInterrupt();

    // Expect the card to be inserted first and then wait a for terminal reset
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strInsertCard);
    while(IsICCInserted() == 0);
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strCardInserted);
    if(logger)
       LogByte1(logger, LOG_ICC_INSERTED, 0);
    while(GetTerminalResetLine() != 0);
    if(lcdAvailable)
        fprintf(stderr, "%s\n", strTerminalReset);
    if(logger)
       LogByte1(logger, LOG_TERMINAL_RST_LOW, 0);

	
    error = InitSCDTransaction(t_inverse, t_TC1, &cInverse,
            &cProto, &cTC1, &cTA3, &cTB3, logger);
	if(error)
    {
        if(lcdAvailable)
        {
            fprintf(stderr, "Error:  %d\n", error);
            _delay_ms(1000);
        }
		goto enderror;
    }

	// update transaction counter
	nCounter++;

	// forward data but don't log until we receive the generate AC
	while(gotAC == 0)
	{
		cmd = ReceiveT0Command(t_inverse, t_TC1, NULL);
		if(cmd == NULL)
        {
            error = RET_TERMINAL_GET_CMD;
            goto enderror;
        }

		if((cmd->cmdHeader->cla & 0xF0) == 0x80 &&
			cmd->cmdHeader->ins == 0xAE)
		{						
			// Generate AC command
            gotAC = 1;
		}

        if(gotAC)
            error = SendT0Command(cInverse, cTC1, cmd, logger);
        else
            error = SendT0Command(cInverse, cTC1, cmd, NULL);
        if(error)
        {	
            error = RET_ICC_SEND_CMD;
            FreeCAPDU(cmd);
            goto enderror;
        }

        if(gotAC)
            response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader, logger);
        else
            response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader, NULL);
        if(response == NULL)
        {
            error = RET_ERROR;
            FreeCAPDU(cmd);
            goto enderror;
        }

        FreeCAPDU(cmd);
        FreeRAPDU(response);
	} //end while(gotAC == 0)

    // continue rest of transaction until we see a terminal reset
    while(1)
    {
        crp = ExchangeCompleteData(
                t_inverse, cInverse, t_TC1, cTC1, logger);
        if(crp == NULL)
            break;
        FreeCRP(crp);	
    }	

    error = 0;

enderror:
    DeactivateICC();
    if(logger)
    {
        LogByte1(logger, LOG_ICC_DEACTIVATED, 0);
        if(lcdAvailable)
            fprintf(stderr, "%s\n", strLog);
        WriteLogEEPROM(logger);
        ResetLogger(logger);
    }

	return error;
}


/**
 * This method writes to EEPROM the log of the last transaction.
 * The log is done either while monitoring a card-terminal
 * transaction or by enabling logging while running  other application
 * (e.g. the Terminal() application).
 *
 * @param logger the log structure. If this is NULL the function
 * will exit promptly.
 */
void WriteLogEEPROM(log_struct_t *logger)
{
	uint16_t addrStream, write_size;
	uint8_t addrHi, addrLo;

    if(logger == NULL)
        return;

    // Visual signal for this app
    Led1Off();
    Led2Off();
    Led3On();
    Led4Off();

	// Update transaction counter in case it was modified
    eeprom_write_byte((uint8_t*)EEPROM_COUNTER, nCounter);

	// Get current transaction log pointer
	addrHi = eeprom_read_byte((uint8_t*)EEPROM_TLOG_POINTER_HI);
	addrLo = eeprom_read_byte((uint8_t*)EEPROM_TLOG_POINTER_LO);
	addrStream = ((addrHi << 8) | addrLo);

	if(logger->position > 0 && addrStream < EEPROM_MAX_ADDRESS)
	{
        // copy all possible data from log structure to EEPROM
        if(EEPROM_MAX_ADDRESS - addrStream < logger->position)
            write_size = EEPROM_MAX_ADDRESS - addrStream;
        else
            write_size = logger->position;
        eeprom_write_block(logger->log_buffer, (void*)addrStream, write_size);
        addrStream += write_size;

		// update log address
		addrHi = (uint8_t)((addrStream >> 8) & 0x00FF);
		addrLo = (uint8_t)(addrStream & 0x00FF);
        eeprom_write_byte((uint8_t*)EEPROM_TLOG_POINTER_HI, addrHi);
        eeprom_write_byte((uint8_t*)EEPROM_TLOG_POINTER_LO, addrLo);
	}

    Led3Off();
}



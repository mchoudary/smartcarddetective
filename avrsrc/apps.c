/** \file
 *	\brief	apps.c source file
 *
 *  This file implements the applications available on the SCD
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
#include <avr/io.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>

#include "apps.h"
#include "scd_io.h"
#include "scd_values.h"
#include "emv.h"
#include "scd_hal.h"
#include "scd.h"
#include "utils.h"
#include "terminal.h"
#include "emv_values.h"
#include "serial.h"
#include "VirtualSerial.h"


/// Set this to 1 to enable LCD functionality
#define LCD_ENABLED 1			

/// Set this to 1 to enable debug mode
#define DEBUG 0

/* Static variables */
#if LCD_ENABLED
static char* strDone = "All     Done";
static char* strError = "Error   Ocurred";
static char* strScroll = "BC to   scroll";
static char* strDecide = "BA = yesBD = no";
static char* strCardInserted = "Card    inserted";
static char* strBadProtocol = "Only T=0support";
static char* strPINOK = "PIN OK";
static char* strPINBAD = "PIN BAD";
static char* strPINTryExceeded = "PIN try exceeded";
#endif



/**
 * Virtual Serial Port application
 */
uint8_t VirtualSerial()
{
    char *buf;
    char *response = NULL;

    InitLCD();
    fprintf(stderr, "\n");

    fprintf(stderr, "Set up  serial\n");
     _delay_ms(500);
    SetupHardware();

    LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
    sei();
    fprintf(stderr, "VS Ready\n");
    _delay_ms(500);

    for (;;)
    {
        buf = GetHostData(255);
        if(buf == NULL)
        {
            _delay_ms(100);
            continue;
        }

        // Solve the problem ... why the virtual serial requires a delay before GetHostData....
#if DEBUG
        fprintf(stderr, "len: %d\n", strlen(buf));
        _delay_ms(500);
        fprintf(stderr, "%s\n", buf);
        _delay_ms(500);
#endif
        response = (char*)ProcessSerialData(buf);
        free(buf);

        if(response != NULL)
        {
            SendHostData(response);
            free(response);
            response = NULL;
        }

#if DEBUG
        fprintf(stderr, "VS round\n");
        _delay_ms(1000);
#endif
    }
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

   wdt_reset();

   // Select application
   //fci = ApplicationSelection(convention, TC1); // to use PSE first
   fci = SelectFromAID(convention, TC1, NULL);
   if(fci == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      status = 1;
      goto endtransaction;
   }
   wdt_reset();

   // Start transaction by issuing Get Processing Opts command
   appInfo = InitializeTransaction(convention, TC1, fci);
   if(appInfo == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      status = 1;
      goto endfci;
   }
   wdt_reset();

   // Get transaction data
   offlineAuthData = (ByteArray*)malloc(sizeof(ByteArray));
   tData = GetTransactionData(convention, TC1, appInfo, offlineAuthData);
   if(tData == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      status = 1;
      goto endappinfo;
   }
   wdt_reset();

   // Send internal authenticate command
   ddata = MakeByteArrayV(4,
         0x05, 0x06, 0x07, 0x08
         );
   response = SignDynamicData(convention, TC1, ddata);
   if(response == NULL)
   {
      fprintf(stderr, "%s\n", strError);
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
   
   return status;
}

/**
 * This method implements a terminal application with the basic steps
 * of an EMV transaction. This includes selection by AID, DDA signature,
 * PIN verification and transaction data authorization (Generate AC).
 *
 * @return 0 if successful, non-zero otherwise
 */
uint8_t Terminal()
{
   uint8_t convention, proto, TC1, TA3, TB3;
   uint8_t tmp;
   RAPDU *response = NULL;
   FCITemplate *fci = NULL;
   APPINFO *appInfo = NULL;
   RECORD *tData = NULL;
   ByteArray *offlineAuthData = NULL;
   ByteArray *pinTryCounter = NULL;
   ByteArray *pin = NULL;
   ByteArray *ddata = NULL;
   GENERATE_AC_PARAMS acParams;
   const TLV *cdol = NULL;

   //_delay_ms(100);  // wait for a possible reset
   SleepUntilCardInserted();

   // if card was removed restart the SCD and wait for insertion
   if(!IsICCInserted())
   {
      wdt_enable(WDTO_15MS);
      _delay_ms(100);
   }

   if(lcdAvailable) 
   {
      InitLCD();
      fprintf(stderr, "\n");
      fprintf(stderr, "%s\n", strCardInserted);
      _delay_ms(1000);
   }
   else
      return RET_ERROR;

#if DEBUG
#else
   // at this point the card should be inserted
   wdt_enable(WDTO_4S);
#endif

   // Initialize card
   if(ResetICC(0, &convention, &proto, &TC1, &TA3, &TB3)) return RET_ERROR;
   if(proto != 0)
   {
      fprintf(stderr, "%s\n", strBadProtocol);
      return RET_ERROR;
   }
   wdt_reset();

   // Select application
   //fci = ApplicationSelection(convention, TC1); // to use PSE first
   fci = SelectFromAID(convention, TC1, NULL);
   if(fci == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      goto endtransaction;
   }
   wdt_reset();

   // Start transaction by issuing Get Processing Opts command
   appInfo = InitializeTransaction(convention, TC1, fci);
   if(appInfo == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      goto endfci;
   }
   wdt_reset();

   // Get transaction data
   offlineAuthData = (ByteArray*)malloc(sizeof(ByteArray));
   tData = GetTransactionData(convention, TC1, appInfo, offlineAuthData);
   if(tData == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      goto endappinfo;
   }
   wdt_reset();

   // Send internal authenticate command (only for DDA cards supporting as per AIP)
   if((appInfo->aip[0] & 0x20) != 0)
   {
      ddata = MakeByteArrayV(4,
            0x01, 0x02, 0x03, 0x04
            );
      response = SignDynamicData(convention, TC1, ddata);
      if(response == NULL)
      {
         fprintf(stderr, "%s\n", strError);
         goto endtdata;
      }
      wdt_reset();
   }

   // Get PIN try counter
   pinTryCounter = GetDataObject(convention, TC1, PDO_PIN_TRY_COUNTER);
   if(pinTryCounter == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      goto endtdata;
   }
   if(pinTryCounter->bytes[0] == 0)
   {
      fprintf(stderr, "%s\n", strPINTryExceeded);
      goto endpintry;
   }
   wdt_reset();

   fprintf(stderr, "pin try:%d\n", pinTryCounter->bytes[0]);
   _delay_ms(1000);
   wdt_reset();

   // Send PIN verification
   // Below just an example for PIN=1234. Implement some pin entry mechanism
   // with buttons if needed
   pin = MakeByteArrayV(8, 0x24, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
   if(pin == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      goto endpintry;
   }
   tmp = VerifyPlaintextPIN(convention, TC1, pin);
   if(tmp == 0)
      fprintf(stderr, "%s\n", strPINOK);
   else
   {
      fprintf(stderr, "%s\n", strPINBAD);
      goto endpin;
   }
   wdt_reset();

   // Send the first GENERATE_AC command
   acParams.amount[5] = 0x55;
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
      fprintf(stderr, "%s\n", strError);
      goto endpin;
   }

   if(response != NULL) FreeRAPDU(response);
   response = SendGenerateAC(convention, TC1, AC_REQ_ARQC, cdol, &acParams);
   if(response == NULL)
   {
      fprintf(stderr, "%s\n", strError);
      goto endpin;
   }
   wdt_reset();


endpin:
   FreeByteArray(pin);
endpintry:
   FreeByteArray(pinTryCounter);
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
   _delay_ms(2000);
   
   return 0;
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
 * @return 0 if successful, non-zero otherwise
 */
uint8_t FilterGenerateAC()
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
	uint8_t tmp;
	uint8_t posCDOL1 = 0;
	uint8_t amount[12];	
	uint8_t gotGAC = 0;
	CAPDU *cmd;
	RAPDU *response;	
	RECORD *record;
	CRP *crp;

    SleepUntilTerminalClock();	
    // enable watchdog timer in case the application hangs.
    // wdr (watchdog reset) should be called to avoid reset
    wdt_enable(WDTO_4S);
	
	if(InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
		&cProto, &cTC1, &cTA3, &cTB3))
		return RET_ERROR;

	// Enable INT0 on falling edge
	// This is the recommended procedure, as in the data sheet
	// First disable the interrupt, then change the mode, then
	// clear the interrupt flag and finally enable the interrupt
	EIMSK &= ~(_BV(INT0));
	EICRA |= _BV(ISC01);
	EICRA &= ~(_BV(ISC00));
	EIFR |= _BV(INTF0);
	EIMSK |= _BV(INT0);	
	

	// reset wdt in case it was activated
	wdt_reset();

	if(lcdAvailable) 
	{
		InitLCD();
		fprintf(stderr, "\n");
	}
	else
	{
		DeactivateICC();		
		return RET_ERROR;
	}

	// forward commands until Read Record is received
	while(posCDOL1 == 0)
	{
		wdt_reset();
		cmd = ReceiveT0Command(t_inverse, t_TC1);
		if(cmd == NULL) return RET_ERROR;

		if((cmd->cmdHeader->cla & 0xF0) == 0 && 
			cmd->cmdHeader->ins == 0xB2)
		{						
			// read record command
			if(SendT0Command(cInverse, cTC1, cmd))
			{	
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			response = ReceiveT0Response(cInverse, cmd->cmdHeader);
			if(response == NULL)
			{	
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			if(response->repData != NULL)
			{
				record = ParseRECORD(response->repData, response->lenData);
				if(record == NULL)
				{
					FreeCAPDU(cmd);
					FreeRAPDU(response);
					return RET_ERROR;
				}

				posCDOL1 = AmountPositionInCDOLRecord(record);
				FreeRECORD(record);
				record = NULL;
			}

			if(SendT0Response(t_inverse, cmd->cmdHeader, response))
			{
				FreeCAPDU(cmd);
				FreeRAPDU(response);
				return RET_ERROR;
			}
		}
		else
		{
			// another command, just forward it
			if(SendT0Command(cInverse, cTC1, cmd))
			{	
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader);
			if(response == NULL)
			{
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			// not interested in data
			FreeCAPDU(cmd);
			FreeRAPDU(response);
		}	
	} //end while(posCDOL1 == 0)

	// Disable WDT as VERIFY command will delay
	MCUSR = 0;
    wdt_disable();

	// forward commands but block first Generate AC command
	while(gotGAC == 0)
	{
		wdt_reset();
		cmd = ReceiveT0Command(t_inverse, t_TC1);
		if(cmd == NULL) return RET_ERROR;		

		if((cmd->cmdHeader->cla & 0xF0) == 0x80 && 
			cmd->cmdHeader->ins == 0xAE)
		{
			// Generate AC command received
			if(cmd->cmdData == NULL)
			{
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			//wdt_enable(WDTO_4S); // re-enable wdt

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
						FreeCAPDU(cmd);
						return RET_ERROR;
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
						FreeCAPDU(cmd);
						return RET_ERROR;
					}
				}while((tmp & BUTTON_C) == 0);		
				_delay_ms(100);							

				fprintf(stderr, "%s\n", strDecide);
				do{
					tmp = GetButton();
					_delay_ms(100);					
					if(SendByteTerminalParity(0x60, t_inverse))
					{
						FreeCAPDU(cmd);
						return RET_ERROR;
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

			wdt_enable(WDTO_4S); // re-enable wdt

		} // if((cmd->cmdHeader->cla & 0xF0) == 0 ...	

		if(SendT0Command(cInverse, cTC1, cmd))
		{
			FreeCAPDU(cmd);
			return RET_ERROR;
		}		

		response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader);
		FreeCAPDU(cmd);	

		if(response == NULL)
		{
			FreeRAPDU(response);
			return RET_ERROR;
		}
		FreeRAPDU(response);
	} //end while(gotGAC == 0)

	// continue rest of transaction until SCD is restarted by terminal reset
	while(1)
	{
		wdt_reset();
		crp = ExchangeCompleteData(t_inverse, cInverse, t_TC1, cTC1);
		if(crp == NULL) return RET_ERROR;
		FreeCRP(crp);	
	}	

	// disable INT0	
	EIMSK &= ~(_BV(INT0));

	return 0;
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
 *
 * @return This method returns zero (success) if the verify command
 * is sent with plaintext PIN. The method returns non-zero otherwise.
 */
uint8_t StorePIN()
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
	uint8_t tmp, len;
	CRP *crp;		

    SleepUntilTerminalClock();	
    // enable watchdog timer in case the application hangs.
    // wdr (watchdog reset) should be called to avoid reset
    wdt_enable(WDTO_4S);
	
	if(InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
		&cProto, &cTC1, &cTA3, &cTB3))
		return RET_ERROR;

	// Enable INT0 on falling edge
	// This is the recommended procedure, as in the data sheet
	// First disable the interrupt, then change the mode, then
	// clear the interrupt flag and finally enable the interrupt
	EIMSK &= ~(_BV(INT0));
	EICRA |= _BV(ISC01);
	EICRA &= ~(_BV(ISC00));
	EIFR |= _BV(INTF0);
	EIMSK |= _BV(INT0);	

	wdt_reset();

#if LCD_ENABLED
	if(lcdAvailable)
	{
		InitLCD();
		fprintf(stderr, "\n");
	}
#endif

	while(1)
	{
		wdt_reset();
		crp = ExchangeCompleteData(t_inverse, cInverse, t_TC1, cTC1);
		if(crp == NULL) return RET_ERROR;		

		// Disable WDT after the first command
		MCUSR = 0;
    	wdt_disable();

		// check for verify command
		if(crp->cmd->cmdHeader->cla == 0x00 &&
			 crp->cmd->cmdHeader->ins == 0x20)
		{
			// if PIN is not plaintext PIN then we abort
			if(crp->cmd->cmdHeader->p2 != 0x80 || 
				crp->cmd->cmdData == NULL)
			{				
				if(lcdAvailable)
					fprintf(stderr, "Bad PIN cmd\n");
				wdt_enable(WDTO_15MS);
				return RET_ERROR;
			}
			
			tmp = crp->cmd->cmdData[0];
			len = crp->cmd->cmdHeader->p3;
			if((tmp & 0xF0) != 0x20 || len != crp->cmd->lenData)
			{
				if(lcdAvailable)
					fprintf(stderr, "Bad PIN cmd\n");
				wdt_enable(WDTO_15MS);
				return RET_ERROR;
			}

			// Write PIN command data to EEPROM
			cli();
                        eeprom_write_byte((uint8_t*)EEPROM_PIN, len);
			eeprom_write_block(crp->cmd->cmdData, (void*)(EEPROM_PIN + 1), len);

			// All done
#if LCD_ENABLED
			if(lcdAvailable)
				fprintf(stderr, "PIN stored\n");
			_delay_ms(1000);
#endif			
			wdt_enable(WDTO_4S); // re-enable wdt
			return 0;
		}

		FreeCRP(crp);
	} // while(1)

	// disable INT0	
	EIMSK &= ~(_BV(INT0));

	return 0;
}

/**
 * This function is similar to ForwardData but it modifies the VERIFY
 * command. The command data of the VERIFY command is replaced with
 * stored data in EEPROM when the command is sent to the ICC. 
 * 
 *
 * @return 0 if successful, non-zero otherwise
 * @sa ForwardData
 */
uint8_t ForwardAndChangePIN()
{
	uint8_t t_inverse = 0, t_TC1 = 0, tdelay;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;	
	CAPDU *cmd, *tcmd = NULL;
	RAPDU *response;	
	uint8_t sreg, len;
	uint8_t *pin;

    SleepUntilTerminalClock();	
    // enable watchdog timer in case the application hangs.
    // wdr (watchdog reset) should be called to avoid reset
    wdt_enable(WDTO_4S);

	// read EEPROM PIN data
	sreg = SREG;
	cli();
	len = eeprom_read_byte((uint8_t*)EEPROM_PIN);
	SREG = sreg;
        pin = (uint8_t*)malloc(len * sizeof(uint8_t));
	if(pin == NULL) return RET_ERROR;	
        eeprom_read_block(pin, (void*)(EEPROM_PIN + 1), len);

	// reset wdt in case it was set
	wdt_reset();	
	
	if(InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
		&cProto, &cTC1, &cTA3, &cTB3))
		return RET_ERROR;

	// Enable INT0 on falling edge
	// This is the recommended procedure, as in the data sheet
	// First disable the interrupt, then change the mode, then
	// clear the interrupt flag and finally enable the interrupt
	EIMSK &= ~(_BV(INT0));
	EICRA |= _BV(ISC01);
	EICRA &= ~(_BV(ISC00));
	EIFR |= _BV(INTF0);
	EIMSK |= _BV(INT0);	

	wdt_reset();

	if(lcdAvailable) 
	{
		InitLCD();
		fprintf(stderr, "\n");
	}
	else
	{
		DeactivateICC();		
		return RET_ERROR;
	}

	// forward commands and change VERIFY
	while(1)
	{
		wdt_reset();
		cmd = ReceiveT0Command(t_inverse, t_TC1);
		if(cmd == NULL) return RET_ERROR;

		// Disable WDT after the first command
		MCUSR = 0;
    	wdt_disable();

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
			

			if(SendT0Command(cInverse, cTC1, tcmd))
			{
				FreeCAPDU(cmd);
				return RET_ERROR;
			}		

			wdt_enable(WDTO_4S); // re-enable wdt			
			
			response = ForwardResponse(t_inverse, cInverse, tcmd->cmdHeader);
			if(response == NULL)
			{
				FreeCAPDU(cmd);			
				FreeCAPDU(tcmd);
				return RET_ERROR;
			}	

			FreeCAPDU(cmd);	
			FreeCAPDU(tcmd);		
			FreeRAPDU(response);			
		}
		else  		
		{
			if(SendT0Command(cInverse, cTC1, cmd))
			{	
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader);
			if(response == NULL)
			{
				FreeCAPDU(cmd);							
				return RET_ERROR;
			}
			
			FreeCAPDU(cmd);			
			FreeRAPDU(response);
		}					
		
	} //end while(1)

	// disable INT0	
	EIMSK &= ~(_BV(INT0));

	return 0;
}

/**
 * This function filters the Generate AC command just like
 * the FilterGenerateAC function but it also logs all the
 * information about the transaction 
 *
 * @return 0 if successful, non-zero otherwise
 * @sa FilterGenerateAC
 */
uint8_t FilterAndLog()
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
	uint8_t tmp;
	uint8_t posCDOL1 = 0;
	uint8_t amount[12];	
	uint8_t gotGAC = 0;
	CAPDU *cmd;
	RAPDU *response;	
	RECORD *record;
	CRP *crp;

    SleepUntilTerminalClock();	
    // enable watchdog timer in case the application hangs.
    // wdr (watchdog reset) should be called to avoid reset
    wdt_enable(WDTO_4S);
	
	if(InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
		&cProto, &cTC1, &cTA3, &cTB3))
		return RET_ERROR;

	// Enable INT0 on falling edge
	// This is the recommended procedure, as in the data sheet
	// First disable the interrupt, then change the mode, then
	// clear the interrupt flag and finally enable the interrupt
	EIMSK &= ~(_BV(INT0));
	EICRA |= _BV(ISC01);
	EICRA &= ~(_BV(ISC00));
	EIFR |= _BV(INTF0);
	EIMSK |= _BV(INT0);		

	// reset wdt in case it was activated
	wdt_reset();

	if(lcdAvailable) 
	{
		InitLCD();
		fprintf(stderr, "\n");
	}
	else
	{
		DeactivateICC();		
		return RET_ERROR;
	}

	// update transaction counter
	nCounter++;

	// forward commands until Read Record is received
	while(posCDOL1 == 0)
	{
		wdt_reset();
		cmd = ReceiveT0Command(t_inverse, t_TC1);
		if(cmd == NULL) return RET_ERROR;

		if((cmd->cmdHeader->cla & 0xF0) == 0 && 
			cmd->cmdHeader->ins == 0xB2)
		{						
			// read record command
			if(SendT0Command(cInverse, cTC1, cmd))
			{	
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			response = ReceiveT0Response(cInverse, cmd->cmdHeader);
			if(response == NULL)
			{	
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			if(response->repData != NULL)
			{
				record = ParseRECORD(response->repData, response->lenData);
				if(record == NULL)
				{
					FreeCAPDU(cmd);
					FreeRAPDU(response);
					return RET_ERROR;
				}

				posCDOL1 = AmountPositionInCDOLRecord(record);
				FreeRECORD(record);
				record = NULL;
			}			

			if(SendT0Response(t_inverse, cmd->cmdHeader, response))
			{
				FreeCAPDU(cmd);
				FreeRAPDU(response);
				return RET_ERROR;
			}
#if DEBUG
			Led3On();
#endif
		}
		else
		{
			// another command, just forward it
			if(SendT0Command(cInverse, cTC1, cmd))
			{	
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader);
			if(response == NULL)
			{
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

#if DEBUG
			Led3On();
#endif
		}			

		if(nTransactions < MAX_EXCHANGES)
		{
			transactionData[nTransactions] = (CRP*)malloc(sizeof(CRP));
			transactionData[nTransactions]->cmd = cmd;
			transactionData[nTransactions]->response = response;
			nTransactions++;
		}
		else
		{
			FreeCAPDU(cmd);
			FreeRAPDU(response);
		}

	} //end while(posCDOL1 == 0)

	// Disable WDT as VERIFY command will delay
	MCUSR = 0;
    wdt_disable();

	// forward commands but block first Generate AC command
	while(gotGAC == 0)
	{
		wdt_reset();
		cmd = ReceiveT0Command(t_inverse, t_TC1);
		if(cmd == NULL) return RET_ERROR;		

		if((cmd->cmdHeader->cla & 0xF0) == 0x80 && 
			cmd->cmdHeader->ins == 0xAE)
		{
			// Generate AC command received
			if(cmd->cmdData == NULL)
			{
				FreeCAPDU(cmd);
				return RET_ERROR;
			}

			//wdt_enable(WDTO_4S); // re-enable wdt

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
						FreeCAPDU(cmd);
						return RET_ERROR;
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
						FreeCAPDU(cmd);
						return RET_ERROR;
					}
				}while((tmp & BUTTON_C) == 0);		
				_delay_ms(100);							

				fprintf(stderr, "%s\n", strDecide);
				do{
					tmp = GetButton();
					_delay_ms(100);					
					if(SendByteTerminalParity(0x60, t_inverse))
					{
						FreeCAPDU(cmd);
						return RET_ERROR;
					}
				}while(((tmp & BUTTON_A) == 0) && 
						((tmp & BUTTON_C) == 0) &&
						((tmp & BUTTON_D) == 0));
				_delay_ms(100);

				if((tmp & BUTTON_D) != 0)
				{					
					// wait for terminal to put reset in low state
					// so that EEPROM becomes written and then restart
					return RET_ERROR; // transaction cancelled
				}

				if((tmp & BUTTON_A) != 0) break; // continue transaction
			} // while(1)

			wdt_enable(WDTO_4S); // re-enable wdt			

		} // if((cmd->cmdHeader->cla & 0xF0) == 0 ...	

		if(SendT0Command(cInverse, cTC1, cmd))
		{
			FreeCAPDU(cmd);
			return RET_ERROR;
		}		

		response = ForwardResponse(t_inverse, cInverse, cmd->cmdHeader);
		if(response == NULL)
		{
			FreeCAPDU(cmd);	
			FreeRAPDU(response);
			return RET_ERROR;
		}

		if(nTransactions < MAX_EXCHANGES)
		{
			transactionData[nTransactions] = (CRP*)malloc(sizeof(CRP));
			transactionData[nTransactions]->cmd = cmd;
			transactionData[nTransactions]->response = response;
			nTransactions++;
		}
		else
		{
			FreeCAPDU(cmd);
			FreeRAPDU(response);
		}
		
	} //end while(gotGAC == 0)

	// continue rest of transaction until SCD is restarted by terminal reset
	while(1)
	{
		wdt_reset();
		crp = ExchangeCompleteData(t_inverse, cInverse, t_TC1, cTC1);
		if(crp == NULL) return RET_ERROR;

		if(nTransactions < MAX_EXCHANGES)		
			transactionData[nTransactions++] = crp;			
		else		
			FreeCRP(crp);	
	}	

	// disable INT0	
	EIMSK &= ~(_BV(INT0));

	return 0;
}


/**
 * This function initiates the communication between ICC and
 * terminal and then forwards the commands and responses
 * from terminal to ICC and back until the terminal sends
 * a reset signal. 
 * 
 * When a reset signal is received the SCD restarts. The reset
 * signal can be used to copy the transaction data into EEPROM
 * as in the current implementation. The EEPROM data can then
 * be retrieved using any programmer.
 *
 * @return 0 if successful, non-zero otherwise
 */
uint8_t ForwardData()
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;

    SleepUntilTerminalClock();	
    // enable watchdog timer in case the application hangs.
    // wdr (watchdog reset) should be called to avoid reset
    wdt_enable(WDTO_4S);
	
	if(InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
		&cProto, &cTC1, &cTA3, &cTB3))
		return RET_ERROR;

	// Enable INT0 on falling edge
	// This is the recommended procedure, as in the data sheet
	// First disable the interrupt, then change the mode, then
	// clear the interrupt flag and finally enable the interrupt
	EIMSK &= ~(_BV(INT0));
	EICRA |= _BV(ISC01);
	EICRA &= ~(_BV(ISC00));
	EIFR |= _BV(INTF0);
	EIMSK |= _BV(INT0);

#if LCD_ENABLED
	if(lcdAvailable)
	{
		InitLCD();
		fprintf(stderr, "\n");
	}
#endif

	// update transaction counter
	nCounter++;

	while(1)
	{
		transactionData[nTransactions++] = ExchangeCompleteData(t_inverse, 
			cInverse, t_TC1, cTC1);
		if(transactionData[nTransactions-1] == NULL) return RET_ERROR;
		if(nTransactions == MAX_EXCHANGES) return RET_ERROR;

		// Disable WDT after the first command
		MCUSR = 0;
    	        wdt_disable();

	}

	// disable INT0	
	EIMSK &= ~(_BV(INT0));

#if LCD_ENABLED
        fprintf(stderr, "%s\n", strDone);
#endif

	return 0;
}


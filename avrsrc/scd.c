/** \file
 *	\brief scd.c source file for Smart Card Detective
 *
 *	This file implements the main application of the Smart Card Detective
 *
 *  It uses the functions defined in halSCD.h and EMV.h to communicate
 *  with the ICC and Terminal and the functions from scdIO.h to
 *  communicate with the LCD, buttons and LEDs 
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

#define F_CPU 16000000UL  		  // Change this to the correct frequency

// this is needed for the delay on the new avr-libc-1.7.0
#ifndef __DELAY_BACKWARD_COMPATIBLE__
#define __DELAY_BACKWARD_COMPATIBLE__
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <string.h>
#include <util/delay.h>
#include <stdlib.h>
#include <avr/wdt.h> 

#include "emv.h"
#include "scd_hal.h"
#include "scd_io.h"
#include "scd.h"
#include "utils.h"
#include "terminal.h"
#include "emv_values.h"
#include "scd_values.h"

/// Set this to 1 to enable LCD functionality
#define LCD_ENABLED 1			

/// Set this to 1 to enable debug mode
#define DEBUG 0

/// Set this to 1 to enable card presence interrupt
#define ICC_PRES_INT_ENABLE 1 		


// If there are any assembler routines, they can be used
// by declaring them as below
// extern void StartClkICC(void);

/* Functions used in the main program */
//static inline void StopTerminal(const char *string);


/* Static variables */
#if LCD_ENABLED
static char* strATRSent = "ATR Sent";
static char* strDone = "All     Done";
static char* strError = "Error Ocurred";
static char* strDataSent = "Data Sent";
static char* strScroll = "BC to   scroll";
static char* strSelect = "BD to   select";
static char* strAvailable = "Avail.  apps:";
static char* strDecide = "BA = yesBD = no";
static char* strCardInserted = "Card    inserted";
static char* strBadProtocol = "Only T=0support";
static char* strPINOK = "PIN     OK";
static char* strPINBAD = "PIN     BAD";
static char* strPINTryExceeded = "PIN try exceeded";
static char* strPowerUp = "Power Up";
static char* strPowerDown = "Power Down";
#endif

/* Global variables */
uint8_t warmResetByte;
CRP* transactionData[MAX_EXCHANGES]; 	// used by ForwardData()
uint8_t nTransactions;		        // used by ForwardData()
uint8_t lcdAvailable;			// non-zero if LCD is working
uint8_t nCounter;			// number of transactions
uint8_t selected;			// ID of application selected


// Use the LCD as stderr (see main)
FILE lcd_str = FDEV_SETUP_STREAM(LcdPutchar, NULL, _FDEV_SETUP_WRITE);


/**
 * Main program
 */
int main(void)
{
   uint8_t sreg;

   // Init SCD
   InitSCD();

   // Check the hardware
   //TestHardware();

   // Select application if BB is pressed while restarting
   if(GetButtonB() == 0)
   {
       selected = SelectApplication();

       if(selected == APP_ERASE_EEPROM)
       {
               Led2On();
               EraseEEPROM();
               Led2Off();
               wdt_enable(WDTO_15MS);
       }
       else
       {
               sreg = SREG;
               cli();
               eeprom_write_byte((uint8_t*)EEPROM_APPLICATION, selected);
               SREG = sreg;
       }

       // restart SCD so that LCD power is reduced (small trick)
       wdt_enable(WDTO_15MS);
   }
   else
   {
       sreg = SREG;
       cli();
       selected = eeprom_read_byte((uint8_t*)EEPROM_APPLICATION);
       SREG = sreg;
   }

   // run the selected application
   switch(selected)
   {
       case APP_STORE_PIN: 
         StorePIN();
       break;

       case APP_LOG_FORWARD: 
         ForwardData();
       break;

       case APP_FW_MODIFY_PIN: 
         ForwardAndChangePIN();
       break;

       case APP_FILTER_GENERATEAC: 
         FilterGenerateAC();
       break;

       case APP_FILTER_LOG:
         FilterAndLog();
       break;

       case APP_TERMINAL_1:
         Terminal1();
       break;

       default:
         selected = APP_TERMINAL_1;
         eeprom_write_byte((uint8_t*)EEPROM_APPLICATION, selected);
         Terminal1();
   }

   // if needed disable wdt to avoid restart
   // wdt_disable();

   SwitchLeds();			
}

/**
 * This function shows a menu with the existing applications and allows
 * the user to select one of them. 
 *
 * The different applications are shown on the LCD and the user can
 * use the buttons to scroll between the list and to select the desired
 * application.
 *
 * @return a number representing the selected application. The relation
 * between numbers and applications is done by the define directives
 * (APP_STORE_PIN, etc.). If any error occurs, this method will return
 * 0 (i.e. no application selected).
 */
uint8_t SelectApplication()
{
	volatile uint8_t tmp;
	uint8_t i;

	if(!lcdAvailable) return 0;

	InitLCD();
	fprintf(stderr, "\n");

	while(1){
		fprintf(stderr, "%s\n", strScroll);
		do{
			tmp = GetButton();
		}while((tmp & BUTTON_C) == 0);
		_delay_ms(500);

		fprintf(stderr, "%s\n", strSelect);
		do{
			tmp = GetButton();
		}while((tmp & BUTTON_C) == 0);
		_delay_ms(500);

		fprintf(stderr, "%s\n", strAvailable);
		do{
			tmp = GetButton();
		}while((tmp & BUTTON_C) == 0);	
		_delay_ms(500);

		for(i = 0; i < APPLICATION_COUNT; i++)
		{
			fprintf(stderr, "%s\n", appStrings[i]);
			while(1)
			{
				tmp = GetButton();
				if((tmp & BUTTON_D) != 0) return (i + 1);
				if((tmp & BUTTON_C) != 0) break;			
			}
			_delay_ms(500);
		}
	}

	return 0;
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

endddata:
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
uint8_t Terminal1()
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

   // at this point the card should be inserted
   wdt_enable(WDTO_4S);

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


/**
 * This method should be called before any other operation. It sets
 * the I/O ports to a correct state and it also recovers any necessary
 * data from EEPROM.
 */
void InitSCD()
{
	// disable any interrupts	
	cli();
	EICRA = 0;
	EICRB = 0;
	EIFR = 0xFF;
	EIMSK = 0;

	// Disable WDT to keep safe operation
	MCUSR = 0;
        wdt_disable();

	// Ports setup
	DDRB = 0x00;

	DDRC = 0x00;
	PORTC = 0x18;		// PC4 with internal pull-up (Terminal I/O)
						// PC3 with internal pull-up (terminal clock)

	DDRD = 0x80;		
	PORTD = 0x83;		// PD7 high (ICC VCC) , PD1 pull-up
						// (ICC switch), PD0 pull-up (terminal reset)

	DDRF &= 0xF0;
	PORTF |= 0x0F; 		// enable pull-up for buttons	

	// change CLK Prescaler value
	clock_prescale_set(clock_div_1); 	

	// light power led
	Led4On();	

#if ICC_PRES_INT_ENABLE
	// enable INT1 on any edge
	EICRA |= _BV(ISC10);
	EICRA &= ~(_BV(ISC11));
	EIMSK |= _BV(INT1);
#endif	

	// Read Warm byte info from EEPROM
	warmResetByte = eeprom_read_byte((uint8_t*)EEPROM_WARM_RESET);

	// Read number of transactions in EEPROM
	nCounter = eeprom_read_byte((uint8_t*)EEPROM_COUNTER);	
	
	// Reset transaction index for log	
	nTransactions = 0;

	// Check LCD status and use as stderr if status OK
	if(CheckLCD())
	{
		stderr = NULL;
		lcdAvailable = 0;
	}
	else	
	{
		stderr = &lcd_str;
		lcdAvailable = 1;
	}

	// disable LCD power
	DDRC &= ~(_BV(PC5));
	PORTC &= ~(_BV(PC5));	
	SetLCDState(0);

	// Disable most modules; they should be re-enabled when needed
	power_adc_disable();
	power_spi_disable();
	power_twi_disable();
	power_usart1_disable();
	power_usb_disable();

	// Enable interrupts
	sei();	
}

/**
 * This function puts the SCD to sleep (including all peripherials), until
 * there is clock received from terminal
 */
void SleepUntilTerminalClock()
{
	uint8_t sreg, lcdstate;

	Write16bitRegister(&OCR3A, 100);
	Write16bitRegister(&TCNT3, 1);
	TCCR3A = 0;	
	TIMSK3 = 0x02;  //Interrupt on Timer3 compare A match
	TCCR3B = 0x0F;	// CTC, timer external source
	sreg = SREG;

	// stop LCD and LEDs before going to sleep
	lcdstate = GetLCDState();
	if(lcdAvailable && lcdstate != 0) LCDOff();
	Led1Off();
	Led2Off();
	Led3Off();
	Led4Off();

	// go to sleep
	set_sleep_mode(SLEEP_MODE_IDLE); // it is also possible to use sleep_mode() below
	cli();
	sleep_enable();	
	sei();
        sleep_cpu();    

	// back from sleep
	sleep_disable();	
	SREG = sreg;
	TIMSK3 = 0; // disable interrupts on Timer3
	TCCR3B = 0; // stop timer	
	Led4On();
}

/**
 * This function puts the SCD to sleep (including all peripherials), until
 * the card is inserted or removed
 */
void SleepUntilCardInserted()
{
	uint8_t sreg, lcdstate;

	// stop LCD and LEDs before going to sleep
	lcdstate = GetLCDState();
	if(lcdAvailable && lcdstate != 0) LCDOff();
	Led1Off();
	Led2Off();
	Led3Off();
	Led4Off();

	// go to sleep
	sreg = SREG;
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
	sleep_enable();	
	sei();
        sleep_cpu();    

	// back from sleep
	sleep_disable();	
	SREG = sreg;
	Led4On();
}

/* Intrerupts */

/**
 * Interrupt routine for INT0. This interrupt can fire when the
 * reset signal from the Terminal goes low (active) and the corresponding
 * interrupt is enabled
 */
ISR(INT0_vect)
{
	uint8_t i;
	uint8_t *stream = NULL, lStream = 0;
	uint16_t addrStream;
	uint8_t addrHi, addrLo;
	uint8_t cmdstr[] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
	uint8_t rspstr[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
	uint8_t appstr[] = {0xDD, 0xDD, 0xDD, 0xDD, 0xDD};
	uint8_t endstr[] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB};

	// disable wdt
	MCUSR = 0;
    wdt_disable();

	// Update transaction counter in case it was modified
        eeprom_write_byte((uint8_t*)EEPROM_COUNTER, nCounter);

	// Get current transaction log pointer
	addrHi = eeprom_read_byte((uint8_t*)EEPROM_TLOG_POINTER_HI);
	addrLo = eeprom_read_byte((uint8_t*)EEPROM_TLOG_POINTER_LO);
	addrStream = ((addrHi << 8) | addrLo);

	if(addrStream == 0xFFFF) addrStream = EEPROM_TLOG_DATA;

	
	if(nTransactions > 0 && addrStream < EEPROM_MAX_ADDRESS)
	{
		// write selected application ID
		eeprom_write_block(appstr, (void*)addrStream, 5);
		addrStream += 5;
		eeprom_write_byte((uint8_t*)addrStream, selected);
		addrStream += 1;

		// serialize and write transaction data to EEPROM
		for(i = 0; i < nTransactions; i++)
		{
			// Write command to EEPROM
			stream = SerializeCommand(transactionData[i]->cmd, &lStream);
			if(stream != NULL)
			{
				eeprom_write_block(cmdstr, (void*)addrStream, 5);
				addrStream += 5;
				eeprom_write_block(stream, (void*)addrStream, lStream);
				free(stream);
				stream = NULL;
				addrStream += lStream;
				if(addrStream > EEPROM_MAX_ADDRESS) break;
			}

			// Write response to EEPROM
			stream = SerializeResponse(transactionData[i]->response, &lStream);
			if(stream != NULL)
			{
				eeprom_write_block(rspstr, (void*)addrStream, 5);
				addrStream += 5;
				eeprom_write_block(stream, (void*)addrStream, lStream);
				free(stream);
				stream = NULL;
				addrStream += lStream;
				if(addrStream > EEPROM_MAX_ADDRESS) break;
			}

			FreeCRP(transactionData[i]);	
		}

		// write end of log
	        eeprom_write_block(endstr, (void*)addrStream, 5);
		addrStream += 5;		

		// update log address
		addrHi = (uint8_t)((addrStream >> 8) & 0x00FF);
		addrLo = (uint8_t)(addrStream & 0x00FF);
		addrLo += 8;
		addrLo = addrLo & 0xF8; // make the address multiple
								// of 8 (page size)
                eeprom_write_byte((uint8_t*)EEPROM_TLOG_POINTER_HI, addrHi);
                eeprom_write_byte((uint8_t*)EEPROM_TLOG_POINTER_LO, addrLo);
	}

	if(GetTerminalFreq())
	{
		// warm reset
		warmResetByte = eeprom_read_byte((uint8_t*)EEPROM_WARM_RESET);

		if(warmResetByte == WARM_RESET_VALUE)
		{
			// we already had a warm reset so go to initial state
                        eeprom_write_byte((uint8_t*)EEPROM_WARM_RESET, 0);
			while(EECR & _BV(EEPE));			
		}
		else
		{
			// set 0xAA in EEPROM meaning we have a warm reset
                        eeprom_write_byte((uint8_t*)EEPROM_WARM_RESET, WARM_RESET_VALUE);
			while(EECR & _BV(EEPE));			
		}		
	}
	else
	{
		// restart SCD
                eeprom_write_byte((uint8_t*)EEPROM_WARM_RESET, 0);
		while(EECR & _BV(EEPE));		
	}

	// disable INT0	
	EIMSK &= ~(_BV(INT0));

	// re-enable wdt to restart device
	wdt_enable(WDTO_15MS);
}

/**
 * Interrupt routine for INT1. This interrupt can fire when the
 * ICC is inserted or removed and the corresponding interrupt is enabled
 */
ISR(INT1_vect)
{	
	if(bit_is_set(PIND, PD1))
	{			
		Led3On();
	}
	else
	{
		Led3Off();		
		DeactivateICC();
	}

}

/**
 * Interrupt routine for Timer3 Compare Match A overflow. This interrupt
 * can fire when the Timer3 matches the OCR3A value and the corresponding
 * interrupt is enabled
 */
ISR(TIMER3_COMPA_vect, ISR_NAKED)
{	
	reti();	// Do nothing, used just to wake up the CPU
}

/**
 * This function performs a test of the hardware.
 */
void TestHardware()
{
#if LCD_ENABLED
	char* strBA = "Press BA";
	char* strBB = "Press BB";
	char* strBC = "Press BC";
	char* strBD = "Press BD";
	char* strAOK = "All fine!";	
#endif


	Led1On();
	_delay_ms(50);
	Led1Off();
	Led2On();
	_delay_ms(50);
	Led2Off();
	Led3On();
	_delay_ms(50);
	Led3Off();
	Led4On();
	_delay_ms(50);
	Led4Off();

#if LCD_ENABLED
	if(lcdAvailable)
	{
		InitLCD();
		fprintf(stderr, "\n");

		WriteStringLCD(strBA, strlen(strBA));		
		while(bit_is_set(PINF, PF3));

		WriteStringLCD(strBB, strlen(strBB));		
		while(bit_is_set(PINF, PF2));

		WriteStringLCD(strBC, strlen(strBC));		
		while(bit_is_set(PINF, PF1));

		WriteStringLCD(strBD, strlen(strBD));		
		while(bit_is_set(PINF, PF0));

		WriteStringLCD(strAOK, strlen(strAOK));		
	}
#endif	
}

/**
 * This function initiates the communication between ICC and
 * terminal and then forwards the commands and responses
 * from terminal to ICC but blocks the Generate AC request
 * until the user accepts or denies the transaction * 
 *
 * If the terminal enables the reset line (either by warm reset or
 * by interrupting the transaction the INT0 interrupt routing will
 * be called
 *
 * This function uses hard-coded data. Please use FilterGenerateAC
 * instead.
 *
 * @return 0 if successful, non-zero otherwise
 * @sa FilterGenerateAC
 */
uint8_t FilterGenerateACSimple()
{
	uint8_t t_inverse = 0, t_TC1 = 0;
	uint8_t cInverse, cProto, cTC1, cTA3, cTB3;
	uint8_t ba, bb;
	uint8_t amount[12];	
	CAPDU *cmd;
	RAPDU *response;
	CRP *crp;
	
	// Enable INT0 on falling edge
	// This is the recommended procedure, as in the data sheet
	// First disable the interrupt, then change the mode, then
	// clear the interrupt flag and finally enable the interrupt
	EIMSK &= ~(_BV(INT0));
	EICRA |= _BV(ISC01);
	EICRA &= ~(_BV(ISC00));
	EIFR |= _BV(INTF0);
	EIMSK |= _BV(INT0);	
	
	if(InitSCDTransaction(t_inverse, t_TC1, &cInverse, 
		&cProto, &cTC1, &cTA3, &cTB3))
		return RET_ERROR;

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

	// forward commands until Generate AC
	while(1)
	{
		cmd = ReceiveT0Command(t_inverse, t_TC1);
		if(cmd == NULL) return RET_ERROR;

		if((cmd->cmdHeader->cla & 0xF0) == 0x80 && cmd->cmdHeader->ins == 0xAE)
		{
			// this is a "trick" where I consider amount to be at the
			// beginning. Please make a proper implementation using a CDOL1
			// analysis and match with the exact position inside the CDOL1
			amount[0] = (cmd->cmdData[0] & 0xF0) >> 4;
			amount[1] = cmd->cmdData[0] & 0x0F;
			amount[2] = (cmd->cmdData[1] & 0xF0) >> 4;
			amount[3] = cmd->cmdData[1] & 0x0F;
			amount[4] = (cmd->cmdData[2] & 0xF0) >> 4;
			amount[5] = cmd->cmdData[2] & 0x0F;
			amount[6] = (cmd->cmdData[3] & 0xF0) >> 4;
			amount[7] = cmd->cmdData[3] & 0x0F;
			amount[8] = (cmd->cmdData[4] & 0xF0) >> 4;
			amount[9] = cmd->cmdData[4] & 0x0F;
			amount[10] = (cmd->cmdData[5] & 0xF0) >> 4;
			amount[11] = cmd->cmdData[5] & 0x0F;

			// block until user allows
			do{
				fprintf(stderr, "Amt:%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d%1d\n",
					amount[0],
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
				_delay_ms(500);

				// request more time while user decides
				if(SendByteTerminalParity(0x60, t_inverse))
				{
					FreeCAPDU(cmd);
					return RET_ERROR;
				}


				fprintf(stderr, "Authorize?\n");
				_delay_ms(500);

				// request more time while user decides
				if(SendByteTerminalParity(0x60, t_inverse))
				{
					FreeCAPDU(cmd);
					return RET_ERROR;
				}

				fprintf(stderr, "BA = YES BB = NO\n");
				_delay_ms(500);				

				ba = GetButtonA();
				bb = GetButtonB();

				// request more time while user decides
				if(SendByteTerminalParity(0x60, t_inverse))
				{
					FreeCAPDU(cmd);
					return RET_ERROR;
				}
			}while(ba && bb);

			if(!bb)
				wdt_enable(WDTO_15MS); // restart SCD, transaction cancelled
		}

		//else just go on
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
	} //end while(1)

	// continue rest of transaction
	while(1)
	{
		crp = ExchangeCompleteData(t_inverse, cInverse, t_TC1, cTC1);
		if(crp == NULL) return RET_ERROR;
		FreeCRP(crp);	
	}	

	// disable INT0	
	EIMSK &= ~(_BV(INT0));

	return 0;
}



/**
 * This function implements an infinite communication loop between
 * the SCD and my terminal emulator, by continuously replying to
 * the SELECT command for "1PAY.SYS.DDF01"
 *
 * The main role of this function is to test the correct transfer
 * of bytes between the terminal and the SCD
 */
void TestSCDTerminal()
{
	char strLCD[16];
	uint8_t tmpa;

	//start Timer for Terminal
	StartCounterTerminal();
	
	// wait for Terminal CLK and send ATR
	while(ReadCounterTerminal() < 100);
	Led1On();	
	while(GetResetStateTerminal() == 0);
	Led2On();
	LoopTerminalETU(10);
	SendT0ATRTerminal(0, 0x0F);
	Led1Off();	


#if LCD_ENABLED
	if(lcdAvailable)
	{
		InitLCD();
		fprintf(stderr, "\n");
		WriteStringLCD(strATRSent, strlen(strATRSent));		
	}
#endif

	while(1)
	{
		// Get SELECT command for "1PAY.SYS.DDF01"
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[0]);	// CLA = 0x00
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[1]);	// INS = 0xA4
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[2]);	// P1 = 0x04
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[3]);	// P2 = 0x00
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[4]);	// P3 = 0x0E

		strLCD[5] = 0;	

		Led1On();
		Led2Off();		

		// Send INS (procedure byte) back
		LoopTerminalETU(20);
		//SendByteTerminalNoParity(0xA4, 0);		
		SendByteTerminalParity(0xA4, 0);		

		Led1Off();
		Led2On();

		// Get Select command data => "1PAY.SYS.DDF01"
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[0]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[1]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[2]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[3]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[4]);	
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[5]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[6]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[7]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[8]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[9]);	
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[10]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[11]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[12]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[13]);		
		strLCD[14] = 0;	
		
		Led1On();
		Led2Off();

#if LCD_ENABLED
		if(lcdAvailable)
		{
			if(tmpa != 0)
				WriteStringLCD(strError, strlen(strError));
			else
				WriteStringLCD(strLCD, 14);		
		}
#endif

		// Send "6104" (procedure bytes) back		
		SendByteTerminalParity(0x61, 0);		
		LoopTerminalETU(2);		
		SendByteTerminalParity(0x04, 0);		

		Led1Off();
		Led2On();

		// Get GetResponse from Reader
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[0]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[1]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[2]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[3]);		
		tmpa = GetByteTerminalParity(0, (uint8_t*)&strLCD[4]);	
		strLCD[5] = 0;	

		Led1On();
		Led2Off();

		// Send some data back as response to select command
		LoopTerminalETU(20);		
		SendByteTerminalParity(0xC0, 0);		
		LoopTerminalETU(2);
		
		SendByteTerminalParity(0xDE, 0);		
		LoopTerminalETU(2);
		
		SendByteTerminalParity(0xAD, 0);		
		LoopTerminalETU(2);
		
		SendByteTerminalParity(0xBE, 0);		
		LoopTerminalETU(2);
		
		SendByteTerminalParity(0xEF, 0);		
		LoopTerminalETU(2);
		
		SendByteTerminalParity(0x90, 0);		
		LoopTerminalETU(2);
		
		SendByteTerminalParity(0x00, 0);		

		Led1Off();
		Led2On();

#if LCD_ENABLED
		if(lcdAvailable)
			WriteStringLCD(strDataSent, strlen(strDataSent));
#endif
	}
}

/**
 * This function makes a simple test with the ICC
 *
 * It powers the ICC, expects the ATR and sends a select
 * command, waiting also for the answer
 */
void TestSCDICC()
{
	uint8_t inverse, proto, TC1, TA3, TB3;
	uint8_t byte;

	// Power ICC and get ATR
	if(ResetICC(0, &inverse, &proto, &TC1, &TA3, &TB3)) return;
 
	// Send SELECT command (no data yet)
	LoopICCETU(5);
	SendByteICCParity(0x00, inverse);
	LoopICCETU(2);
	SendByteICCParity(0xA4, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x04, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x00, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x0E, inverse);

	// Get INS back
	LoopICCETU(1);
	GetByteICCParity(inverse, &byte);
	if(byte != 0xA4) return;

	// Send "1PAY.SYS.DDF01"
	LoopICCETU(5);
	SendByteICCParity(0x31, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x50, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x41, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x59, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x2E, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x53, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x59, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x53, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x2E, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x44, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x44, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x46, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x30, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x31, inverse);
	
	// Expect 0x61, 0xXX
	LoopICCETU(1);
	GetByteICCParity(inverse, &byte); 
	if(byte != 0x61) return;
	LoopICCETU(1);
	GetByteICCParity(inverse, &byte);

	// Send GET Response
	LoopICCETU(5);
	SendByteICCParity(0x00, inverse);
	LoopICCETU(2);
	SendByteICCParity(0xC0, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x00, inverse);
	LoopICCETU(2);
	SendByteICCParity(0x00, inverse);
	LoopICCETU(2);
	SendByteICCParity(byte, inverse);

	// Data should follow from the ICC...
	Led1On();
#if LCD_ENABLED
	if(lcdAvailable)
	{
		InitLCD();
		fprintf(stderr, "\n");
		WriteStringLCD(strDataSent, strlen(strDataSent));		
	}
#endif
}

/** 
 * This function is used just to switch leds on and off
 */
void SwitchLeds()
{
   while(1)
   {
      _delay_ms(500);
      Led1On();
      Led2Off();
      _delay_ms(500);
      Led1Off();
      Led2On();
   }
}

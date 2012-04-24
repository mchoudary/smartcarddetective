/**
 * \file
 * \brief terminal.c source file for Smartcard Defender
 *
 * This file implements the functions for a terminal application
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

#include <string.h>
#include <util/delay.h>
#include <stdlib.h>

#include "emv.h"
#include "scd_hal.h"
#include "terminal.h"
#include "emv_values.h"
#include "scd_values.h"
#include "scd_io.h"

/// Set this to 1 to enable debug code
#define DEBUG 1

/// Set this to enable trigger signals, e.g. to use with oscilloscope
#define TRIGGER 1

// ------------------------------------------------
// Static declarations
static RAPDU* TerminalSendT0CommandR(CAPDU* tmpCommand, RAPDU *tmpResponse,
   uint8_t inverse_convention, uint8_t TC1, log_struct_t *logger);

//--------------------------------------------------------------------
// Constants
static const uint8_t nPSELen = 14;
static const uint8_t bPSEString[14] = { '1', 'P', 'A', 'Y', '.', 'S', 'Y',
   'S', '.', 'D', 'D', 'F', '0', '1'};
static const uint8_t nAIDLen = 7;
static const uint8_t nAIDEntries = 6;
static const uint8_t bAIDList[42] = {
   0xA0, 0, 0, 0, 0x29, 0x10, 0x10, // Link ATM
   0xA0, 0, 0, 0, 0x03, 0x10, 0x10, // Connect Debit VISA
   0xA0, 0, 0, 0, 0x04, 0x10, 0x10, // Connect Debit MasterCard
   0xA0, 0, 0, 0, 0x03, 0x80, 0x02, // CAP VISA
   0xA0, 0, 0, 0, 0x04, 0x80, 0x02, // CAP MasterCard
   0xA0, 0, 0, 0x02, 0x44, 0, 0x10  // Other App
};

                                 

// ------------------------------------------------
// Public methods


/**
 * This function handles the process of sending a command for
 * the protocol T=0, including the intermediate GET_RESPONSE
 * for the different command classes. Because of the
 * recursivity of this method, it introduces an initial
 * delay of 16 ICC ETUs to allow the card to be ready for
 * a new command
 * 
 * @param cmd Command APDU to be sent
 * @param inverse_convention different than 0 if inverse convention
 * is to be used
 * @param TC1 byte returned in the ATR
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the RAPDU created from the card reply, or NULL
 * if a response APDU cannot be constructed
 */
RAPDU* TerminalSendT0Command(
        CAPDU* cmd,
        uint8_t inverse_convention,
        uint8_t TC1,
        log_struct_t *logger)
{
   CAPDU *tmpCommand;

   tmpCommand = CopyCAPDU(cmd);
   if(tmpCommand == NULL) return NULL;

   return TerminalSendT0CommandR(
           tmpCommand, NULL, inverse_convention, TC1, logger);
}


/**
 * This function handles the process of sending a command for
 * the protocol T=0, including the intermediate GET_RESPONSE
 * for the different command classes. This function should
 * should only be used by TerminalSendT0Command
 * 
 * @param cmd Command APDU to be sent
 * @param tmpResponse temporary response data used by this method
 * @param inverse_convention different than 0 if inverse convention
 * is to be used
 * @param TC1 byte returned in the ATR
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the RAPDU created from the card reply, or NULL
 * if a response APDU cannot be constructed
 * @sa TerminalSendT0Command
 */
static RAPDU* TerminalSendT0CommandR(
        CAPDU *tmpCommand,
        RAPDU *tmpResponse,
        uint8_t inverse_convention,
        uint8_t TC1,
        log_struct_t *logger)
{
   RAPDU *response, *tmp;

#if TRIGGER
   // Make sure the trigger signals are low so we can watch them going high
   JTAG_P1_Low();
   JTAG_P3_Low();
#endif

   LoopICCETU(16); // wait for card to be ready to receive new command
   
   if(SendT0Command(inverse_convention, TC1, tmpCommand, logger))
   {
      FreeRAPDU(tmpResponse);
      FreeCAPDU(tmpCommand);
      return NULL;
   }

#if TRIGGER
   /* Code below used to create a trigger signal */
   if(TC1 > 0)
   {
       asm volatile("nop\n\t"::);
       JTAG_P1_High();
       if(TC1 == 2) JTAG_P3_High();
       _delay_ms(1);

       JTAG_P1_Low();
       if(TC1 == 2) JTAG_P3_Low();
   }
#endif

   tmp = ReceiveT0Response(inverse_convention, tmpCommand->cmdHeader, logger);

   if(tmp == NULL || tmp->repStatus == NULL)
   {
      FreeRAPDU(tmpResponse);
      FreeCAPDU(tmpCommand);
      return NULL;
   }

   response = CopyRAPDU(tmp);
   if(response == NULL)
   {
      FreeRAPDU(tmpResponse);
      FreeRAPDU(tmp);
      FreeCAPDU(tmpCommand);
      return NULL;
   }

   // If there was already some data from an early response keep it.
   // else just keep the last response. In case this last response
   // is bad this will be seen in the status bytes
   if(tmpResponse != NULL && tmpResponse->repData != NULL &&
      tmpResponse->lenData != 0)
   {
      response->lenData = tmpResponse->lenData + tmp->lenData;
      response->repData = (uint8_t*)realloc(response->repData,
         (response->lenData) * sizeof(uint8_t));
      if(response->repData == NULL)
      {
         FreeRAPDU(tmpResponse);
         FreeRAPDU(tmp);
         FreeRAPDU(response);
         FreeCAPDU(tmpCommand);
         return NULL;
      }
      memcpy(response->repData, tmpResponse->repData, tmpResponse->lenData);
      memcpy(&(response->repData[tmpResponse->lenData]), tmp->repData, tmp->lenData);
   }
   FreeRAPDU(tmp);
   FreeRAPDU(tmpResponse);

   if(response->repStatus->sw1 == (uint8_t)SW1_MORE_DATA ||
      response->repStatus->sw1 == (uint8_t)SW1_WARNING1 ||
      response->repStatus->sw1 == (uint8_t)SW1_WARNING2)
   {
      FreeCAPDU(tmpCommand);
      CAPDU *cmdGet = MakeCommandC(CMD_GET_RESPONSE, NULL, 0);
      if(cmdGet == NULL)
      {
         FreeRAPDU(response);
         return NULL;
      }
      
      if(response->repStatus->sw1 == (uint8_t)SW1_MORE_DATA)
         cmdGet->cmdHeader->p3 = response->repStatus->sw2;

      return TerminalSendT0CommandR(
              cmdGet, response, inverse_convention, TC1, logger);
   }
   else if(response->repStatus->sw1 == (uint8_t)SW1_WRONG_LENGTH)
   {
      tmpCommand->cmdHeader->p3 = response->repStatus->sw2;
      return TerminalSendT0CommandR(
              tmpCommand, response, inverse_convention, TC1, logger);
   }

   // For any other result we return the APDU, which could be either success or not
   FreeCAPDU(tmpCommand);
   return response;
}

/**
 * This function handles the application selection process,
 * where the first choice is the PSE selection and then
 * a list of AIDs. See EMV 4.2 book 1, page 143.
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param aid the AID of the desired application; pass NULL to use existing list
 * @param autoselect just used for the case of PSE selection: use zero to enable
 * user selection, non-zero to automatically select the first available application
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the FCI Template resulted from application
 * selection or NULL if there is an error. The caller is responsible
 * for eliberating the memory used by the FCI Template.
 */
FCITemplate* ApplicationSelection(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *aid,
        uint8_t autoselect,
        log_struct_t *logger)
{
    CAPDU* command;
    RAPDU* response;
    uint8_t sfi;

    // First try to select using PSE, else use list of AIDs
    command = MakeCommandC(CMD_SELECT, bPSEString, nPSELen);
    if(command == NULL) return NULL;
    response = TerminalSendT0Command(command, convention, TC1, logger);
    if(response == NULL)
    {
        FreeCAPDU(command);
        return NULL;
    }
    FreeCAPDU(command);

    if(response->repStatus->sw1 == 0x90 &&
         response->repStatus->sw2 == 0)
    {
        sfi = GetSFIFromSELECT(response);
        FreeRAPDU(response);
        return SelectFromPSE(convention, TC1, sfi, autoselect, logger);
    }
    else if((response->repStatus->sw1 == 0x6A && response->repStatus->sw2 == 0x82) ||
        (response->repStatus->sw1 == 0x62 && response->repStatus->sw2 == 0x83))
    {
        FreeRAPDU(response);
        return SelectFromAID(convention, TC1, aid, logger);
    }

    FreeRAPDU(response);
    return NULL;
}

/**
 * This function initiates a transaction by sending a
 * GET PROCESSING OPTS command to the card. The caller
 * is responsible for eliberating the memory used by the
 * returned APPINFO.
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param fci the FCI Template returned in application selection
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return an APPINFO cotnaining the AIP and AFL or NULL if
 * an error ocurrs
 */
APPINFO* InitializeTransaction(
        uint8_t convention,
        uint8_t TC1,
        const FCITemplate *fci,
        log_struct_t *logger)
{
    TLV *pdol;
    ByteArray *data;
    CAPDU *command;
    RAPDU *response;
    APPINFO *appInfo;

    pdol = GetPDOL(fci);
    if(pdol == NULL) return NULL;
    pdol->tag1 = 0x83;
    pdol->tag2 = 0;
    data = SerializeTLV(pdol);
    if(data == NULL) return NULL;

    command = MakeCommandC(CMD_GET_PROCESSING_OPTS, data->bytes, data->len);
    FreeByteArray(data);
    if(command == NULL) return NULL;
    response = TerminalSendT0Command(command, convention, TC1, logger);
    if(response == NULL)
    {
        FreeCAPDU(command);
        return NULL;
    }
    FreeCAPDU(command);

    appInfo = ParseApplicationInfo(response->repData, response->lenData);
    FreeRAPDU(response);

    return appInfo;
}

/**
 * This method retrieves all the Data Objects (TLVs) from the card as specified
 * in the APPINFO structure, using READ RECORD commands. If the pointer to a
 * ByteArray structure is non NULL then the offline authentication data is
 * stored at that location
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param appInfo the APPINFO structure that specifies which files to read
 * @param offlineAuthData array of bytes representing the offline authentication
 * data. The user should send an empty but initialized ByteArray if this data
 * is required. This method will ignore any previous contents.
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return a RECORD structure containing all the data objects read or NULL
 * if there are no objects to read or an error ocurrs
 */
RECORD* GetTransactionData(
        uint8_t convention,
        uint8_t TC1,
        const APPINFO* appInfo,
        ByteArray *offlineAuthData,
        log_struct_t *logger)
{
   RECORD *data, *tmp;
   CAPDU *command;
   RAPDU *response;
   AFL* afl;
   uint8_t i, j, k, l;

   if(appInfo == NULL || appInfo->aflList == NULL) return NULL;
   data = (RECORD*)malloc(sizeof(RECORD));
   data->count = 0;
   data->objects = NULL;

   if(offlineAuthData != NULL)
   {
      offlineAuthData->len = 0;
      offlineAuthData->bytes = NULL;
   }

   command = MakeCommandC(CMD_READ_RECORD, NULL, 0);
   if(command == NULL)
   {
      free(data);
      return NULL;
   }

   for(i = 0; i < appInfo->count; i++)
   {
      afl = appInfo->aflList[i];
      if(afl == NULL) continue;
      
      for(j = afl->recordStart; j <= afl->recordEnd; j++)
      {
         command->cmdHeader->p1 = j;
         command->cmdHeader->p2 = (uint8_t)(afl->sfi | 4);
         response = TerminalSendT0Command(command, convention, TC1, logger);
         
         if(response == NULL || response->repStatus->sw1 != 0x90 || 
               response->repStatus->sw2 != 0)
         {
            if(response != NULL) FreeRAPDU(response);
            FreeRECORD(data);
            FreeCAPDU(command);
            return NULL;
         }
         if(response->repData == NULL || response->lenData < 2)
         {
            FreeRAPDU(response);
            continue;
         }

         // If there is data for offline authentication
         if(offlineAuthData != NULL && 
               (afl->recordsOfflineAuth > j - afl->recordStart))
         {
            if(afl->sfi > 0x50) // or ((afl->sfi >> 3) > 10)
            {
               k = response->lenData + offlineAuthData->len;
               offlineAuthData->bytes = 
                  (uint8_t*)realloc(offlineAuthData->bytes, k);
               if(offlineAuthData->bytes != NULL)
               {
                  memcpy(&offlineAuthData->bytes[offlineAuthData->len],
                        response->repData, response->lenData);
                  offlineAuthData->len = k;
               }
               else offlineAuthData->len = 0;
            }
            else
            {
               l = 2; // tag + length
               if(response->repData[1] == EMV_EXTRA_LENGTH_BYTE) l++;
               k = response->lenData - l + offlineAuthData->len;
               offlineAuthData->bytes = (uint8_t*)realloc(offlineAuthData->bytes, k);
               if(offlineAuthData->bytes != NULL)
               {
                  memcpy(&offlineAuthData->bytes[offlineAuthData->len],
                        &response->repData[l], response->lenData - l);
                  offlineAuthData->len += response->lenData - l;
               }
               else offlineAuthData->len = 0;
               
            }
         } // end if(offlineAuthData != NULL ...)

         tmp = ParseRECORD(response->repData, response->lenData);
         FreeRAPDU(response);
         if(AddRECORD(data, tmp))
         {
            FreeRECORD(data);
            FreeCAPDU(command);
            return NULL;
         }
         FreeRECORD(tmp);
      } // end for(j = afl->recordStart; j <= afl->recordEnd; j++)
   } // end for(i = 0; i < appInfo->count; i++)
   
   FreeCAPDU(command);
   return data;
}


/**
 * This function handles the application selection by AID.
 * For the moment it automatically selects the first available
 * application, based on the bAIDList configuration, but
 * this could be improved by actually storing a list of
 * available applications and allowing the user to choose.
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param aid if given will be used as Application ID (AID), else
 * use predefined list. This parameter remains untouched
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the FCI Template resulted from application
 * selection or NULL if there is an error. The caller is responsible
 * for eliberating the memory used by the FCI Template.
 * @sa ApplicationSelection
 */
FCITemplate* SelectFromAID(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *aid,
        log_struct_t *logger)
{
   CAPDU* command;
   RAPDU* response;
   volatile uint8_t i = 0;
   FCITemplate* fci = NULL;

   if(aid != NULL && aid->len == nAIDLen)
   {
      command = MakeCommandC(CMD_SELECT, aid->bytes, nAIDLen);
      if(command == NULL) return NULL;
      response = TerminalSendT0Command(command, convention, TC1, logger);
      if(response == NULL)
      {
          FreeCAPDU(command);
          return NULL;
      }
      FreeCAPDU(command);

      if(response->repStatus->sw1 == 0x90 && response->repStatus->sw2 == 0)
      {
         fci = ParseFCI(response->repData, response->lenData);
         FreeRAPDU(response);
         return fci;
      }

      FreeRAPDU(response);
      return NULL;
   }

   while(i < nAIDEntries)
   {
      command = MakeCommandC(CMD_SELECT, &(bAIDList[nAIDLen * i]), nAIDLen);
      if(command == NULL) return NULL;
      response = TerminalSendT0Command(command, convention, TC1, logger);
      if(response == NULL)
      {
          FreeCAPDU(command);
          return NULL;
      }

      FreeCAPDU(command);
      if(response->repStatus->sw1 == 0x90 && response->repStatus->sw2 == 0)
      {
         fci = ParseFCI(response->repData, response->lenData);
         FreeRAPDU(response);
         return fci;
      }
      else if((response->repStatus->sw1 != 0x6A || response->repStatus->sw2 != 0x82) &&
            (response->repStatus->sw1 != 0x62 || response->repStatus->sw2 != 0x83))
      {
         FreeRAPDU(response);
         return NULL;
      }

      FreeRAPDU(response);
      i++;
   }

   return NULL;
}

/**
 * This function handles the application selection by PSE.
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param sfiPSE the SFI of the PSE as returned from an initial
 * SELECT command
 * @param autoselect if non-zero the first application will be used,
 * else the user will select from a menu (LCD needed).
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the FCI Template resulted from application
 * selection or NULL if there is an error. The caller is responsible
 * for eliberating the memory used by the FCI Template.
 * @sa ApplicationSelection
 */
FCITemplate* SelectFromPSE(
        uint8_t convention,
        uint8_t TC1,
        uint8_t sfiPSE,
        uint8_t autoselect,
        log_struct_t *logger)
{
   FCITemplate* fci = NULL;
   CAPDU* command = NULL;
   RAPDU* response = NULL;
   RECORDList* rlist = NULL;
   TLV* adfName = NULL;
   uint8_t more, status, k, i;
   volatile uint8_t tmp;

   // Get the application list
   rlist = (RECORDList*)malloc(sizeof(RECORDList));
   if(rlist == NULL) goto clean;
   rlist->count = 0;
   rlist->objects = NULL;
   command = MakeCommandC(CMD_READ_RECORD, NULL, 0);
   if(command == NULL) goto clean;
   command->cmdHeader->p1 = 0;
   command->cmdHeader->p2 = (uint8_t)((sfiPSE << 3) | 4);
   more = 1;

   while(more)
   {
      more = 0;
      command->cmdHeader->p1 += 1;
      response = TerminalSendT0Command(command, convention, TC1, logger);
      if(response == NULL) goto clean;
      
      if(response->repData != NULL)
      {
         more = 1;
         status = ParsePSD(rlist, response->repData, response->lenData);
         if(status) goto clean;
      }
      FreeRAPDU(response);
      if(rlist == NULL) goto clean;
   }
   FreeCAPDU(command);
   command = NULL;
   if(rlist->count == 0) goto clean;

   k = 0;
   if(autoselect == 0)
   {
       while(1)
       {
           adfName = rlist->objects[k]->objects[0];
           fprintf(stderr, "%d:", k + 1);
           for(i = 0; i < adfName->len && i < 7; i++)
               fprintf(stderr, "%02X", adfName->value[i]);
           _delay_ms(200);

           do{
               tmp = GetButton();
           }while((tmp == 0));

           if((tmp & BUTTON_C))
           {
               k++;
               if(k == rlist->count) k = 0;
           }
           else 
           {
               break;
           }
       }
   }

   // select application, either by means of user or automatically
   // as here; modify as needed
   adfName = rlist->objects[k]->objects[0];
   command = MakeCommandC(CMD_SELECT, adfName->value, adfName->len);
   if(command == NULL) goto clean;
   response = TerminalSendT0Command(command, convention, TC1, logger);
   FreeCAPDU(command);
   command = NULL;
   if(response == NULL) goto clean;
   if(response->repData != NULL && response->repStatus->sw1 == SW1_COMPLETED)
      fci = ParseFCI(response->repData, response->lenData);
   FreeRAPDU(response);

clean:
   if(command != NULL)
      FreeCAPDU(command);
   if(rlist != NULL)
      FreeRECORDList(rlist);

   return fci;
}

/**
 * This function tests if the specified PIN is accepted by
 * the card using the VERIFY command
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param pin the PIN as a ByteArray having the structure
 * required by the VERIFY command. The data in the ByteArray
 * will be copied as it is on the VERIFY payload
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return 0 if the PIN is correct, non-zero otherwise or if
 * an error ocurred
 */
uint8_t VerifyPlaintextPIN(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *pin,
        log_struct_t *logger)
{
    CAPDU* command;
    RAPDU* response;

    if(pin == NULL || pin->len == 0) return -1;

    command = MakeCommandC(CMD_VERIFY, pin->bytes, pin->len);
    if(command == NULL) return RET_ERROR;
    response = TerminalSendT0Command(command, convention, TC1, logger);
    if(response == NULL)
    {
        FreeCAPDU(command);
        return RET_ERROR;
    }
    FreeCAPDU(command);

    if(response->repStatus == NULL || response->repStatus->sw1 != 0x90 || 
         response->repStatus->sw2 != 0)
    {
        FreeRAPDU(response);
        return RET_ERROR;
    }

    FreeRAPDU(response);
    return 0;
}

/**
 * This function sends a GENERATE AC command to the card
 * with the specified amount and request (ARQC, AAC or TC)
 * Any of the amount parameters can be sent as NULL in which
 * case that field will be filled with zeros
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param cdol the CDOL data object read from the card
 * @param acType the type of Applicatio Cryptogram (AC)
 * requested (see AC_REQ_TYPE)
 * @param params a GENERATE_AC_PARAMS structure containing the
 * data to be sent in the GENERATE AC command. This structure is
 * mandatory for this command, even if some of the fields are unused.
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the response APDU given by the card or NULL if an
 * error ocurred. The caller is responsible for eliberating this
 * memory.
 */
RAPDU* SendGenerateAC(
        uint8_t convention,
        uint8_t TC1,
        AC_REQ_TYPE acType,
        const TLV* cdol,
        const GENERATE_AC_PARAMS *params,
        log_struct_t *logger)
{
    CAPDU* command;
    RAPDU* response;
    uint8_t* data;
    uint8_t i, j, k, len;
    TLV* tlv;

    if(cdol == NULL || params == NULL) return NULL;

    // make the command data to be sent
    data = NULL;
    len = 0;
    i = 0;
    k = 0;
    while(k < cdol->len)
    {
        tlv = ParseTLV(&(cdol->value[k]), cdol->len - k, 0);
        k += 2;
        if(tlv == NULL) continue;
        if(tlv->tag2 != 0) k += 1;
        len += tlv->len;
        data = (uint8_t*)realloc(data, len * sizeof(uint8_t));
        if(data == NULL)
        {
            FreeTLV(tlv);
            return NULL;
        }

        if(tlv->tag1 == 0x9F && tlv->tag2 == 0x02)
        {
            for(j = 0; j < tlv->len && j < sizeof(params->amount); j++)
                data[i++] = params->amount[j];
            while(j < tlv->len)
            {
                 data[i++] = 0;
                 j++;
            }
        }
        else if(tlv->tag1 == 0x9F && tlv->tag2 == 0x03) 
        {
            for(j = 0; j < tlv->len && j < sizeof(params->amountOther); j++)
             data[i++] = params->amountOther[j];
            while(j < tlv->len)
            {
             data[i++] = 0;
             j++;
            }
        }
        else if(tlv->tag1 == 0x9F && tlv->tag2 == 0x1A)
        {
            for(j = 0; j < tlv->len && j < sizeof(params->terminalCountryCode); j++)
                data[i++] = params->terminalCountryCode[j];
             while(j < tlv->len)
             {
                data[i++] = 0;
                j++;
             }
        }
        else if(tlv->tag1 == 0x95)
        {
            for(j = 0; j < tlv->len && j < sizeof(params->tvr); j++)
                data[i++] = params->tvr[j];
            while(j < tlv->len)
            {
                data[i++] = 0;
                j++;
            }
        }
      else if(tlv->tag1 == 0x5F && tlv->tag2 == 0x2A)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->terminalCurrencyCode); j++)
            data[i++] = params->terminalCurrencyCode[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else if(tlv->tag1 == 0x8A)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->arc); j++)
            data[i++] = params->arc[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else if(tlv->tag1 == 0x91)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->IssuerAuthData); j++)
            data[i++] = params->IssuerAuthData[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else if(tlv->tag1 == 0x9A)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->transactionDate); j++)
            data[i++] = params->transactionDate[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else if(tlv->tag1 == 0x9C)
      {
         data[i++] = params->transactionType;
      }
      else if(tlv->tag1 == 0x9F && tlv->tag2 == 0x37)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->unpredictableNumber); j++)
            data[i++] = params->unpredictableNumber[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else if(tlv->tag1 == 0x9F && tlv->tag2 == 0x35)
      {
         data[i++] = params->terminalType;
      }
      else if(tlv->tag1 == 0x9F && tlv->tag2 == 0x45)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->dataAuthCode); j++)
            data[i++] = params->dataAuthCode[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else if(tlv->tag1 == 0x9F && tlv->tag2 == 0x4C)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->iccDynamicNumber); j++)
            data[i++] = params->iccDynamicNumber[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else if(tlv->tag1 == 0x9F && tlv->tag2 == 0x34)
      {
         for(j = 0; j < tlv->len && j < sizeof(params->cvmResults); j++)
            data[i++] = params->cvmResults[j];
         while(j < tlv->len)
         {
            data[i++] = 0;
            j++;
         }
      }
      else  // any other data
      {
         for(j = 0; j < tlv->len; j++)
            data[i++] = 0;
      }

      FreeTLV(tlv);
   } //end while


   command = MakeCommandC(CMD_GENERATE_AC, data, len);
   if(command == NULL) return NULL;
   command->cmdHeader->p1 = (uint8_t)acType;
   response = TerminalSendT0Command(command, convention, TC1, logger);

   FreeCAPDU(command);
   return response;
}

/**
 * This function is used to make the card sign the Dynamic
 * Application Data using the INTERNAL AUTHENTICATE command.
 * This data is composed of some ICC-resident data and the
 * data passed to this methdo. 
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param data ByteArray structure containing the data that shall
 * be passed to the INTERNAL AUTHENTICATE command
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return the response APDU given by the card or NULL if an
 * error ocurred. The caller is responsible for eliberating this
 * memory.
 */
RAPDU* SignDynamicData(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *data,
        log_struct_t *logger)
{
   CAPDU* command;
   RAPDU* response;

   command = MakeCommandC(CMD_INTERNAL_AUTHENTICATE, data->bytes, data->len);
   if(command == NULL) return NULL;
   response = TerminalSendT0Command(command, convention, TC1, logger);
   FreeCAPDU(command);
   return response;
}

/**
 * This method parses a stream of data corresponding to a Payment
 * System Directory (such as the response to a READ RECORD
 * command on the PSE) and adds it to an existing RECORDList structure
 * containing the available applications as RECORD objects.
 * The current implementation only handles a PSD wihtout
 * directories. That means that it will parse only elements 
 * of type ADF, but none of type DDF
 *
 * @param rlist RECORDList structure used to store retrieved ADFs. This
 * structure should be initialized before being passed to this method,
 * which will only allocate memory for new records on the list and update
 * the count
 * @param data the stream to be parsed
 * @param lenData the length of the data to be parsed
 * @return zero if success, non-zero otherwise
 * This function allocates the necessary memory for the RECORDList object. 
 * It is the caller responsability to free that memory after use. If the
 * method is not successful it will return NULL. 
 */
uint8_t ParsePSD(RECORDList* rlist, const uint8_t *data, uint8_t lenData)
{
   RECORD *rec, *adf;
   TLV *obj;
   uint8_t i;

   if(rlist == NULL) return RET_ERROR;
   rec = ParseRECORD(data, lenData);
   if(rec == NULL) return RET_ERROR;

   // parse each adf and put it in the list
   for(i = 0; i < rec->count; i++)
   {
      obj = rec->objects[i];
      adf = ParseManyTLV(obj->value, obj->len);
      if(adf == NULL || obj->tag1 != EMV_TAG1_APPLICATION_TEMPLATE || 
            obj->tag2 != EMV_TAG2_APPLICATION_TEMPLATE)
      {
         FreeRECORD(rec);
         return RET_ERROR;
      }
      
      rlist->count++;
      rlist->objects = (RECORD**)realloc(rlist->objects, rlist->count * sizeof(RECORD*));
      rlist->objects[rlist->count-1] = adf;
   }

   FreeRECORD(rec);
   return RET_SUCCESS;
}

/**
 * This method parses a stream of data corresponding to the
 * response of GET PROCESSING OPTS command, and returns
 * an APPINFO structure containing the AIP and AFL list.
 *
 * @param data the stream to be parsed
 * @param len the length of the data to be parsed
 * @return APPINFO structure containing the AIP and AFL List
 * This function allocates the necessary memory for the APPINFO object. 
 * It is the caller responsability to free that memory after use. If the
 * method is not successful it will return NULL. 
 */
APPINFO* ParseApplicationInfo(const uint8_t* data, uint8_t len)
{
   APPINFO *appInfo = NULL;
   uint8_t i = 0, j, k, t, l, tag;

   if(data == NULL || len < 8) return NULL;

   tag = data[i++];   
   if(tag == 0x80) // remaining data is LEN | AIP | AFL
   {
      k = data[i++];
      if(k == EMV_EXTRA_LENGTH_BYTE) k = data[i++];
      if(len < k + 2) return NULL;
      j = (k - 2) / 4;

      appInfo = (APPINFO*)malloc(sizeof(APPINFO));
      if(appInfo == NULL) return NULL;
      appInfo->count = j;
      appInfo->aip[0] = data[i++];
      appInfo->aip[1] = data[i++];
      appInfo->aflList = (AFL**)malloc(j * sizeof(AFL*));
      if(appInfo->aflList == NULL)
      {
         free(appInfo);
         return NULL;
      }

      for(k = 0; k < j; k++)
      {
         appInfo->aflList[k] = (AFL*)malloc(sizeof(AFL));
         if(appInfo->aflList[k] == NULL)
         {
            FreeAPPINFO(appInfo);
            return NULL;
         }
         appInfo->aflList[k]->sfi = data[i++];
         appInfo->aflList[k]->recordStart = data[i++];
         appInfo->aflList[k]->recordEnd = data[i++];
         appInfo->aflList[k]->recordsOfflineAuth = data[i++];
      }
   } // end if t = 0x88
   else if(tag == 0x77) // constructed BER-TLV object
   {
      k = data[i++];
      j = 2;
      if(k == EMV_EXTRA_LENGTH_BYTE)
      {
         k = data[i++];
         j++;
      }
      if(len < k + j) return NULL;

      appInfo = (APPINFO*)malloc(sizeof(APPINFO));
      if(appInfo == NULL) return NULL;
      appInfo->count = 0;
      appInfo->aflList = NULL;

      while(i - j < k)
      {
         tag = data[i++];
         if(tag == 0x82)  // AIP
         {
            l = data[i++];
            if(l != 2)
            {
               FreeAPPINFO(appInfo);
               return NULL;
            }
            appInfo->aip[0] = data[i++];
            appInfo->aip[1] = data[i++];
         }
         else if(tag == 0x94) // AFL
         {
            l = data[i++];
            if(l == EMV_EXTRA_LENGTH_BYTE) l = data[i++];
            l = l / 4;
            appInfo->count = l;
            if(len > 0) appInfo->aflList = (AFL**)malloc(l * sizeof(AFL*));
            if(appInfo->aflList == NULL)
            {
               free(appInfo);
               return NULL;
            }

            for(t = 0; t < l; t++)
            {
               appInfo->aflList[t] = (AFL*)malloc(sizeof(AFL));
               if(appInfo->aflList[t] == NULL)
               {
                  FreeAPPINFO(appInfo);
                  return NULL;
               }
               appInfo->aflList[t]->sfi = data[i++];
               appInfo->aflList[t]->recordStart = data[i++];
               appInfo->aflList[t]->recordEnd = data[i++];
               appInfo->aflList[t]->recordsOfflineAuth = data[i++];
            }
         }
         else i++;
      }

   } // end else if t = 0x94

   return appInfo;
}

/**
 * Parses the SFI value of the main directory (PSE) from the
 * response of the first SELECT command (app selection)
 *
 * @param response RAPDU containing the response to a SELECT
 * command
 * @return SFI value or 0 if error
 */
uint8_t GetSFIFromSELECT(const RAPDU *response)
{
   uint8_t sfi = 0, i = 0;

   if(response == NULL || response->repData == NULL)
      return 0;

   while (i < response->lenData && response->repData[i] != 0x88) i++;
           
   if (i < response->lenData - 2) sfi = response->repData[i + 2];
                          
   return sfi;
}

/**
 * Returns the PDOL object (TLV) from a FCI such as the one
 * returned in application selection
 *
 * @param fci FCI Template to search for the PDOL
 * @returns a pointer to the TLV representing the PDOL or 
 * NULL if the PDOL was not found or an error ocurred
 */
TLV* GetPDOLFromFCI(const FCITemplate *fci)
{
   uint8_t i;
   TLV *tmp;

   if(fci == NULL || fci->fciData == NULL || 
      fci->fciData->count == 0 || fci->fciData->objects == NULL)
         return NULL;

   for(i = 0; i < fci->fciData->count; i++)
   {
      tmp = fci->fciData->objects[i];
      if(tmp != NULL && tmp->tag1 == 0x9F && tmp->tag2 == 0x38)
         return tmp;
   }

   return NULL;
}

/**
 * This method returns a TLV object containing a PDOL
 * from either a FCI Template if the PDOL is available
 * or makes a default one if the FCI Template is NULL
 * or does not contain a PDOL. This method returns
 * the tag as given in the FCI Template (usually '9F38').
 * In order to use it for GET_PROCESSING_OPTS you need
 * to change to tag to '83'.
 *
 * @param fci FCI Template containing or not a PDOL. If
 * NULL then a default PDOL will be returned
 * @return TLV object contianing a PDOL
 * @sa GetPDOLFromFCI
 */
TLV* GetPDOL(const FCITemplate *fci)
{
   TLV *pdol = NULL;

   if(fci != NULL) pdol = GetPDOLFromFCI(fci);
   if(pdol == NULL)
   {
      pdol = (TLV*)malloc(sizeof(TLV));
      if(pdol == NULL) return NULL;
      pdol->value = NULL;
      pdol->len = 0;
      pdol->tag1 = 0x9F;
      pdol->tag2 = 0x38;
   }

   return pdol;
}

/**
 * This function returns the specified primitive data object
 * (see enum CARD_PDO) from the card using the GET DATA command.
 *
 * @param convention parameter from ATR
 * @param TC1 parameter from ATR
 * @param pdo the kind of data object you want to retrieve
 * (see CARD_PDO)
 * @param logger a pointer to a log structure or NULL if no log is desired
 * @return a ByteArray structure containing the data retrieved
 * or NULL if no data is available or there is an error. The
 * caller is responsible to release the memory used by the
 * ByteArray structure
 */
ByteArray* GetDataObject(
        uint8_t convention,
        uint8_t TC1,
        CARD_PDO pdo,
        log_struct_t *logger)
{
    ByteArray* data = NULL;
    CAPDU* command;
    RAPDU* response;
    TLV* tlv;

    command = MakeCommandC(CMD_GET_DATA, NULL, 0);
    if(command == NULL) return NULL;
    command->cmdHeader->p1 = 0x9F;
    command->cmdHeader->p2 = (uint8_t)pdo;
    response = TerminalSendT0Command(command, convention, TC1, logger);
    if(response == NULL)
    {
        FreeCAPDU(command);
        return NULL;
    }
    FreeCAPDU(command);

    if(response->repData == NULL)
    {
        FreeRAPDU(response);
        return NULL;
    }

    tlv = ParseTLV(response->repData, response->lenData, 1);
    FreeRAPDU(response);
    if(tlv == NULL) return NULL;
    data = (ByteArray*)malloc(sizeof(ByteArray));
    data->len = tlv->len;
    data->bytes = NULL;
    if(tlv->len > 0 && tlv->value != NULL)
    {
        data->bytes = (uint8_t*)malloc(data->len * sizeof(uint8_t));
        if(data->bytes == NULL)
        {
            free(data);
            return NULL;
        }
        memcpy(data->bytes, tlv->value, data->len);
    }

    return data;
}

/**
 * This function parses a stream of data and returns a FCI Template
 * if the data is valid
 * 
 * @param data stream of bytes to be parsed
 * @param lenData total length in bytes of data
 * @return the parsed FCITemplate object. This function allocates the necessary
 * memory for the FCITemplate object. It is the caller responsability to free
 * that memory after use.
 */     
FCITemplate* ParseFCI(const uint8_t *data, uint8_t lenData)
{
   FCITemplate *fci;
   uint8_t i, len;

   if(data == NULL || lenData == 0)
      return NULL;

   i = 0;
   if(lenData < 4 || data[i++] != 0x6F) return NULL;
   len = data[i++];
   if(len == EMV_EXTRA_LENGTH_BYTE) len = data[i++];
   if(len > lenData - 2) return NULL;

   // get DF Name
   if(data[i++] != 0x84) return NULL;
   len = data[i++];
   if(len == EMV_EXTRA_LENGTH_BYTE) len = data[i++];
   if(len > lenData - 4) return NULL;

   fci = (FCITemplate*)malloc(sizeof(FCITemplate));
   if(fci == NULL) return NULL;
   fci->dfName = (uint8_t*)malloc(len * sizeof(uint8_t));
   if(fci->dfName == NULL)
   {
      free(fci);
      return NULL;
   }
   memcpy(fci->dfName, &data[i], len);
   fci->lenDFName = len;
   i += len;

   // get FCI Proprietary template (sfi, app label, etc...)
   if(data[i++] != 0xA5)
   {
      FreeFCITemplate(fci);
      return NULL;
   }
   len = data[i++];
   if(len == EMV_EXTRA_LENGTH_BYTE) len = data[i++];
   if(len > (lenData - fci->lenDFName))
   {
      FreeFCITemplate(fci);
      return NULL;
   }
   fci->fciData = ParseManyTLV(&data[i], len);
   if(fci->fciData == NULL)
   {
      FreeFCITemplate(fci);
      return NULL;
   }

   return fci;
}

/**
 * This function parses a stream of data and returns a TLV object if
 * the data contains a valid BER-TLV object
 * 
 * @param data stream of bytes to be parsed
 * @param lenData total length in bytes of data
 * @param includeValue if this parameter is 0 then only the tag and
 * length of the TLV are parsed (useful for Data Object Lists). If
 * this parameter is non-zero then also the value of the TLV is returned
 * @return the parsed TLV object. This function allocates the necessary
 * memory for the TLV object. It is the caller responsability to free
 * that memory after use.
 */     
TLV* ParseTLV(const uint8_t *data, uint8_t lenData, uint8_t includeValue)
{       
   TLV* obj = NULL;
   uint8_t i = 0, j;

   obj = (TLV*)malloc(sizeof(TLV));
   if(obj == NULL) return NULL;
   obj->value = NULL;

   obj->tag1 = data[i++];
   if((obj->tag1 & 0x1F) == 0x1F)
          obj->tag2 = data[i++];
   else
          obj->tag2 = 0;

   obj->len = data[i++];
   if(obj->len == EMV_EXTRA_LENGTH_BYTE) obj->len = data[i++];  // for len > 127

   if(includeValue != 0)
   {
      if(obj->len > lenData - 2)
      {
         free(obj);
         return NULL;
      }

      obj->value = (uint8_t*)malloc(obj->len*sizeof(uint8_t));
      if(obj->value == NULL)
      {
            free(obj);              
            return NULL;
      }

      for(j = 0; j < obj->len; j++)
            obj->value[j] = data[i++];
   }

   return obj;
}

/**
 * This function copies the contents of a TLV into a new TLV structure
 * 
 * @param data the TLV to be copied
 * @return the new TLV object or NULL if there is an error. This function
 * allocates the necessary memory for the TLV object. It is the caller
 * responsability to free that memory after use.
 */     
TLV* CopyTLV(const TLV *data)
{
   TLV *clone;

   if(data == NULL) return NULL;

   clone = (TLV*)malloc(sizeof(TLV));
   if(clone == NULL) return NULL;
   clone->len = 0;
   clone->tag1 = data->tag1;
   clone->tag2 = data->tag2;
   clone->value = NULL;

   if(data->value != NULL && (data->len > 0))
   {
      clone->value = (uint8_t*)malloc(data->len * sizeof(uint8_t));
      if(clone->value == NULL)
      {
         free(clone);
         return NULL;
      }
      memcpy(clone->value, data->value, data->len);
      clone->len = data->len;
   }

   return clone;
}

/**
 * This method parses a stream of data representing a constructed
 * BER-TLV, such as the response to a
 * READ RECORD command, and fills a RECORD structure
 *
 * @param data the stream to be parsed
 * @param lenData the length of the data to be parsed
 * @return RECORD structure containing the TLV objects parsed
 * This function allocates the necessary memory for the RECORD object. 
 * It is the caller responsability to free that memory after use. If the
 * method is not successful it will return NULL. 
 */
RECORD* ParseRECORD(const uint8_t *data, uint8_t lenData)
{
   uint8_t i, len;

   if(data == NULL || lenData == 0)
      return NULL;

   i = 0;
   if(lenData < 4 || data[i++] != 0x70) return NULL;
   len = data[i++];
   if(len == EMV_EXTRA_LENGTH_BYTE) len = data[i++];
   if(len > lenData - 2) return NULL;

   return ParseManyTLV(&data[i], len);
}

/**
 * This method copies the contents of the RECORD src
 * after the existing contents of the RECORD dest.
 *
 * @param dest the RECORD in which the new data from src
 * will be appended
 * @param src the RECORD containing the data to be copied
 * on dest
 * @return 0 if successful, non-zero otherwise
 * This function allocates the necessary memory for the extra data
 * needed in the dest RECORD object. If there is not sufficient
 * memory (indicated by a non-zero return value) then the contents
 * of this RECORD will be truncated or NULL.
 */
uint8_t AddRECORD(RECORD *dest, const RECORD *src)
{
   uint8_t i, k;

   if(dest == NULL || src == NULL || src->objects == NULL) return RET_ERROR;

   k = dest->count + src->count;
   dest->objects = (TLV**)realloc(dest->objects, k * sizeof(TLV*));
   if(dest->objects == NULL)
      return RET_ERROR;

   for(i = 0; i < src->count; i++)
   {
      dest->objects[dest->count] = CopyTLV(src->objects[i]);
      if(dest->objects[dest->count] == NULL) return RET_ERROR;
      dest->count++;
   }

   return 0;
}

/**
 * This method can be used to find a TLV within a RECORD structure.
 *
 * @param rec the RECORD structure to be searched for the TLV
 * @param tag1 the first (or only) tag of the interested TLV
 * @param tag2 the second tag of the interested TLV. This is to
 * be used only in cases where the TLV we are looking for
 * has a 2-byte tag. If the tag is only 1 byte then put 0 here.
 * @return a const pointer to the TLV within the RECORD structure
 * or NULL if the TLV cannot be found or some error occurs
 *
 */
TLV* GetTLVFromRECORD(RECORD *rec, uint8_t tag1, uint8_t tag2)
{
   TLV *tlv;
   uint8_t i;

   if(rec == NULL) return NULL;
   for(i = 0; i < rec->count; i++)
   {
      tlv = rec->objects[i];
      if(tlv != NULL && tlv->tag1 == tag1 && tlv->tag2 == tag2)
         return tlv;
   }

   return NULL;
}

/**
 * This method parses a stream of data containing several
 * concatenated TLV objects and fills a RECORD structure
 *
 * @param data the stream to be parsed
 * @param lenData the length of the data to be parsed
 * @return RECORD structure containing the TLV objects parsed
 * This function allocates the necessary memory for the RECORD object. 
 * It is the caller responsability to free that memory after use. If the
 * method is not successful it will return NULL. 
 */
RECORD* ParseManyTLV(const uint8_t *data, uint8_t lenData)
{
   RECORD *rec;
   TLV *obj;
   uint8_t i;

   if(data == NULL || lenData == 0)
      return NULL;

   rec = (RECORD*)malloc(sizeof(RECORD));
   if(rec == NULL) return NULL;
   rec->count = 0;
   rec->objects = NULL;
   i = 0;

   while(i < lenData)
   {
      obj = ParseTLV(&(data[i]), lenData - i, 1);
      if(obj == NULL)
      {
        FreeRECORD(rec);
        return NULL;
      }
      i += 2 + obj->len;
      if(obj->tag2 != 0) i++;
      if(obj->len > 127) i++;

      rec->count++;
      rec->objects = (TLV**)realloc(rec->objects, rec->count * sizeof(TLV*));
      rec->objects[rec->count-1] = obj;
   }

   return rec;
}


/** 
 * This function parses a RECORD structure and searches for the CDOL1
 * tag. If found, it searches for the position of the Authorized Amount
 * value inside the CDOL1. This function is used in applications like
 * FilterGenerateAC to determine in which place is sent the amount
 * of a transaction
 *
 * @param record RECORD structure to be parsed
 * @return the position (starting at 1) of the Authorized Amount value
 * inside CDOL1 if found, 0 if unsuccessful
 */
uint8_t AmountPositionInCDOLRecord(const RECORD *record)
{
   uint8_t i;
   TLV *cdol1 = NULL, *obj;

   if(record == NULL) return 0;

   for(i = 0; i < record->count; i++)
          if(record->objects[i] != NULL && record->objects[i]->tag1 == 0x8C)
          {
                  cdol1 = record->objects[i];
                  break;
          }

   if(cdol1 == NULL) return 0;

   i = 0;
   while(i < cdol1->len)
   {
          obj = ParseTLV(&(cdol1->value[i]), cdol1->len - i, 0);
          if(obj == NULL) return 0;

          if(obj->tag1 == 0x9F && obj->tag2 == 0x02)
          {
                  free(obj);
                  return i + 1;
          }

          i += 2;
          if(obj->tag2 != 0) i++;

          free(obj);
   }

   return 0;
}

/**
 * This method converts a TLV object into a stream of bytes,
 * copying the contents of the TLV into the ByteArray.
 * The caller is responsible for eliberating the memory used
 * by the ByteArray returned.
 *
 * @param tlv TLV object to serialize
 * @return the stream of bytes representing the TLV
 */
ByteArray* SerializeTLV(const TLV* tlv)
{
   ByteArray *stream;
   uint8_t *data;
   uint8_t len, i;

   if(tlv == NULL) return NULL;

   len = 1; // first tag
   if(tlv->tag2 != 0) len++;
   len++; // first len byte
   if(tlv->len > 127) len++;
   if(tlv->value != NULL) len += tlv->len;
   data = (uint8_t*)malloc(len * sizeof(uint8_t));
   if(data == NULL) return NULL;

   i = 0;
   data[i++] = tlv->tag1;
   if(tlv->tag2 != 0) data[i++] = tlv->tag2;
   if(tlv->len > 127) data[i++] = EMV_EXTRA_LENGTH_BYTE;
   data[i++] = tlv->len;
   if(tlv->value != NULL && tlv->len != 0)
      memcpy(&data[i], tlv->value, tlv->len);

   stream = MakeByteArray(data, len);
   if(stream == NULL)
   {
      free(data);
      return NULL;
   }

   return stream;
}

/**
 * Eliberates the memory used by a TLV structure
 *
 * @param data the TLV structure to be erased
 */
void FreeTLV(TLV *data)
{
   if(data == NULL) return;

   if(data->value != NULL)
   {
      free(data->value);
      data->value = NULL;
   }
   free(data);
}

/**
 * Eliberates the memory used by a RECORD structure
 *
 * @param data the RECORD structure to be erased
 */
void FreeRECORD(RECORD *data)
{
   uint8_t i;

   if(data == NULL) return;

   if(data->objects != NULL)
   {
      for(i = 0; i < data->count; i++)
         if(data->objects[i] != NULL)
         {
            FreeTLV(data->objects[i]);
            data->objects[i] = NULL;
         }
      free(data->objects);
      data->objects = NULL;
   }
   free(data);
}

/**
 * Eliberates the memory used by a RECORDList structure
 *
 * @param data the RECORDList structure to be erased
 */
void FreeRECORDList(RECORDList *data)
{
   uint8_t i = 0;

   if(data->objects != NULL)
   {
      for(i = 0; i < data->count; i++)
         if(data->objects[i] != NULL)
         {
            FreeRECORD(data->objects[i]);
            data->objects[i] = NULL;
         }
      free(data->objects);
      data->objects = NULL;
   }
   free(data);
}

/**
 * Eliberates the memory used by a FCITemplate structure
 *
 * @param data the FCITemplate structure to be erased
 */
void FreeFCITemplate(FCITemplate *data)
{
   if(data->fciData != NULL)
   {
      FreeRECORD(data->fciData);
      data->fciData = NULL;
   }
   if(data->dfName != NULL)
   {
      free(data->dfName);
      data->dfName = NULL;
   }
   free(data);
}

/**
 * Eliberates the memory used by a FCIList structure
 *
 * @param data the FCIList structure to be erased
 */
void FreeFCIList(FCIList *data)
{
   uint8_t i = 0;

   if(data->objects != NULL)
   {
      for(i = 0; i < data->count; i++)
         if(data->objects[i] != NULL)
         {
            FreeFCITemplate(data->objects[i]);
            data->objects[i] = NULL;
         }
      free(data->objects);
      data->objects = NULL;
   }
   free(data);
}

/**
 * Eliberates the memory used by an APPINFO structure
 *
 * @param data the APPINFO structure to be erased
 */
void FreeAPPINFO(APPINFO *data)
{
   uint8_t i;

   if(data == NULL) return;

   if(data->aflList != NULL)
   {
      for(i = 0; i < data->count; i++)
         if(data->aflList[i] != NULL)
         {
            free(data->aflList[i]);
            data->aflList[i] = NULL;
         }
      free(data->aflList);
      data->aflList = NULL;
   }
   free(data);
}


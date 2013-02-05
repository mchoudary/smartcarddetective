/**
 * \file
 * \brief terminal.h Header file
 *
 * Contains definitions of functions used by a terminal application
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


#ifndef _TERMINAL_H_
#define _TERMINAL_H_

/// Maximum number of command-response pairs recorded when logging
#define MAX_EXCHANGES 50

/* Global external variables */
extern CRP* transactionData[MAX_EXCHANGES];     // used to log data
extern uint8_t nTransactions;                   // used to log data
extern uint8_t nCounter;                        // number of transactions

// -------------------------------------------------------------------
// Structures and enums used by the terminal


/**
 * Enum defining the different primitive data objects retrieved
 * using the GET DATA command
 */
typedef enum {
   PDO_ATC = 0x36,
   PDO_LAST_ATC = 0x13,
   PDO_PIN_TRY_COUNTER = 0x17,
   PDO_LOG_FORMAT = 0x4F
} CARD_PDO;

/**
 * Enum defining the types of GENERATE AC requests available
 */
typedef enum{
   AC_REQ_AAC = 0,
   AC_REQ_ARQC = 0x80,
   AC_REQ_TC = 0x40
} AC_REQ_TYPE;

/**
 * Structure defining a BER-TLV object
 */
typedef struct {
        uint8_t tag1;
        uint8_t tag2;
        uint8_t len;
        uint8_t *value;
} TLV;
   
/**
 * Structure defining a record (constructed BER-TLV object)
 */
typedef struct {
        uint8_t count;
        TLV **objects;
} RECORD;

/**
 * Structure representing a FCI template
 */
typedef struct {
   uint8_t lenDFName;
   uint8_t* dfName;
   RECORD* fciData;
} FCITemplate;
   
/**
 * Structure to hold multiple FCI Template objects
 */
typedef struct {
   uint8_t count;
   FCITemplate** objects;
} FCIList;

/**
 * Structure used to hold multiple RECORD objects
 */
typedef struct {
   uint8_t count;
   RECORD** objects;
} RECORDList;

/**
 * Structure representing an Application File Locator (AFL)
 */
typedef struct {
   uint8_t sfi;
   uint8_t recordStart;
   uint8_t recordEnd;
   uint8_t recordsOfflineAuth;
} AFL;

/**
 * Structure representing the AFL list and AIP
 */
typedef struct {
   uint8_t count;
   uint8_t aip[2];
   AFL** aflList;
} APPINFO;
   
/**
 * Structure used to transmit data to a GENERATE AC
 * command (based on CDOL1 and CDOL2).
 *
 * Some information is available here:
 * http://www.xenco.co.uk/e-manual/xcas-cfg.htm
 * and of course in the EMV specification.
 */
typedef struct {
   uint8_t amount[6];                  // tag 0x9F02
   uint8_t amountOther[6];             // tag 0x9F03
   uint8_t terminalCountryCode[2];     // tag 0x9F1A
   uint8_t tvr[5];                     // tag 95
   uint8_t terminalCurrencyCode[2];    // tag 0x5F2A
   uint8_t transactionDate[3];         // tag 0x9A
   uint8_t transactionType;            // tag 0x9C
   uint8_t unpredictableNumber[4];     // tag 0x9F37
   uint8_t terminalType;               // tag 0x9F35
   uint8_t dataAuthCode[2];            // tag 0x9F45
   uint8_t iccDynamicNumber[8];        // tag 0x9F4C
   uint8_t cvmResults[3];              // tag 0x9F34
   uint8_t arc[2];                     // tag 0x8A
   uint8_t IssuerAuthData[8];          // tag 0x91
} GENERATE_AC_PARAMS;


// -------------------------------------------------------------------
// Methods used by the terminal application

/// This function sends a T=0 command from the terminal to the ICC
RAPDU* TerminalSendT0Command(
        CAPDU* cmd,
        uint8_t inverse_convention,
        uint8_t TC1,
        log_struct_t *logger);

/// Starts the application selection process
FCITemplate* ApplicationSelection(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *aid,
        uint8_t autoselect,
        log_struct_t *logger);

/// Initialize a transaction by sending GET PROCESSING OPTS command
APPINFO* InitializeTransaction(
        uint8_t convention,
        uint8_t TC1,
        const FCITemplate *fci,
        log_struct_t *logger);

/// Retrieves all the Data Objects from the card
RECORD* GetTransactionData(
        uint8_t convention,
        uint8_t TC1,
        const APPINFO* appInfo,
        ByteArray *offlineAuthData,
        log_struct_t *logger);

/// Selects application based on AID list
FCITemplate* SelectFromAID(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *aid,
        log_struct_t *logger);

/// Selects application based on PSE
FCITemplate* SelectFromPSE(
        uint8_t convention,
        uint8_t TC1,
        uint8_t sfiPSE,
        uint8_t autoselect,
        log_struct_t *logger);

/// Checks if the specified PIN is accepted by the card
uint8_t VerifyPlaintextPIN(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *pin,
        log_struct_t *logger);

/// Send a GENERATE AC request with the specified amounts
RAPDU* SendGenerateAC(
        uint8_t convention,
        uint8_t TC1,
        AC_REQ_TYPE acType,
        const TLV* cdol,
        const GENERATE_AC_PARAMS *params,
        log_struct_t *logger);

/// Sign the Dynamic Application Data using INTERNAL AUTHENTICATE
RAPDU* SignDynamicData(
        uint8_t convention,
        uint8_t TC1,
        const ByteArray *data,
        log_struct_t *logger);

/// Returns the SFI value from the response to a SELECT command
uint8_t GetSFIFromSELECT(const RAPDU *response);

/// Returns the PDOL TLV from a FCI
TLV* GetPDOLFromFCI(const FCITemplate *fci);

/// Returns a PDOL TLV from a FCI or a default one
TLV* GetPDOL(const FCITemplate *fci);

/// Return the specified primitive data object from the card
ByteArray* GetDataObject(
        uint8_t convention,
        uint8_t TC1,
        CARD_PDO pdo,
        log_struct_t *logger);

/// Returns a TLV from a RECORD based on its tag
TLV* GetTLVFromRECORD(RECORD *rec, uint8_t tag1, uint8_t tag2);

/// Get the position of the Authorized Amount value inside CDOL1 if exists
uint8_t AmountPositionInCDOLRecord(const RECORD *record);

/// Parse a FCI Template object from a data stream
FCITemplate* ParseFCI(const uint8_t *data, uint8_t lenData);

/// Parse a TLV object from a data stream
TLV* ParseTLV(const uint8_t *data, uint8_t lenData, uint8_t includeValue);

/// Makes a copy of a TLV
TLV* CopyTLV(const TLV *data);

/// Parse a record from a stream of data
RECORD* ParseRECORD(const uint8_t *data, uint8_t lenData);

/// Adds the contents of a record at the end of another one
uint8_t AddRECORD(RECORD *dest, const RECORD *src);

/// Parses a data stream containing many TLV objects
RECORD* ParseManyTLV(const uint8_t *data, uint8_t lenData);

/// Extract the available applications from a Payment System Directory
uint8_t ParsePSD(RECORDList *rlist, const uint8_t *data, uint8_t lenData);

/// Returns the AIP and AFL List from the response of GET PROCESSING OPTS
APPINFO* ParseApplicationInfo(const uint8_t* data, uint8_t len);

/// Get a stream of bytes representing a TLV
ByteArray* SerializeTLV(const TLV* tlv);

/// Eliberates the memory used by a TLV structure
void FreeTLV(TLV *data);

/// Eliberates the memory used by a RECORD structure
void FreeRECORD(RECORD *data);

/// Eliberates the memory used by a RECORDList structure
void FreeRECORDList(RECORDList *data);

/// Eliberates the memory used by a FCITemplate structure
void FreeFCITemplate(FCITemplate *data);

/// Eliberates the memory used by a FCIList structure
void FreeFCIList(FCIList *data);

/// Eliberates the memory used by an APPINFO structure
void FreeAPPINFO(APPINFO *data);

#endif // _TERMINAL_H_


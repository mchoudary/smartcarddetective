import binascii

TO_CARD=1
TO_TERMINAL=2

## Invalid command, used to signal card removal
## CLA = 0x6b (in RFU region)
## INS = 0x99 (0x9X is not allowed)
## P1, P2, P3 = 0x00 (May be useful later)
escape_command = '\x6b\x99\x00\x00\x00'

command={
    "0xc0": (TO_TERMINAL, "READ MORE"),
    "8x1e": (TO_CARD, "APPLICATION BLOCK"),
    "8x18": (TO_CARD, "APPLICATION UNBLOCK"),
    "8x16": (TO_CARD, "CARD BLOCK"),
    "0x82": (TO_CARD,     "EXTERNAL AUTHENTICATE"),
    "8xae": (TO_CARD,     "GENERATE AC"),
    "0x84": (TO_TERMINAL, "GET CHALLENGE"),
    "8xca": (TO_TERMINAL, "GET DATA"),
    "8xa8": (TO_CARD,     "GET PROCESSING OPTIONS"),
    "0x88": (TO_CARD, "INTERNAL AUTHENTICATE"),
    "8x24": (TO_CARD, "PIN CHANGE/UNBLOCK"),
    "0xb2": (TO_TERMINAL, "READ RECORD"),
    "0xa4": (TO_CARD,     "SELECT/READ FILE"),
    "0x20": (TO_CARD,     "VERIFY"),
    "4x41": (TO_CARD,     "TEST A TO CARD"),
    "4x42": (TO_TERMINAL, "TEST B TO TERM")
    }

response={
    "61xx": "More data available",
    "63cx": "NV changed",
    "6cxx": "Wrong length",

## From http://www.cardwerk.com/smartcards/smartcard_standard_ISO7816-4_5_basic_organizations.aspx
    ##
    ## Normal processing
    ##
    "9000": "OK",

    ##
    ## Warning processing
    ##
    ## State of non-volatile memory unchanged
    "6200": "Warning: state of non-volatile memory unchanged",
    "6281": "Warning: part of returned data may be corrupted",
    "6282": "Warning: End of file/record reached before reading Le bytes",
    "6283": "Selected file invalidated",
    "6284": "FCI not formatted according to 1.1.5",
    ## State of non-volatile memory changed
    "6300": "Warning: state of non-volatile memory changed",
    "6381": "Warning: file filled up by the last write",
    ## 63CX is a special case (counter provided by X)

    ##
    ## Execution error
    ##
    ## State of non-volatile memory unchanged
    "6400": "Error: state of non-volatile memory unchanged",
    ## State of non-volatile memory changed
    "6500": "Error: state of non-volatile memory changed",
    "6581": "Error: memory failure",
    ## 66XX is a special case (reserved for security-related issues)

    ##
    ## Checking error
    ##
    "6700": "Wrong length",
    ## Functions in CLA not supported 
    "6800": "Checking error: functions in CLA not supported",
    "6881": "Checking error: logical channel not supported",
    "6882": "Checking error: secure messaging not supported",
    ## Command not allowed
    "6900": "Checking error: command not allowed",
    "6981": "Checking error: Command incompatible with file structure",
    "6982": "Checking error: security status not satisfied",
    "6983": "Checking error: authentication method blocked",
    "6984": "Checking error: referenced data invalidated",
    "6985": "Checking error: conditions of use not satisfied",
    "6986": "Checking error: command not allowed (no current EF)",
    "6987": "Checking error: expected SM data objects missing",
    "6988": "Checking error: SM data objects incorrect",
    ## Wrong parameter(s) P1-P2
    "6a00": "Checking error: wrong parameters P1-P2 (a)",
    "6a80": "Checking error: incorrect parameters in the data field",
    "6a81": "Checking error: function not supported",
    "6a82": "Checking error: file not found",
    "6a83": "Checking error: record not found",
    "6a84": "Checking error: not enough memory space in the file",
    "6a85": "Checking error: Lc inconsistent with TLV structure",
    "6a86": "Checking error: wrong parameters P1-P2 (b)",
    "6a87": "Checking error: Lc inconsistent with P1-P2",
    "6a88": "Checking error: referenced data not found",
    "6b00": "Checking error: wrong parameters P1-P2 (c)",
    ## 6CXX is a special case (Wrong length Le: SW2 indicates the exact length)
    "6d00": "Instruction code not supported or invalid",
    "6e00": "Class not supported",
    "6f00": "No precise diagnosis"
    }
    
def explain_response(response):
    code = response[-4:].upper()

    ## Check for special cases
    if code.startswith("63C"):
        meaning = "Warning: counter is 0x0%s"%code[3]
    elif code.startswith("61"):
        meaning = "OK: 0x%s bytes available"%code[2:4]
    elif code.startswith("66"):
        meaning = "Security error 0x%s"%code[2:4]
    elif code.startswith("6C"):
        meaning = "Checking error: wrong length, should be 0x%s"%code[2:4]
    elif response_codes.has_key(code):
        meaning = response_codes[code]
    else:
        meaning = "Unknown response"

    return meaning + " (0x%s)"%code
    
def to_card(com):
    '''Return True if card data direction is terminal -> card'''

    ## Replace 2nd nibble with "x" for matching
    s=binascii.b2a_hex(com[:2])
    
    if len(s)<4:
        return True
    
    template = s[0]+"x"+s[2:]
    ## Find command
    if command.has_key(template):
        ## Command found, return direction
        return command[template][0] == TO_CARD
    else:
        ## Command not found, assume terminal -> card
        print "WARNING: Failed to find command", s
        return True
    
def command_name(s):
    template = s[0]+"x"+s[2:]
    if command.has_key(template):
        return "("+command[template][1]+")"
    else:
        return ""

def response_name(r):
    for i in range(3):
        template=r[:4-i]+"x"*i
        if response.has_key(template):
            return "("+response[template]+")"
    return ""
        

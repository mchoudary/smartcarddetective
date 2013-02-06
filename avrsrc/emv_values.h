/**
 * \file
 * \brief emv_values.h Header file
 *
 * Contains definitions of bytes used in EMV
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

#ifndef _EMV_VALUES_H_
#define _EMV_VALUES_H_


// -------------------------------------------------------------------
// Structures definining the different values used by EMV

/**
 * Enum defining the different values for byte 1 of tag
 */
typedef enum {
    EMV_TAG1_CDOL_1 = 0x8C,
    EMV_TAG1_CDOL_2 = 0x8D,
    EMV_TAG1_CA_PK_INDEX = 0x8F,
    EMV_TAG1_ISSUER_PK_CERT = 0x90,
    EMV_TAG1_ISSUER_PK_REMINDER = 0x92,
    EMV_TAG1_ISSUER_PK_EXPONENT = 0x9F,
    EMV_TAG1_SIGNED_STATIC_DATA = 0x93,
    EMV_TAG1_APPLICATION_TEMPLATE = 0x61
}EMV_TAG_BYTE1;

/**
 * Enum defining the different values for byte 2 of tag
 */
typedef enum {
    EMV_TAG2_CDOL_1 = 0,
    EMV_TAG2_CDOL_2 = 0,
    EMV_TAG2_CA_PK_INDEX = 0,
    EMV_TAG2_ISSUER_PK_CERT = 0,
    EMV_TAG2_ISSUER_PK_REMINDER = 0,
    EMV_TAG2_ISSUER_PK_EXPONENT = 0x32,
    EMV_TAG2_SIGNED_STATIC_DATA = 0,
    EMV_TAG2_APPLICATION_TEMPLATE = 0
}EMV_TAG_BYTE2;


/**
 * Enum defining the different types of values for the status byte SW1
 */
typedef enum {
    SW1_COMPLETED = 0x90,
    SW1_MORE_TIME = 0x60,
    SW1_MORE_DATA = 0x61,
    SW1_WARNING1 = 0x62,
    SW1_WARNING2 = 0x63,
    SW1_WRONG_LENGTH = 0x6C
}APDU_RESPONSE;

#endif // _EMV_VALUES_H_

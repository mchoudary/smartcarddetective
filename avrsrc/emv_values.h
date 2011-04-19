/** \file
 *      \brief emv_values.h Header file
 *
 *  Contains definitions of bytes used in EMV
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

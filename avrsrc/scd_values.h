/** \file
 *	\brief scd_values.h header file
 *
 *  This file defines the error/success codes returned by functions
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
 */

#ifndef _SCD_VALUES_H_
#define _SCD_VALUES_H_

typedef enum retstat{
   RET_SUCCESS = 0,

   // General errors
   RET_ERROR =                          0x01,
   RET_ERR_CHECK =                      0x02,
   RET_ERR_PARAM =                      0x03,

   // Initialisation errors
   RET_ERR_INIT_ICC_ACTIVATE =          0x10,
   RET_ERR_INIT_ICC_RESPONSE =          0x11,
   RET_ERR_INIT_ICC_ATR_TS =            0x12,
   RET_ERR_INIT_ICC_ATR_T0 =            0x13,
   RET_ERR_INIT_ICC_ATR_TB1 =           0x14,
   RET_ERR_INIT_ICC_ATR_TD1 =           0x15,
   RET_ERR_INIT_ICC_ATR_TA2 =           0x16,
   RET_ERR_INIT_ICC_ATR_TB2 =           0x17,
   RET_ERR_INIT_ICC_ATR_TC2 =           0x18,
   RET_ERR_INIT_ICC_ATR_TA3 =           0x19,
   RET_ERR_INIT_ICC_ATR_TB3 =           0x10,
   RET_ERR_INIT_ICC_ATR_TC3 =           0x1A,
   RET_ERR_INIT_ICC_ATR_T1_CHECK =      0x1B,
} RETURN_CODE;

#endif // _SCD_VALUES_H_

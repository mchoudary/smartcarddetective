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
   RET_ERROR = 1,
   RET_ERR_CHECK = 2,
} RETURN_CODE;

#endif // _SCD_VALUES_H_

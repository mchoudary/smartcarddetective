/**
 * \file
 * \brief scd_values.h header file
 *
 * This file defines the error/success codes returned by functions
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

#ifndef _SCD_VALUES_H_
#define _SCD_VALUES_H_

typedef enum retstat{
    RET_SUCCESS = 0,

    // General errors
    RET_ERROR =                          0x01,
    RET_ERR_CHECK =                      0x02,
    RET_ERR_PARAM =                      0x03,
    RET_ERR_MEMORY =                     0x04,

    // ICC errors
    RET_ICC_INIT_ACTIVATE =              0x10,
    RET_ICC_INIT_RESPONSE =              0x11,
    RET_ICC_INIT_ATR_TS =                0x12,
    RET_ICC_INIT_ATR_T0 =                0x13,
    RET_ICC_INIT_ATR_TB1 =               0x14,
    RET_ICC_INIT_ATR_TD1 =               0x15,
    RET_ICC_INIT_ATR_TA2 =               0x16,
    RET_ICC_INIT_ATR_TB2 =               0x17,
    RET_ICC_INIT_ATR_TC2 =               0x18,
    RET_ICC_INIT_ATR_TA3 =               0x19,
    RET_ICC_INIT_ATR_TB3 =               0x10,
    RET_ICC_INIT_ATR_TC3 =               0x1A,
    RET_ICC_INIT_ATR_T1_CHECK =          0x1B,
    RET_ICC_BAD_PROTO =                  0x1C,
    RET_ICC_TIME_OUT =                   0x1D,
    RET_ICC_SEND_CMD =                   0x1E,
    RET_ICC_GET_RESPONSE =               0x1F,

    // Terminal conditions
    RET_TERMINAL_RESET_LOW =             0x20,
    RET_TERMINAL_TIME_OUT =              0x21,
    RET_TERMINAL_GET_CMD =               0x22,
    RET_TERMINAL_SEND_RESPONSE =         0x22,
    RET_TERMINAL_ENCRYPTED_PIN =         0x23,
    RET_TERMINAL_NO_CLOCK =              0x24,

    // EMV protocol/command errors
    RET_EMV_SELECT =                     0x30,
    RET_EMV_INIT_TRANSACTION =           0x31,
    RET_EMV_READ_DATA =                  0x32,
    RET_EMV_GET_DATA =                   0x33,
    RET_EMV_DDA =                        0x34,
    RET_EMV_PIN_TRY_EXCEEDED =           0x35,
    RET_EMV_GENERATE_AC =                0x35,
} RETURN_CODE;

#endif // _SCD_VALUES_H_

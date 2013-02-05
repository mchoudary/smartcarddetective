/**
 * \file
 * \brief scd_logger.h header file
 *
 * This file defines functions and structures needed to keep a log of events
 * and data used by the SCD
 *
 * These functions are not microcontroller dependent but they are intended
 * for the AVR 8-bit architecture
 *
 * Copyright (C) 2012 Omar Choudary (omar.choudary@cl.cam.ac.uk)
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


#ifndef _SCD_LOGGER_H_
#define _SCD_LOGGER_H_

#include <stdint.h>

#define LOG_BUFFER_SIZE 3900    // static for simplicity
// we are restricted here by the memory capacity

/** Structure used to keep the log **/
struct log_struct {
    uint8_t log_buffer[LOG_BUFFER_SIZE];
    uint32_t position;
};
typedef struct log_struct log_struct_t;

/**
  * Definition of log direction bits, used in some methods to select
  * which part of a transaction to log
  */
typedef enum {
    LOG_DIR_TERMINAL = 1,
    LOG_DIR_ICC = 2,
    LOG_DIR_BOTH = 3,
} SCD_LOG_DIR;


/**
 * Definition of possible byte options.
 * Each entry in the log is composed of at least 2 bytes: L1 L2 ....
 * L1 = XXXXXXYY defines what the next byte(s) mean, where XXXXXX is
 * used for the encoding of the type (6 bits) and YY (2 bits) to specify
 * how many bytes follow (b'00 -> 1, b'01 -> 2, b'10 -> 3 or b'11 -> 4).
 * These are defined next.
 */
typedef enum {
    // EMV/ISO-7816 data bytes
    LOG_BYTE_ATR_FROM_ICC = (0x00 << 2 | 0x00),             // 0x00
    LOG_BYTE_ATR_TO_TERMINAL = (0x01 << 2 | 0x00),          // 0x04
    LOG_BYTE_TO_TERMINAL = (0x02 << 2 | 0x00),              // 0x08
    LOG_BYTE_FROM_TERMINAL = (0x03 << 2 | 0x00),            // 0x0C
    LOG_BYTE_TO_ICC = (0x04 << 2 | 0x00),                   // 0x10
    LOG_BYTE_FROM_ICC = (0x05 << 2 | 0x00),                 // 0x14
    LOG_BYTE_ATR_FROM_USB = (0x06 << 2 | 0x00),             // 0x18

    // Terminal events
    LOG_TERMINAL_CLK_ACTIVE = (0x10 << 2 | 0x00),           // 0x40
    LOG_TERMINAL_RST_LOW = (0x11 << 2 | 0x00),              // 0x44
    LOG_TERMINAL_TIME_OUT = (0x12 << 2 | 0x00),             // 0x48
    LOG_TERMINAL_ERROR_RECEIVE = (0x13 << 2 | 0x00),        // 0x4C
    LOG_TERMINAL_ERROR_SEND = (0x14 << 2 | 0x00),           // 0x50
    LOG_TERMINAL_NO_CLOCK = (0x15 << 2 | 0x00),             // 0x54

    // ICC events
    LOG_ICC_ACTIVATED = (0x20 << 2 | 0x00),                 // 0x80
    LOG_ICC_DEACTIVATED = (0x21 << 2 | 0x00),               // 0x84
    LOG_ICC_RST_HIGH = (0x22 << 2 | 0x00),                  // 0x88
    LOG_ICC_ERROR_RECEIVE = (0x23 << 2 | 0x00),             // 0x8C
    LOG_ICC_ERROR_SEND = (0x24 << 2 | 0x00),                // 0x90
    LOG_ICC_INSERTED = (0x25 << 2 | 0x00),                  // 0x94

    // General events
    // The time should be saved as little endian using 4 bytes
    LOG_TIME_DATA_TO_ICC = (0x30 << 2 | 0x03),              // 0xC3
    LOG_TIME_GENERAL = (0x31 << 2 | 0x03),                  // 0xC7
    // Error events
    LOG_ERROR_MEMORY = (0x32 << 2 | 0x00),                  // 0xC8
    LOG_WDT_RESET = (0x33 << 2 | 0x00),                     // 0xCC

}SCD_LOG_BYTE;

/**
 * Definition of possible communication sides when logging the communication
 * between card and terminal
 */
typedef enum{
    LOG_COM_SIDE_ICC = 0,
    LOG_COM_SIDE_TERMINAL = 1,
    LOG_COM_SIDE_BOTH = 2,
}LOG_COM_SIDE;

/// Reset the log buffer and position
void ResetLogger(log_struct_t *logger);

/// Log one byte of data
uint8_t LogByte1(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a);

/// Log two bytes of data
uint8_t LogByte2(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a,
        uint8_t byte_b);

/// Log three bytes of data
uint8_t LogByte3(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a,
        uint8_t byte_b, uint8_t byte_c);

/// Log four bytes of data
uint8_t LogByte4(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a,
        uint8_t byte_b, uint8_t byte_c, uint8_t byte_d);


#endif // _SCD_LOGGER_H_


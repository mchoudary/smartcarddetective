/**
 * \file
 * \brief	scd_logger.c source file
 *
 * This file implements functions to update the SCD log structure
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

#include <string.h>

#include "scd_logger.h"
#include "scd_values.h"


/**
 * Function to reset the log structure
 *
 * @param logger the log structure
 */
void ResetLogger(log_struct_t *logger)
{
  if(logger == NULL)
    return;

  memset(logger->log_buffer, 0, LOG_BUFFER_SIZE);
  logger->position = 0;
}

/**
 * Function used to log one byte of data. 
 *
 * @param logger the log structure
 * @param type the kind of data to be logged
 * @param byte_a the byte to be logged
 * @return zero if the logging was done or non-zero if some error
 * (e.g. out of memory or wrong tag) ocurred
 */
uint8_t LogByte1(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a)
{
  if(logger == NULL)
    return RET_ERR_PARAM;
  if((type & 0x03) != 0x00)
    return RET_ERR_PARAM;
  if(logger->position > LOG_BUFFER_SIZE - 2)
    return RET_ERR_MEMORY;

  logger->log_buffer[logger->position++] = type;
  logger->log_buffer[logger->position++] = byte_a;

  return 0;
}

/**
 * Function used to log two bytes of data.
 *
 * @param logger the log structure
 * @param type the kind of data to be logged
 * @param byte_a the first byte to be logged
 * @param byte_b the second byte to be logged
 * @return zero if the logging was done or non-zero if error
 * @sa LogByte1
 */
uint8_t LogByte2(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a,
    uint8_t byte_b)
{
  if(logger == NULL)
    return RET_ERR_PARAM;
  if((type & 0x03) != 0x01)
    return RET_ERR_PARAM;
  if(logger->position > LOG_BUFFER_SIZE - 3)
    return RET_ERR_MEMORY;

  logger->log_buffer[logger->position++] = type;
  logger->log_buffer[logger->position++] = byte_a;
  logger->log_buffer[logger->position++] = byte_b;

  return 0;
}


/**
 * Function used to log three bytes of data.
 *
 * @param logger the log structure
 * @param type the kind of data to be logged
 * @param byte_a the first byte to be logged
 * @param byte_b the second byte to be logged
 * @param byte_c the third byte to be logged
 * @return zero if the logging was done or non-zero if error
 * @sa LogByte1
 */
uint8_t LogByte3(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a,
    uint8_t byte_b, uint8_t byte_c)
{
  if(logger == NULL)
    return RET_ERR_PARAM;
  if((type & 0x03) != 0x02)
    return RET_ERR_PARAM;
  if(logger->position > LOG_BUFFER_SIZE - 4)
    return RET_ERR_MEMORY;

  logger->log_buffer[logger->position++] = type;
  logger->log_buffer[logger->position++] = byte_a;
  logger->log_buffer[logger->position++] = byte_b;
  logger->log_buffer[logger->position++] = byte_c;

  return 0;
}

/**
 * Function used to log four bytes of data.
 *
 * @param logger the log structure
 * @param type the kind of data to be logged
 * @param byte_a the first byte to be logged
 * @param byte_b the second byte to be logged
 * @param byte_c the third byte to be logged
 * @param byte_d the fourth byte to be logged
 * @return zero if the logging was done or non-zero if error
 * @sa LogByte1
 */
uint8_t LogByte4(log_struct_t *logger, SCD_LOG_BYTE type, uint8_t byte_a,
    uint8_t byte_b, uint8_t byte_c, uint8_t byte_d)
{
  if(logger == NULL)
    return RET_ERR_PARAM;
  if((type & 0x03) != 0x03)
    return RET_ERR_PARAM;
  if(logger->position > LOG_BUFFER_SIZE - 5)
    return RET_ERR_MEMORY;

  logger->log_buffer[logger->position++] = type;
  logger->log_buffer[logger->position++] = byte_a;
  logger->log_buffer[logger->position++] = byte_b;
  logger->log_buffer[logger->position++] = byte_c;
  logger->log_buffer[logger->position++] = byte_d;

  return 0;
}


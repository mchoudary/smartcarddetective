/*
 * Author: Omar Choudary
 *
 * File used to combine both C and assembler to use a relative timer/counter.
 *
 * Hepful hints provided by Joerg Wunsch:
 * http://www.nongnu.org/avr-libc/user-manual/group__asmdemo.html
 *
 * ---------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Joerg Wunsch wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Joerg Wunsch
 * ---------------------------------------------------------------------------
 */

#ifndef _COUNTER_H_
#define _COUNTER_H_

/*
 * Global register variables.
 */
#ifdef __ASSEMBLER__

// Syncronization counter, 32 bit value accessed through 4 CONSECUTIVE registers
// In the code, these values should be loaded using the C global variable as follows
#  define counter_b0    r18
#  define counter_b1    r19
#  define counter_b2    r20
#  define counter_b3    r21

// Variables used in asm subroutines to store temporary values
#  define counter_inc   r26
#  define counter_sreg  r27

#else  /* !ASSEMBLER */

#include <stdint.h>

volatile uint32_t counter_t2; // this will be updated and used in both C and asm code
#define counter_res_us 1024 // each counter unit represents 1024 micro-seconds
#define SYNC_COUNTER_SIZE (sizeof(counter_t2)) // size of counter variable in bytes

#endif /* ASSEMBLER */

#endif // _COUNTER_H_


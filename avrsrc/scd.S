// Assembler file for the main program of the Smartcard Defender
//
// This file should contain functions such as intrerrupt handlers that
// may be better done in assembler for efficiency of speed and space
//
// Author: Omar Choudary
//
// for information see:
// http://www.nongnu.org/avr-libc/user-manual/group__asmdemo.html

#include <avr/io.h>

#include "counter.h"


/*
 * Timer 2 COMPA hit. 
 * Used to update our sync counter.
 * The variable counter (see counter.h) can be accessed in
 * C code, and is stored as 32-bit integer little-endian.
 */
.global TIMER2_COMPA_vect
TIMER2_COMPA_vect:
    push counter_sreg                    ; used for SREG
    in  counter_sreg, _SFR_IO_ADDR(SREG) ; save SREG
    cli                                  ; disable interrupts
    push counter_b0
    push counter_b1
    push counter_b2
    push counter_b3
    push counter_inc
    push r30
    push r31

    ldi r30, lo8(counter_t2)
    ldi r31, hi8(counter_t2)
    ld counter_b0, Z
    ldd counter_b1, Z+1
    ldd counter_b2, Z+2
    ldd counter_b3, Z+3
    ldi counter_inc, 1                   ; use to increment with carry
    add counter_b0, counter_inc
    adc counter_b1, r1
    adc counter_b2, r1
    adc counter_b3, r1
    st Z, counter_b0
    std Z+1, counter_b1
    std Z+2, counter_b2
    std Z+3, counter_b3

    pop r31
    pop r30
    pop counter_inc
    pop counter_b3
    pop counter_b2
    pop counter_b1
    pop counter_b0

    out _SFR_IO_ADDR(SREG), counter_sreg
    pop counter_sreg
    reti


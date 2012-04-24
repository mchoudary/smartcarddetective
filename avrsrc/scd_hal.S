// Assembler file for the Smart Card Detective Hardware Abstraction Layer
//
// This file should contain functions such as timer increments that
// may be better done in assembler for efficiency of speed and space
//
// Author: Omar Choudary
//
// for information see:
// http://www.nongnu.org/avr-libc/user-manual/group__asmdemo.html

#include <avr/io.h>

#include "counter.h"


/**
 * Increment the value of the sync counter.
 * This method should be called from an interrupt routine
 * in order to update its value regularly.
 * No registers are preserved in order to speed up so
 * we asume the calling function is doing that.
 *
 * @return the value of the sync counter, which is assumed
 * to be used by avr-gcc in little endian
 * @sa ReatTimerT2
 */
.global IncrementCounter
IncrementCounter:
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
    ret


/**
 * This method returns the value of the sync counter.
 * The synchronization counter is updated within an interrupt
 * subroutine which is dependent on a hardware timer.
 *
 * @return the value of the sync counter, assumed to be used
 * by avr-gcc in little endian
 * @sa IncrementCounter
 */
.global GetCounter
GetCounter:
    push counter_sreg                    ; used for SREG
    in  counter_sreg, _SFR_IO_ADDR(SREG) ; save SREG
    cli                                  ; disable interrupts

    ldi r30, lo8(counter_t2)
    ldi r31, hi8(counter_t2)
    ld r22, Z+
    ld r23, Z+
    ld r24, Z+
    ld r25, Z+

    out _SFR_IO_ADDR(SREG), counter_sreg
    pop counter_sreg
    ret

/**
 * This method resets to 0 the value of the sync counter.
 *
 * @sa IncrementCounter
 */
.global ResetCounter
ResetCounter:
    push counter_sreg                    ; used for SREG
    in  counter_sreg, _SFR_IO_ADDR(SREG) ; save SREG
    cli                                  ; disable interrupts

    ldi r30, lo8(counter_t2)
    ldi r31, hi8(counter_t2)
    st Z, r1
    std Z+1, r1
    std Z+2, r1
    std Z+3, r1

    out _SFR_IO_ADDR(SREG), counter_sreg
    pop counter_sreg
    ret

/**
 * This method sets the value of the sync counter to the given value
 * 
 * @param value the desired value of the counter
 * @sa IncrementCounter
 */
.global SetCounter
SetCounter:
    push counter_sreg                    ; used for SREG
    in  counter_sreg, _SFR_IO_ADDR(SREG) ; save SREG
    cli                                  ; disable interrupts

    ldi r30, lo8(counter_t2)
    ldi r31, hi8(counter_t2)
    st Z, r22
    std Z+1, r23
    std Z+2, r24
    std Z+3, r25

    out _SFR_IO_ADDR(SREG), counter_sreg
    pop counter_sreg
    ret


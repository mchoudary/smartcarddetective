; Smartcard Defender code for AT90USB1287
;
; Author: Omar Choudary
; Version: 1.0

#include <avr/io.h>


; defines

#define ValueOCR0A 0x1					; OCR0A used as prescaler for CLK to ICC

TempS1 = 18    					; R18, R19 for use in subroutines
TempS2 = 19	 


; Start timer 0 and toggle output A which should be connected
; to the ICC. This function also sets up the direction of PB6
; as output
.global StartClkICC
StartClkICC:
	sbi _SFR_IO_ADDR(DDRB), 0x7			; PB7 set as output
	ldi TempS1, 0x42		
	out _SFR_IO_ADDR(TCCR0A), TempS1	; Timer 0 in CTC mode, toggle OC0A on match
	ldi TempS1, ValueOCR0A				; Frequency on OC0A = fCLK_I/O / (2*(valueOCRA+1))
										; so if CLK is 16 Mhz and valueOCRA = 1
										; we give 4 Mhz to ICC.
										; If CLK is 8 Mhz we give 2 Mhz to ICC
										; Both values should be acceptable by ICC
	out _SFR_IO_ADDR(OCR0A), TempS1
	ldi TempS1, 0x1	
	out _SFR_IO_ADDR(TCCR0B), TempS1	; Start timer with no prescaler, i.e. the
										; fCLK_I/O = CPU_CLK
	ret
	
//Stop timer 0 so no toggle occurs on OC0A
.global StopClkICC
StopClkICC:
	ldi TempS1, 0
	out _SFR_IO_ADDR(TCCR0B), TempS1	; Stop timer
	out _SFR_IO_ADDR(TCCR0A), TempS1	; Disable OC0A toggling

	ret

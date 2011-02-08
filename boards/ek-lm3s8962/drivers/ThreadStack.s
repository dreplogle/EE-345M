; ThreadStack.s contains assembly functions for manipulating the 
; thread stack
;
; Written by: Katy Loeffler 2/7/2011
;*****************************************************************************

  THUMB
  REQUIRE8
  PRESERVE8
  AREA |.text|, CODE, READONLY, ALIGN=2

  EXPORT PushRegs4to11
  EXPORT PullRegs4to11
  EXPORT SetStackPointer

StkPtr     RN R0    ; 1st function input: Stack Pointer

;******************************************************************************
; PushRegs4to11
;
; Pushes registers 4 through 11 for thread switching purposes
; \param R0 holds the stack pointer
; \return the new position of the stack pointer
;******************************************************************************
PushRegs4to11
  PUSH {R4, R5, R7, R8, R9, R10, R11}	; save registers
  MRS R0, PSP							; move PSP into result register R0
  BX LR

;******************************************************************************
; PullRegs4to11
;
; Pulls registers 4 through 11 for thread switching purposes
; \param R0 holds the stack pointer
; \return the new position of the stack pointer
;******************************************************************************
PullRegs4to11
  POP {R4, R5, R7, R8, R9, R10, R11}	; save registers
  MRS R0, PSP							; move PSP into result register R0
  BX LR

;******************************************************************************
; SetStackPointer
;
; Loads the stack pointer with the indicated value
; \param R0 holds the new stack pointer value
; \return none
;******************************************************************************
SetStackPointer
  MSR PSP, R0							; set PSP to value in R0 
  BX LR

;******************************************************************************
;
; Make sure the end of this section is aligned.
;
;******************************************************************************
  ALIGN

;******************************************************************************
;
; Tell the assembler that we're done.
;
;******************************************************************************
  END



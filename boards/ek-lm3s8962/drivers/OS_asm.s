;*****************************************************************************
; OS_asm.s 
; 
;
; Written by: Katy Loeffler 2/7/2011
;*****************************************************************************



  THUMB
  REQUIRE8
  PRESERVE8
  AREA |.text|, CODE, READONLY, ALIGN=2

  EXPORT  StackInit
  EXPORT  LaunchInternal
  EXPORT  SwitchThreads
  EXPORT  TriggerPendSV
  EXPORT  SRSave
  EXPORT  SRRestore

  IMPORT  CurrentThread
  IMPORT  NextThread

NVIC_INT_CTRL   EQU     0xE000ED04     ; Interrupt control state register.
NVIC_PENDSVSET  EQU     0x10000000     ; Value to trigger PendSV exception.
NEXT_PTR_OFFSET EQU		4


ThreadStkPtr	RN R0
task			RN R1


;*********************************************************************
;
; Enter a critical section.  Copied from uCOSII.
;
;*********************************************************************
SRSave
  MRS	R0, PRIMASK
  CPSID I
  BX  	LR

;*********************************************************************
;
; Exit a critical section.  Copied from uCOSII.
;
;*********************************************************************
SRRestore
  MSR  PRIMASK, R0
  BX	LR

;*********************************************************************
;
; Initialize the stack of a new thread
;
; R0 is first parameter, holds the Thread SP
; R1 is the second parameter, holds the address of the first function
;
; Returns: R0 is the new position of the stack pointer
;
; Written by Katy Loeffler
;
;*********************************************************************
StackInit
  CPSID	I
  PUSH  {R4}				; We'll use R4 for manipulation
  MOV	R2, R13				; Save current stack pointer in R2
  MOV 	R13, R0				; Assign SP to ThreadSP based on R0 parameter

  POP  {R4}					; Throw away
  MOV	R4, #0x01000000
  PUSH	{R4}				; Fake PSR
  PUSH	{R1}				; Initial PC value passed in R1
  MOV	R4, #0x0E0E0E0E
  PUSH 	{R4}				; Fake LR
  MOV	R4, #0x12121212		
  PUSH 	{R4}				; Fake R12
  MOV	R4, #0x03030303		
  PUSH 	{R4}				; Fake R3
  MOV	R4, #0x02020202		
  PUSH 	{R4}				; Fake R2
  MOV	R4, #0x01010101		
  PUSH 	{R4}				; Fake R1
  MOV	R4, #0x00010001		
  PUSH 	{R4}				; Fake R0
  MOV	R4, #0x11111111		
  PUSH 	{R4}				; Fake R11
  MOV	R4, #0x10101010		
  PUSH 	{R4}				; Fake R10
  MOV	R4, #0x09090909		
  PUSH 	{R4}				; Fake R9
  MOV	R4, #0x08080808		
  PUSH 	{R4}				; Fake R8
  MOV	R4, #0x07070707		
  PUSH 	{R4}				; Fake R7
  MOV	R4, #0x06060606		
  PUSH 	{R4}				; Fake R6
  MOV	R4, #0x05050505		
  PUSH 	{R4}				; Fake R15
  MOV	R4, #0x04040404		
  PUSH 	{R4}				; Fake R4
  MOV	R4, #0xFFFFFFF9		
  PUSH 	{R4}				; Push LR value onto stack tell the processor
  							; to enter thread mode the first time the thread
							; is accessed.

  MOV 	R0, R13				; Return the new ThreadSP
  MOV 	R13, R2				; Restore the current SP
  POP	{R4}			    ; Restore R4
  CPSIE	I
  BX LR

;******************************************************************************
;
; Launch the operating system. 
;
; R0 is the first parameter, holds the SP of the first thread to be executed.
;
; Returns: none.
;******************************************************************************
LaunchInternal
  ;CPSID I
  MOV 	R13, R0						; Load thread SP
  POP	{LR}
  POP	{R4,R5,R6,R7,R8,R9,R10,R11}	; Pop registers for new thread
  POP	{R0-R3,R12}
  POP	{LR}						; Throw away
  POP	{LR}						; LR = PC					
  CPSIE I
  BX	LR


;******************************************************************************
;
; Switch Threads: Push R4-R11 on the stack and save the SP of the old thread
; in the TCB, then restore the SP of the new thread and pop R4-R11.
;
; Input: none
;
; Returns: R0 is the SP of the outgoing thread to be saved in the TCB
; Written by Katy Loeffler
;
;******************************************************************************
SwitchThreads
  CPSID I
  LDR	R1, =CurrentThread
  LDR   R0,[R1]						
  PUSH 	{R11,R10,R9,R8,R7,R6,R5,R4}	; Push registers for old thread
  PUSH	{LR}
  STR	R13,[R0]			 		; Save new SP in TCB, it is the first entry
  LDR   R0, =NextThread
  LDR   R0,[R0]						; Dereference
  STR	R0,[R1]
  LDR	R13,[R0]					; Get stack pointer
  POP	{LR}
  POP	{R4,R5,R6,R7,R8,R9,R10,R11}	; Pop registers for new thread   
  CPSIE I
  BX	LR
	
;******************************************************************************
;
; Trigger a PendSV exception.  This function was copied from uOSII.
;
;******************************************************************************
TriggerPendSV
	CPSID  	I
    LDR     R0, =NVIC_INT_CTRL ; Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]
	CPSIE	I
    BX      LR

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


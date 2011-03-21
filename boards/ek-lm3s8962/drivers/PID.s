;/******************** (C) COPYRIGHT 2009  STMicroelectronics ********************
;* File Name          : PID_stm32.s
;* Author             : MCD Application Team
;* Version            : V2.0.0
;* Date               : 04/27/2009
;* Description        : This source file contains assembly optimized source code
;*                      of a PID controller.
;********************************************************************************
;* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
;* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
;* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
;* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
;* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
;* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
;*******************************************************************************/

  THUMB
  REQUIRE8
  PRESERVE8

  AREA |.text|, CODE, READONLY, ALIGN=2

  EXPORT PID_stm32
;  IMPORT IntTerm
;  IMPORT PrevError
  EXTERN IntTerm
  EXTERN PrevError

Err     RN R0    				; 1st function input: Error  
Coeff   RN R1    				; 2nd fct input: Address of coefficient table 
Kd      RN R1
Ki      RN R2
Kp      RN R3

Out     RN R4
Result  RN R2
Integ   RN R5
PrevErr RN R12

;/******************* (C) COPYRIGHT 2009  STMicroelectronics *****END OF FILE****/
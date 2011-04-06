//*****************************************************************************
//
// Filename: tachometer.h
// Authors: Dan Cleary   
// Initial Creation Date: March 29, 2011 
// Description: This file includes functions for interfacing with the 
//   QRB1134 optical reflectance sensor.    
// Lab Number: 6    
// TA: Raffaele Cetrulo      
// Date of last revision: March 29, 2011      
// Hardware Configuration:
//   PB0 - Tachometer A Input
//   PB1 - Tachometer B Input
//
//*****************************************************************************

#ifndef TACH
#define TACH

#define MAX_TACH_FIFOSIZE 128 		// can be any size

// *********** Tach_Init ************
// Initializes tachometer I/O pins and interrupts
// Inputs: none
// Outputs: none
void Tach_Init(unsigned long priority);

// ********** Tach_InputCapture ***********
// Input capture exception handler for tachometer.
//   Interrupts on rising edge. Obtains time since
//   previous interrupts, sends time to foreground
//   via FIFO.
// Inputs: none
// Outputs: none
void Tach_InputCapture(void);

// ********** Tach_SendData ***********
// Analyzes tachometer data, passes to
//   big board via CAN.
// Inputs: none
// Outputs: none
void Tach_SendData(void);

#endif

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

#ifndef TACHO
#define TACHO

// ************* Tachometer_Init ***********
// Initializes tachometer I/O pins and interrupts
// Inputs: none
// Outputs: none
void Tachometer_Init(unsigned long priority);

// ********** Tachometer_InputCapture ***********
// Input capture exception handler for tachometer.
//   Interrupts on rising edge. Obtains time since
//   previous interrupts, sends time to foreground
//   via FIFO.
// Inputs: none
// Outputs: none
void Tachometer_InputCapture(void);

// ********** Tachometer_FG ***********
// Foreground thread that passes tachometer data
// to other computer via CAN.
//   1) Obtains period from IC FIFO
//   2) Transforms data into correct units
//   3) Filters data
//   4) Sends to other comp via CAN
// Inputs: none
// Outputs: none
//
void Tachometer_FG(void);

#endif

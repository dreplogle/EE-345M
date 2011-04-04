//*****************************************************************************
//
// Filename: tachometer.c
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

#include "tachometer.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "driverlib/can.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "drivers/OS.h"
#include "drivers/can_fifo.h"

long SRSave (void);
void SRRestore(long sr);

#define OS_ENTERCRITICAL(){sr = SRSave();}
#define OS_EXITCRITICAL(){SRRestore(sr);}

//***********************************************************************
// OS_Fifo variables, this code segment copied from Valvano, lecture1
//***********************************************************************
typedef unsigned long dataType;
dataType volatile *Tachometer_PutPt; // put next
dataType volatile *Tachometer_GetPt; // get next
dataType static Tachometer_Fifo[MAX_TACH_FIFOSIZE];
unsigned long Tachometer_FifoSize;
Sema4Type Tachometer_FifoDataReady;

//***********************************************************************
//
// OS_Fifo_Init
//
//***********************************************************************
void
Tachometer_Fifo_Init(unsigned int size)
{
  long sr = 0;
  OS_ENTERCRITICAL();

  Tachometer_PutPt = Tachometer_GetPt = &Tachometer_Fifo[0];
  Tachometer_FifoSize = size;
  OS_InitSemaphore(&Tachometer_FifoDataReady,0);
  

  OS_EXITCRITICAL();  
}

//***********************************************************************
//
// OS_Fifo_Get
//
//***********************************************************************
unsigned int
Tachometer_Fifo_Get(unsigned long * dataPtr)
{
  if(Tachometer_PutPt == Tachometer_GetPt ){
    return FAIL;// Empty if Tachometer_PutPt=GetPt
  }
  else{
  OS_Wait(&Tachometer_FifoDataReady);
  *dataPtr = *(Tachometer_GetPt++);
    if(Tachometer_GetPt==&Tachometer_Fifo[Tachometer_FifoSize]){
      Tachometer_GetPt = &Tachometer_Fifo[0]; // wrap
    }
    return SUCCESS;
  }
}

//***********************************************************************
//
// OS_Fifo_Put
//
//***********************************************************************
unsigned int
Tachometer_Fifo_Put(unsigned long data)
{
  dataType volatile *nextPutPt;
  nextPutPt = Tachometer_PutPt+1;
  if(nextPutPt ==&Tachometer_Fifo[Tachometer_FifoSize]){
    nextPutPt = &Tachometer_Fifo[0]; // wrap
  }
  if(nextPutPt == Tachometer_GetPt){
    return FAIL; // Failed, fifo full
  }
  else{
    *(Tachometer_PutPt) = data; // Put
    Tachometer_PutPt = nextPutPt; // Success, update
	OS_Signal(&Tachometer_FifoDataReady);
    return SUCCESS;
  }
}

// ********** Tachometer_Filter ***********
// Peforms IIR filter calculations, stores
//   filtered data in y buffer
// (Based on Filter from Lab2.c	by Jonathan Valvano)
// Inputs:
// 	 data - data to be filtered
// Outputs: filtered data
static
unsigned long Tachometer_Filter(unsigned long data){
	static unsigned long x[4];
	static unsigned long y[4];
	static unsigned int n=2;

	n++;
	if(n==4) n=2;
	x[n] = x[n-2] = data; // two copies of new data
	y[n] = (y[n-1] + x[n])/2;
	y[n-2] = y[n]; // two copies of filter outputs too
	return y[n];
} 

// *********** Tachometer_Init ************
// Initializes tachometer I/O pins and interrupts
// Inputs: none
// Outputs: none
void Tachometer_Init(unsigned long priority){
	//Configure port pin for digital input
	GPIODirModeSet(GPIO_PORTB_BASE, (GPIO_PIN_0 | GPIO_PIN_1), GPIO_DIR_MODE_IN);
	GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

	
	//Configure and enable edge interrupts
	GPIOIntTypeSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_FALLING_EDGE); 
	GPIOPinIntEnable(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);


	//Enable port interrupt in NVIC
	IntEnable(INT_GPIOB);
	IntPrioritySet(INT_GPIOB, priority << 5);

	//Initialize FIFO
	Tachometer_Fifo_Init(128);
}

// ********** Tachometer_InputCapture ***********
// Input capture exception handler for tachometer.
//   Interrupts on rising edge. Obtains time since
//   previous interrupts, sends time to foreground
//   via FIFO.
// Inputs: none
// Outputs: none
void Tachometer_InputCapture(void){
	static unsigned long time1 = 0;
	unsigned long time2;

    // TODO: Get Time Automatically from hardware
	time2 = OS_Time();
	if (time1 != 0){
		Tachometer_Fifo_Put(OS_TimeDifference(time2, time1));
	}
	time1 = time2;
}


// ********** Tachometer_FG ***********
// Foreground thread that analyzes tachometer data,
//   passes to other computer via CAN.
// Inputs: none
// Outputs: none
void Tachometer_FG(void){
	unsigned long *data;

    while(1){
		Tachometer_Fifo_Get(data);
		*data = 60/((*data << 2)/1000000); //convert to RPM
		*data = Tachometer_Filter(*data);
		CANTransmitFIFO((unsigned char *)data, 4);	
	} 
}

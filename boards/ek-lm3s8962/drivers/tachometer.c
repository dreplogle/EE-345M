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
#include "timer.h"
#include "inc/hw_timer.h"
#include "driverlib/sysctl.h"
//#include "drivers/can_fifo.h"

long SRSave (void);
void SRRestore(long sr);

#define OS_ENTERCRITICAL(){sr = SRSave();}
#define OS_EXITCRITICAL(){SRRestore(sr);}

//***********************************************************************
// OS_Fifo variables, this code segment copied from Valvano, lecture1
//***********************************************************************
typedef unsigned long dataType;
dataType volatile *Tach_PutPt; // put next
dataType volatile *Tach_GetPt; // get next
dataType static Tach_Fifo[MAX_TACH_FIFOSIZE];
unsigned long Tach_FifoSize;
Sema4Type Tach_FifoDataReady;

//***********************************************************************
//
// OS_Fifo_Init
//
//***********************************************************************
static
void Tach_Fifo_Init(unsigned int size)
{
  long sr = 0;

  Tach_PutPt = Tach_GetPt = &Tach_Fifo[0];
  Tach_FifoSize = size;
}

//***********************************************************************
//
// OS_Fifo_Get
//
//***********************************************************************
static
unsigned int
Tach_Fifo_Get(unsigned long * dataPtr)
{
  if(Tach_PutPt == Tach_GetPt ){
    return FAIL;// Empty if Tach_PutPt=GetPt
  }
  else{
    *dataPtr = *(Tach_GetPt++);
    if(Tach_GetPt==&Tach_Fifo[Tach_FifoSize]){
      Tach_GetPt = &Tach_Fifo[0]; // wrap
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
Tach_Fifo_Put(unsigned long data)
{
  dataType volatile *nextPutPt;
  nextPutPt = Tach_PutPt+1;
  if(nextPutPt ==&Tach_Fifo[Tach_FifoSize]){
    nextPutPt = &Tach_Fifo[0]; // wrap
  }
  if(nextPutPt == Tach_GetPt){
    return FAIL; // Failed, fifo full
  }
  else{
    *(Tach_PutPt) = data; // Put
    Tach_PutPt = nextPutPt; // Success, update
    return SUCCESS;
  }
}

// ********** Tach_Filter ***********
// Peforms IIR filter calculations, stores
//   filtered data in y buffer
// (Based on Filter from Lab2.c	by Jonathan Valvano)
// Inputs:
// 	 data - data to be filtered
// Outputs: filtered data
static
unsigned long Tach_Filter(unsigned long data){
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

// *********** Tach_TimeDifference ************
// Returns time difference (in ___ (units)) between time1 and time2
// Inputs:
//   time1 - new time (smaller b/c countdown)
//   time2 - old time
// Outputs: none
static
long Tach_TimeDifference(unsigned long time1, unsigned long time2)
//                                   thisTime          	  LastTime
{
  if(time2 >= time1)
  {
  	return (long)(time2-time1);
  }
  else
  {
    return (long)(time2 + (0xFFFF-time1));      
  } 
} 

// *********** Tach_Init ************
// Initializes tachometer I/O pins and interrupts
// Inputs: none
// Outputs: none
void Tach_Init(unsigned long priority){

    
  	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

	//Configure port pin for digital input
	GPIODirModeSet(GPIO_PORTB_BASE, (GPIO_PIN_0 | GPIO_PIN_1), GPIO_DIR_MODE_HW);
	GPIOPadConfigSet(GPIO_PORTB_BASE, (GPIO_PIN_0 | GPIO_PIN_1), GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

	//Configure alternate function
	GPIOPinConfigure(GPIO_PB0_CCP0);
	//GPIOPinConfigure(GPIO_PB1_CCP2);

	// Configure GPTimerModule to generate triggering events
  	// at the specified sampling rate.
  	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

	TimerDisable(TIMER0_BASE, TIMER_A);
	//TimerDisable(TIMER1_BASE, TIMER_A);

	// Configure for Subtimer A Input Edge Timing Mode
	HWREG(TIMER0_BASE + TIMER_O_CFG) = 0x04;
	HWREG(TIMER0_BASE + TIMER_O_TAMR) = 0x07;
	//TimerConfigure(TIMER0_BASE, TIMER_CFG_A_CAP_TIME); 
	HWREG(TIMER0_BASE + TIMER_O_CTL) = HWREG(TIMER0_BASE + TIMER_O_CTL) & ~(0x06);
	HWREG(TIMER0_BASE + TIMER_O_TAILR) = 0xFFFF; 
	TimerIntEnable(TIMER0_BASE, TIMER_CAPA_EVENT);
	TimerEnable(TIMER0_BASE, TIMER_A);

	//Enable port interrupt in NVIC
	IntEnable(INT_TIMER0A);
	IntPrioritySet(INT_TIMER0A, priority << 5);
	//

	//Initialize FIFO
	Tach_Fifo_Init(128);
}

// ********** Tach_InputCapture ***********
// Input capture exception handler for tachometer.
//   Interrupts on rising edge. Obtains time since
//   previous interrupts, sends time to foreground
//   via FIFO.
// Inputs: none
// Outputs: none
void Tach_InputCapture(void){
	static unsigned long time1 = 0;
	unsigned long time2;

    // Get time automatically from hardware
	time2 = HWREG(TIMER0_BASE + TIMER_O_TAR);
	if (time1 != 0){
		Tach_Fifo_Put(Tach_TimeDifference(time2, time1));
	}
	time1 = time2;

	TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT);
}


// ********** Tach_SendData ***********
// Analyzes tachometer data, passes to
//   big board via CAN.
// Inputs: none
// Outputs: none
void Tach_SendData(void){
	unsigned long *data;

	Tach_Fifo_Get(data);
	*data = 60/((*data << 2)/1000000); //convert to RPM
	*data = Tach_Filter(*data);
	//CANTransmitFIFO((unsigned char *)data, 4); 
}

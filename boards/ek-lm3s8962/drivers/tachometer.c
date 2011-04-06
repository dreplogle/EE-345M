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
dataType Tach_Fifo[MAX_TACH_FIFOSIZE];
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
	GPIODirModeSet(GPIO_PORTB_BASE, (GPIO_PIN_1), GPIO_DIR_MODE_HW);
	GPIOPadConfigSet(GPIO_PORTB_BASE, (GPIO_PIN_1), GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

	//Configure alternate function
	//GPIOPinConfigure(GPIO_PB0_CCP0);
	GPIOPinConfigure(GPIO_PB1_CCP2);

	// Configure GPTimerModule to generate triggering events
  	// at the specified sampling rate.
  	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

	TimerDisable(TIMER1_BASE, TIMER_A);
	//TimerDisable(TIMER1_BASE, TIMER_A);

	// Configure for Subtimer A Input Edge Timing Mode
	HWREG(TIMER1_BASE + TIMER_O_CFG) |= 0x04;
	HWREG(TIMER1_BASE + TIMER_O_TAMR) |= 0x07;
	//TimerConfigure(TIMER0_BASE, TIMER_CFG_A_CAP_TIME); 
	HWREG(TIMER1_BASE + TIMER_O_CTL) &= ~(0x06);
	//debugging
	HWREG(TIMER1_BASE + TIMER_O_CTL) |= 0x02;
	HWREG(TIMER1_BASE + TIMER_O_TAILR) |= 0xFFFF; 
	TimerIntEnable(TIMER1_BASE, (TIMER_CAPA_EVENT | TIMER_TIMA_TIMEOUT));
	TimerEnable(TIMER1_BASE, TIMER_A);

	//Enable port interrupt in NVIC
	IntEnable(INT_TIMER1A);
	IntPrioritySet(INT_TIMER1A, priority << 5);
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
unsigned long tach_time_count;
void Tach_InputCapture(void){
	static unsigned long time1 = 0;
	static unsigned char first_flag = 0;
	unsigned long time2;
	long time_debug;

	// if timeout
 	if (HWREG(TIMER1_BASE + TIMER_O_MIS) & TIMER_TIMA_TIMEOUT){
		 tach_time_count++;
	}
	// if input capture
	if (HWREG(TIMER1_BASE + TIMER_O_MIS) & TIMER_CAPA_EVENT){
    // Get time automatically from hardware
		time2 = HWREG(TIMER1_BASE + TIMER_O_TAR) + tach_time_count * 0xFFFF;
		tach_time_count = 0;
		if (!first_flag){
			first_flag = 1;
		}
		else {
			//time_debug = Tach_TimeDifference(time2, time1);
			//Tach_Fifo_Put(time_debug);
			Tach_Fifo_Put(time2);
		}
		time1 = time2;
	}
	TimerIntClear(TIMER1_BASE, (TIMER_CAPA_EVENT | TIMER_TIMA_TIMEOUT));
}


// ********** Tach_SendData ***********
// Analyzes tachometer data, passes to
//   big board via CAN.
// Inputs: none
// Outputs: none
unsigned long SeeTach;
unsigned long speed;
//int CANTransmitFIFO(unsigned char *pucData, unsigned long ulSize);
unsigned char speedBuffer[CAN_FIFO_SIZE];
void Tach_SendData(void){
	unsigned long data;
	unsigned char tachArr[CAN_FIFO_SIZE];
	int i;
	if(Tach_Fifo_Get(&data)){
		SeeTach = data;
		data = (375000000/data); //convert to RPM	-> (60 s)*(10^9ns)/4*(T*40 ns)
		data = Tach_Filter(data);
		speedBuffer[0] = 't';
		memcpy(&speedBuffer[1], &data, 4);

		//tachArr[0] = 't';
		//tachArr[1] = data;
		CAN_Send(speedBuffer);
	}
}

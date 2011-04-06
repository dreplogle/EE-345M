//*****************************************************************************
//
// Lab5.c - user programs, File system, stream data onto disk
// Jonathan Valvano, March 16, 2011, EE345M
//     You may implement Lab 5 without the oLED display
//*****************************************************************************
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
#include <stdio.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/systick.h"
#include "driverlib/fifo.h"
#include "driverlib/adc.h"
#include "drivers/OS.h"
#include "drivers/OSuart.h"
#include "drivers/rit128x96x4.h"
#include "lm3s2110.h"
#include "drivers/tachometer.h"
#include "drivers/ping.h"

unsigned long NumCreated;   // number of foreground threads created
//unsigned long NumSamples;   // incremented every sample
//unsigned long DataLost;     // data sent by Producer, but not received by Consumer
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished

extern unsigned long JitterHistogramA[];
extern unsigned long JitterHistogramB[];
extern long MaxJitterA;
extern long MinJitterA;

unsigned short SoundVFreq = 1;
unsigned short SoundVTime = 0;
unsigned short FilterOn = 1;

int Running;                // true while robot is running

#define GPIO_PF0  (*((volatile unsigned long *)0x40025004))
#define GPIO_PF1  (*((volatile unsigned long *)0x40025008))
#define GPIO_PF2  (*((volatile unsigned long *)0x40025010))
#define GPIO_PF3  (*((volatile unsigned long *)0x40025020))
#define GPIO_PG1  (*((volatile unsigned long *)0x40026008))

//*******************lab 5 main **********
//int main(void){        // lab 5 real main
//  OS_Init();           // initialize, disable interrupts
////  Running = 0;         // robot not running
////  DataLost = 0;        // lost data between producer and consumer
////  NumSamples = 0;
//
////********initialize communication channels
////  OS_Fifo_Init(512);    // ***note*** 4 is not big enough*****
////  ADC_Open();
////  ADC_Collect(0, 1000, &Producer); // start ADC sampling, channel 0, 1000 Hz 
//
////*******attach background tasks***********
////  OS_AddButtonTask(&ButtonPush,2);
////  OS_AddDownTask(&DownPush,3);
//  //OS_AddPeriodicThread(disk_timerproc,TIME_1MS,5);
//
//  NumCreated = 0 ;
//// create initial foreground threads
//  NumCreated += OS_AddThread(&Interpreter,128,2); 
//  NumCreated += OS_AddThread(&IdleTask,128,7);  // runs when nothing useful to do
// 
//  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
//  return 0;             // this never executes
//}

void SmallBoardInit(){	  
	IntMasterDisable();
	// Set the clocking to run from PLL at 50 MHz 
	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	Tach_Init(2);
	IntMasterEnable();
	}

int main(void){
	SmallBoardInit();
	while(1){
		Tach_SendData();
	}
} 
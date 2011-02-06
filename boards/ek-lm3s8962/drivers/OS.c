//***********************************************************************
//
// OS.c
//
// This file includes functions for managing the operating system, such
// as adding periodic tasks.
//
// Created: 2/3/2011 by Katy Loeffler
//
//***********************************************************************

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "drivers/OS.h"



//***********************************************************************
// 
// Global Variables
//
//***********************************************************************
void(*PeriodicTask)(void);
long MsTime = 0;
 

//***********************************************************************
//
// OS_AddPeriodicThread 
//
// \param task is a pointer to the function to be executed at a periodic rate
// \param period is the period in milliseconds
// \param priority is the priority of the task to be used in the NVIC
//
// \return the numerical ID for the periodic thread.  If there is no space 
// for the requested thread, returns NO_THREAD error code.
//
//***********************************************************************
int 
OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority)
{
	PeriodicTask = task;

	//
	// Enable Timer3 module
	//
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);

	//
	// Configure Timer3 as a 32-bit periodic timer.
	//
	TimerConfigure(TIMER3_BASE, TIMER_CFG_32_BIT_PER);

	//
    // Set the Timer3 load value to generate the period specified by the user.
    //
	if(period <= 100 && period >= 1)
	{
    	TimerLoadSet(TIMER3_BASE, TIMER_BOTH, (SysCtlClockGet()/1000)*period);
	}
	else
	{
	 	return FAIL; 
	}

	//
	// Enable the Timer3 interrupt.
	//
	TimerIntEnable(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
	TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
	IntEnable(INT_TIMER3A);

	//
	// Set priority for the timer interrupt.
	//
	if(priority < 8)
	{
		IntPrioritySet(INT_TIMER3A,(unsigned char)priority);
	}
	else
	{
	 	return FAIL; 
	}
	//
    // Start Timer3.
    //
    TimerEnable(TIMER3_BASE, TIMER_BOTH);

	return SUCCESS;
}

//***********************************************************************
//
// OS_ClearMsTime
//
// \param none
// 
// This function resets the counter for the scheduled periodic task.
//
// \return none
//
//***********************************************************************
void
OS_ClearMsTime(void)
{
	MsTime = 0;
}

//***********************************************************************
//
// OS_MsTime
//
// \param none
//
// This function returns the current value of the counter for the 
// scheduled periodic task.
//
// \return counter value.
//
//***********************************************************************
long 
OS_MsTime(void)
{
	return MsTime;
}

//***********************************************************************
//
// Timer 3A Interrupt handler, executes the user defined period task.
//
//***********************************************************************
void
Timer3IntHandler(void)
{
	TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
	PeriodicTask(); 
	MsTime++;	
}
//******************************EOF**************************************




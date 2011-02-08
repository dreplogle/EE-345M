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
#include "driverlib/gpio.h"
#include "drivers/OS.h"



//***********************************************************************
// 
// Global Variables
//
//***********************************************************************
void(*PeriodicTask)(void);
long MsTime = 0;
struct tcb * CurrentThread;     //pointer to the current thread 
 

extern unsigned long PushRegs4to11(unsigned long StkPtr);
extern unsigned long PullRegs4to11(unsigned long StkPtr);
extern unsigned long SetStackPointer(unsigned long StkPtr);

//***********************************************************************
//
// OS_AddPeriodicThread 
//
// \param task is a pointer to the function to be executed at a periodic rate
// \param period is the period in milliseconds
// \param priority is the priority of the task to be used in the NVIC
//
// \return the numerical ID for the periodic thread. Error code FAIL returned
// if \param period or \param priority is out of acceptable range.
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
// OS_PerThreadSwitchInit initializes Timer2 to interrupt at the 
// specified period and priority to perform thread switching
//
// \param period is the period at which threads will be swtiched
// \param priority is the priority for the periodic interrupt
// 
// \return SUCCESS if function was successful and FAIL if either 
// paramter is out of range.
//
//***********************************************************************
int 
OS_PerThreadSwitchInit(unsigned long period, unsigned long priority)
{
	//
	// Enable Timer2 module
	//
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);

	//
	// Configure Timer2 as a 32-bit periodic timer.
	//
	TimerConfigure(TIMER2_BASE, TIMER_CFG_32_BIT_PER);

	//
    // Set the Timer2 load value to generate the period specified by the user.
    //
	if(period <= MAX_THREAD_SW_PER_MS && period >= MIN_THREAD_SW_PER_MS)
	{
    	TimerLoadSet(TIMER2_BASE, TIMER_BOTH, (SysCtlClockGet()/1000)*period);
	}
	else
	{
	 	return FAIL; 
	}

	//
	// Enable the Timer2 interrupt.
	//
	TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
	TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
	IntEnable(INT_TIMER2A);
                                                                                
	//
	// Set priority for the timer interrupt.
	//
	if(priority < 8)
	{
		IntPrioritySet(INT_TIMER2A,(unsigned char)priority);
	}
	else
	{
	 	return FAIL; 
	}
	//
    // Start Timer2.
    //
    TimerEnable(TIMER2_BASE, TIMER_BOTH);

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
// OS_DebugProfileInit initializes GPIO port B pins 0 and 1 for time profiling
//
// \param none.
// \return none.
//
//***********************************************************************
void
OS_DebugProfileInit(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);	
}

//***********************************************************************
//
// OS_DebugB0Set sets PortB0 to 1
//
// \param none.
// \return none.
//
//***********************************************************************
void
OS_DebugB0Set(void)
{
	GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 1);
}
void
OS_DebugB1Set(void)
{
	GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, 1);
}

//***********************************************************************
//
// OS_DebugB0Clear sets PortB0 to 0
//
// \param none.
// \return none.
//
//***********************************************************************
void
OS_DebugB0Clear(void)
{
	GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0);
}
void
OS_DebugB1Clear(void)
{
	GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, 0);
}
//***********************************************************************
//
// Timer 3A Interrupt handler, executes the user defined period task.
//
//***********************************************************************
void
Timer3IntHandler(void)
{
	OS_DebugB0Set();
	TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
	PeriodicTask(); 
	MsTime++;
	OS_DebugB0Clear();	
}

//***********************************************************************
//
// Timer 2A Interrupt handler, switches OS threads.
//
//***********************************************************************

void
OSThSwitchIntHandler(void)
{ 	
	// Disable interrupts for this critical section
	IntMasterDisable();
	
	// Save registers R4 to R11 on the user stack
	(*CurrentThread).stackPtr = PushRegs4to11((*CurrentThread).stackPtr);
	
	// Assign next thread as current thread
	CurrentThread = (*CurrentThread).next;

	// Set the SP for the new TCB and restore registers R4 to R11
	PullRegs4to11((*CurrentThread).stackPtr);
	
	// Re-enable interrupts
	IntMasterEnable();
	
} 
//******************************EOF**************************************





//*****************************************************************************
//
// Filename: OS.c
// Authors: Dustin Replogle, Katy Loeffler   
// Initial Creation Date: February 3, 2011 
// Description: This file includes functions for managing the operating system, such
// as adding periodic tasks.    
// Lab Number. 01    
// TA: Raffaele Cetrulo      
// Date of last revision: February 9, 2011 (for style modifications)      
// Hardware Configuration: default
//
//*****************************************************************************


#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/systick.h"
#include "drivers/OS.h"
#include "string.h"



//***********************************************************************
// 
// Global Variables
//
//***********************************************************************
void(*PeriodicTask)(void);
long MsTime = 0;
TCB * CurrentThread;     //pointer to the current thread
TCB * ThreadList;		 //pointer to the beginning of the circular linked list of TCBs
struct tcb OSThreads[MAX_NUM_OS_THREADS];  //pointers to all the threads in the OS
unsigned char ThreadStacks[MAX_NUM_OS_THREADS][STACK_SIZE];


//***********************************************************************
//
// Function prototypes for internal functions
//
//***********************************************************************
int OS_PerThreadSwitchInit(unsigned long period);

//***********************************************************************
//
// OS_Init
//
//***********************************************************************
void
OS_Init(void)
{
  int threadNum;
  IntMasterDisable();
  //Initialize the OSThreads array to empty
  for(threadNum = 0; threadNum < MAX_NUM_OS_THREADS; threadNum++)
  {
    OSThreads[threadNum].id = DEAD;  
  }
  CurrentThread = NULL;	
  ThreadList = NULL;
	 
} 


//***********************************************************************
//
// OS_AddThread	initializes a TCB in the global TCB array and places the
// new TCB at the end of a circular linked list.
//
// \param task is the program associated with the thread
// \param stackSize is the size of the thread's stack in bytes
// \param id is a numerical identifier for the TCB
//
// \return SUCCESS if there was room for the thread, FAIL otherwise.
//
//***********************************************************************
int
OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned char id)
{
  int threadNum;
  int addNum = 0;
  TCB * searchPtr;

  //OS_CPU_SR  cpu_sr = 0;

  //OS_ENTER_CRITICAL();
  
  //
  // Look for the lowest avaliable slot in the OSThread list, if there is no spot,
  // return with error code.  If there is a spot, initialize it with the given
  // parameters.
  //
  int addSuccess = FAIL;
  for(threadNum = MAX_NUM_OS_THREADS-1; threadNum >= 0; threadNum--)
  {
    if(OSThreads[threadNum].id == DEAD)  //Indicates corresponding thread is inactive
    {
      addNum = threadNum;
	  addSuccess = SUCCESS;
    }
  }

  if(addSuccess == SUCCESS)
  {
    //
    // Initialize the stack pointer for the TCB with the bottom of the 
    // stack.
    //
    OSThreads[addNum].stackPtr = &ThreadStacks[addNum][STACK_SIZE-1];
    //
    // Load initial values onto the stack
    //
	OSThreads[addNum].stackPtr = OS_StackInit(OSThreads[addNum].stackPtr, task);

	OSThreads[addNum].id = id;
  }	   
  
  //
  // Add the new thread to the circular linked list of threads
  //
  if(ThreadList == NULL) // List is empty, this is the first thread
  {
    // Current thread is the start of the list
	ThreadList = &OSThreads[addNum];
	
	// Thread points back to the beginning of the list  
	OSThreads[addNum].next = ThreadList; 

  }
  else
  {
    searchPtr = (*ThreadList).next;

	// Sets searchPtr to the end element on the list
	// Find where the list wraps around  
	while((*searchPtr).next != ThreadList)
	{
	  searchPtr = (*searchPtr).next;
	}

	// Add the new TCB to the end of the list
	searchPtr->next = &OSThreads[addNum];

	// Advance to the current element
	searchPtr = (*searchPtr).next;

	// Set the next pointer of the new TCB to the beginning of the list
	(*searchPtr).next = ThreadList; 
  }

  return addSuccess;
  //OS_EXIT_CRITICAL();
}

//***********************************************************************
//
// PerThreadSwitchInit initializes the SysTick timer to interrupt at the 
// specified period to perform thread switching
//
// \param period is the period at which threads will be swtiched
// 
// \return SUCCESS if function was successful and FAIL if period 
// paramter is out of range.
//
//***********************************************************************
int 
PerThreadSwitchInit(unsigned long period)
{
  // Enable SysTick Interrupts
  SysTickIntEnable();

  // Set the period in ms
  if(period <= MAX_THREAD_SW_PER_MS && period >= MIN_THREAD_SW_PER_MS)
  {
    SysTickPeriodSet((SysCtlClockGet()/1000)*period);
  }
  else
  {  
    return FAIL;
  }
  
  // Enable the SysTick module  
  SysTickEnable();

  return SUCCESS;
}

//***********************************************************************
//
// OS_Launch
//
//***********************************************************************
void
OS_Launch(unsigned long period)
{
  //The first thread is the one at the beginning of the list
  CurrentThread = ThreadList;
  PerThreadSwitchInit(period);
  OS_Launch_Internal(CurrentThread->stackPtr);  //doesn't return  

}

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
// Timer 2A Interrupt handler, enables PendSV for thread switching.
//
//***********************************************************************

void
SysTickThSwIntHandler(void)
{   
  OS_TriggerPendSV();  
}

//***********************************************************************
//
// PendSV handler, switches threads.  Disables interrupts during execution.
//
//***********************************************************************
void 
PendSVHandler(void)
{
  OS_SwitchThreads();
}

//***********************************************************************
//
//  OS_Suspend causes control to be passed to the next thread in the 
//  list without the allotted timeslice timing out.
//
// \param none.
// \return none.
//
//***********************************************************************
void
OS_Suspend(void)
{
  OS_TriggerPendSV();  
}

//***********************************************************************
//
//  OS_InitSemaphore initializes semaphore within the OS to a given 
//  value.
//
// /param
//
//***********************************************************************

void 
OS_InitSemaphore(Sema4Type *semaPt, unsigned int value)
{
  IntMasterDisable();
  (semaPt->value) = value;
  IntMasterEnable();
}

//***********************************************************************
//
//   OS_Signal signals a given semaphore.
//
//***********************************************************************

void 
OS_Signal(Sema4Type *semaPt)
{
  IntMasterDisable();
  (semaPt->value)++;
  IntMasterEnable();
}

//***********************************************************************
//
//   OS_Wait waits for a given semaphore.
//
//***********************************************************************

void 
OS_Wait(Sema4Type *semaPt)
{
  IntMasterDisable();
  while((semaPt->value) == 0)
  {
  	IntMasterEnable();

  	IntMasterDisable();
  }
  (semaPt->value)--;
  IntMasterEnable();
}

//***********************************************************************
//
//   OS_bSignal signals binary semaphore. 
//
//***********************************************************************

void 
OS_bSignal(Sema4Type *semaPt)
{
  IntMasterDisable();
  (semaPt->value) = 1;
  IntMasterEnable();
}

//***********************************************************************
//
//   OS_bSignal waits for binary semaphore.
//
//***********************************************************************

void 
OS_bWait(Sema4Type *semaPt)
{
    IntMasterDisable();
  while((semaPt->value) == 0)
  {
  	IntMasterEnable();

  	IntMasterDisable();
  }
  (semaPt->value) = 0;
  IntMasterEnable();
} 
//******************************EOF**************************************





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
#include "drivers/rit128x96x4.h"
#include "string.h"



//***********************************************************************
// 
// Global Variables
//
//***********************************************************************
void(*PeriodicTask)(void);
void(*ButtonTask)(void);
long MsTime = 0;
TCB * CurrentThread;     //pointer to the current thread
TCB * Sleeper;			 //pointer to a sleeping thread
TCB * ThreadList;		 //pointer to the beginning of the circular linked list of TCBs
struct tcb OSThreads[MAX_NUM_OS_THREADS];  //pointers to all the threads in the OS
unsigned char ThreadStacks[MAX_NUM_OS_THREADS][STACK_SIZE];


//***********************************************************************
//
// Function prototypes for internal functions
//
//***********************************************************************
int PerThreadSwitchInit(unsigned long period);
unsigned char * StackInit(unsigned char * ThreadStkPtr, void(*task)(void));
void SwitchThreads(void);
void LaunchInternal(unsigned char * firstStackPtr);
void TriggerPendSV(void);
//***********************************************************************
//
// OS_Init initializes the array of TCBs, diables interrupts
//
// \param none.
// \return none.
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
OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority)
{
  int threadNum;
  int addNum = 0;
  TCB * searchPtr;
  int addSuccess = FAIL;

  //OS_CPU_SR  cpu_sr = 0;

  //Enter critical
  IntMasterDisable();
  
  //
  // Look for the lowest avaliable slot in the OSThread list, if there is no spot,
  // return with error code.  If there is a spot, initialize it with the given
  // parameters.
  //
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
	OSThreads[addNum].stackPtr = StackInit(OSThreads[addNum].stackPtr, task);

	OSThreads[addNum].id = addNum+1;
	OSThreads[addNum].priority = priority;
	OSThreads[addNum].sleepCount = 0;
	OSThreads[addNum].blockedState = UNBLOCKED;
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

  //Exit critical
  IntMasterEnable();

  return addSuccess;

}

//***********************************************************************
//
// OS_AddButtonTask initializes an interrupt to occur on PF1,
// the select switch.
//
// \param task is the function to be executed when the switch is pressed
// \param priority is the priority for the switch interrupt.
//
// \return SUCCESS if priority is within valid limits, FAIL otherwise.
//
//***********************************************************************
int 
OS_AddButtonTask(void(*task)(void), unsigned long priority)
{
  // Assign the task function pointer to the global pointer to be
  // accessed in the ISR.
  ButtonTask = task;

  // Enable GPIO PortF module
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

  // Make the switch pin an input
  GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);

  // Enable interrupts associated with the switch
  GPIOPinIntEnable(GPIO_PORTF_BASE, GPIO_PIN_1);
  // Set priority for the button interrupt.
  if(priority < 8)
  {
  	IntPrioritySet(INT_GPIOF,(unsigned char)priority);
  }
  else
  {
    return FAIL; 
  }
  GPIOPinIntClear(GPIO_PORTF_BASE, GPIO_PIN_1);
  IntEnable(INT_GPIOF); 
  
  return SUCCESS;
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
  //Enter critical 
  IntMasterDisable();

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

  //Exit critical 
  IntMasterEnable();

  return SUCCESS;
}

//***********************************************************************
//
// OS_Launch starts the OS on the first thread in the circular linked list
// of threads.
//
// \param period is the timeslice of the OS, the execution time share for 
// each thread.
//
// \retun none.
//
//***********************************************************************
void
OS_Launch(unsigned long period)
{
  //The first thread is the one at the beginning of the list
  CurrentThread = ThreadList;
  PerThreadSwitchInit(period);
  LaunchInternal(CurrentThread->stackPtr);  //doesn't return  

}

//***********************************************************************
//
// OS_Sleep puts a thread to sleep for a given number of milliseconds
//
//***********************************************************************
void
OS_Sleep(unsigned long sleepCount)
{
  // Put the thread to sleep by loading a value into the sleep counter
  CurrentThread->sleepCount = sleepCount;

  // Pass control to the next thread
  TriggerPendSV();  
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
  TriggerPendSV();  
}

//***********************************************************************
//
// OS_Kill removes the current thread from the linked list.  This 
// function does not change the CurrentThread pointer so that the
// current thread can finish and the next thread switch will be
// correct.
//
// \param none.
// \return none. 
//
//***********************************************************************
void 
OS_Kill(void)
{
  // Start the search pointer at the beginning of the list
  TCB * searchPtr = ThreadList;

  // Search the list and find the thread that points to the
  // one to be removed.
  while((searchPtr->next) != CurrentThread)
  {
    searchPtr = searchPtr->next;
  }

  // Remove the current thread from the linked list by assigning
  // the pointer of the previous thread to the thread ahead of the
  // element to be removed. 
  searchPtr->next = CurrentThread->next;

  // Indicate to AddThread that this spot is open
  CurrentThread->id = DEAD;

  // Move on to the next thread
  TriggerPendSV();

  // Wait for the PendSV to take effect (very short)
  while(1);
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
// Timer 3A Interrupt handler, executes the user defined period task.
//
//***********************************************************************
void
Timer3IntHandler(void)
{
  IntMasterDisable();
//  OS_DebugB0Set();
  TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
  PeriodicTask(); 
  MsTime++;
//  OS_DebugB0Clear();
  IntMasterEnable();  
}

//***********************************************************************
//
// SysTick handler, enables PendSV for thread switching.
//
//***********************************************************************

void
SysTickThSwIntHandler(void)
{   
  TriggerPendSV();  
}

//***********************************************************************
//
// PendSV handler, switches threads.  Disables interrupts during execution.
//
//***********************************************************************
void 
PendSVHandler(void)
{
  IntMasterDisable();
  SwitchThreads();

  // If the new thread is asleep or blocked, pass control to the next thread
  if((CurrentThread->sleepCount) != 0)
  {
    CurrentThread->sleepCount--;
    OS_Suspend();
  }
  else if((CurrentThread->blockedState) == BLOCKED)
  {
	OS_Suspend();
  }
  IntMasterEnable();
}

//***********************************************************************
//
// Select switch interrupt handler.  Executes task set by OS_AddButtonTask.
//
//***********************************************************************
void
SelectSwitchIntHandler(void)
{
  //Debounce for 10ms
  SysCtlDelay((SysCtlClockGet()/1000)*10);

  //If the button is still pressed, execute the user task.
  if(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1))
  {
    ButtonTask();
  }

  //Wait for the user to release the button
  while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
  
  //Debounce for 10ms
  SysCtlDelay((SysCtlClockGet()/1000)*10);

  //Re-read switch to make sure it is unpressed.
  while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));

  //Clear the interrupt
  GPIOPinIntClear(GPIO_PORTF_BASE, GPIO_PIN_1);
}
 
//******************************EOF**************************************





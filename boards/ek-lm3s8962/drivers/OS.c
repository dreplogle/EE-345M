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
// Modules used:
//		1. GPTimers:
//			a. GPTimer3 is used for periodic tasks (see OS_AddPeriodicThread)
//			b. GPTimer1 is like a general TCNT
//			c. GPTimer0 is used for ADC triggering (see ADC_Collect)
//      2. SysTick: The period of the SysTick is used to dictate the TIMESLICE
//		   for thread switching.  Systick handler causes thread switch.
//		3. ADC: all 4 channels may be accessed.
//		4. UART: UART0 is used for communication with a console.
//
//*****************************************************************************


#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/systick.h"
#include "driverlib/fifo.h"
#include "driverlib/adc.h"
#include "drivers/OS.h"
#include "drivers/rit128x96x4.h"
#include "string.h"

//***********************************************************************
//
// MACROS
//
//***********************************************************************
#define NVIC_PRI11_REG (*((volatile unsigned long *)(0xE000E42C)))	//#46 GPIOF	  Reg:(44-47)
#define NVIC_PRI12_REG (*((volatile unsigned long *)(0xE000E430)))  //#51 Timer3A Reg:(48-51)
#define OS_ENTERCRITICAL(){sr = SRSave(); timeIoff = OS_Time();}
#define OS_EXITCRITICAL(){TimeIbitDisabled += (OS_TimeDifference(OS_Time(),timeIoff)*CLOCK_PERIOD)/1000000; SRRestore(sr);}
//***********************************************************************
// 
// Global Variables
//
//***********************************************************************

//***********************************************************************
// For Periodic Tasks
//***********************************************************************
void(*PeriodicTaskA)(void);
void(*PeriodicTaskB)(void);
unsigned char TimerAFree;
unsigned char TimerBFree;
Sema4Type PeriodicTimerMutex;

unsigned long const JitterSize=JITTERSIZE;
unsigned long JitterHistogramA[JITTERSIZE]={0,};
unsigned long JitterHistogramB[JITTERSIZE]={0,};
long MaxJitterA;             // largest time jitter between interrupts in usec
long MinJitterA;             // smallest time jitter between interrupts in usec
long MaxJitterB;             // largest time jitter between interrupts in usec
long MinJitterB;             // smallest time jitter between interrupts in usec
long PeriodTimerA;
long PeriodTimerB;
unsigned char firstJitterA;
unsigned char firstJitterB;
extern unsigned long NumSamples;

//***********************************************************************
// For Time Profiling
//***********************************************************************
unsigned long CumulativeRunTime;
unsigned long TimeIbitDisabled;
unsigned long RunTimeProfile[NUM_EVENTS][2]; //Collect data for 100 events   RunTimeProfile[Time(ms)][0x04] -> Periodic Thread Start (based on OS.h)
int EventIndex;
unsigned long CumLastTime;  // time at previous interrupt 




// Following code copied from game logic 
//*****************************************************************************
//
// The debounced state of the five push buttons.  The bit positions correspond
// to:
//
//     0 - Up
//     1 - Down
//     2 - Left
//     3 - Right
//     4 - Select
//
//*****************************************************************************
unsigned char g_ucSwitches = 0x1f;

//*****************************************************************************
//
// The vertical counter used to debounce the push buttons.  The bit positions
// are the same as g_ucSwitches.
//
//*****************************************************************************
static unsigned char g_ucSwitchClockA = 0;
static unsigned char g_ucSwitchClockB = 0;

//*****************************************************************************
//
// Handles the SysTick timeout interrupt.
//
//*****************************************************************************

//***********************************************************************
// For Button Tasks
//***********************************************************************
void(*ButtonTask)(void) = NULL;
void(*DownTask)(void) = NULL;
void(*UpTask)(void) = NULL;
void(*LeftTask)(void) = NULL;
void(*RightTask)(void) = NULL;
//***********************************************************************
// For Thread Switcher
//***********************************************************************
TCB * CurrentThread;     //pointer to the current thread
TCB * NextThread;		 //pointer to the next thread to run
TCB * Sleeper;			 //pointer to a sleeping thread
TCB * ThreadList;		 //pointer to the beginning of the circular linked list of TCBs
struct tcb OSThreads[MAX_NUM_OS_THREADS];  //pointers to all the threads in the OS
unsigned char ThreadStacks[MAX_NUM_OS_THREADS][STACK_SIZE];

//***********************************************************************
// Miscellaneous
//***********************************************************************
unsigned long MailBox1;
Sema4Type fifoDataReady;

//***********************************************************************
// OS_Fifo variables, this code segment copied from Valvano, lecture1
//***********************************************************************
typedef unsigned long dataType;
dataType volatile *PutPt; // put next
dataType volatile *GetPt; // get next
dataType static Fifo[MAX_OS_FIFOSIZE];
unsigned long FifoSize;

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
long SRSave (void);
void SRRestore(long sr);
extern void OSuart_Open(void);

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

  // Set the clocking to run from PLL at 50 MHz 
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
  
  //Initialize a TCNT-like countdown timer on timer 1
 
  // Enable Timer1 module
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

  // Configure Timer1 as a 32-bit periodic timer.
  TimerConfigure(TIMER1_BASE, TIMER_CFG_32_BIT_PER);
  TimerLoadSet(TIMER1_BASE, TIMER_A, MAX_TCNT);

  // Start Timer1.
  TimerEnable(TIMER1_BASE, TIMER_A);

  // For profiling the time interrupts are disabled.
  TimeIbitDisabled = 0;

  //Initialize the OSThreads array to empty
  for(threadNum = 0; threadNum < MAX_NUM_OS_THREADS; threadNum++)
  {
    OSThreads[threadNum].id = DEAD;  
  }
  CurrentThread = NULL;	
  ThreadList = NULL;

  OS_DebugProfileInit();
  // Initialize oLED display
  RIT128x96x4Init(1000000);
  // Initialize ADC
  ADC_Open();
  // Initialize serial communication
  OSuart_Open();

  // For periodic threads
  OS_InitSemaphore(&PeriodicTimerMutex, 1);
  TimerAFree = 1;
  TimerBFree = 1;
  firstJitterA = 1;
  firstJitterB = 1;

  //For profiling
  TimeIbitDisabled = 0;
  EventIndex = 0;
  CumulativeRunTime = 0;
  EventIndex = 0;
  CumLastTime = 0;

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

  //Enter critical
  long sr = 0;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();
  
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
	OSThreads[addNum].BlockPt = NULL;
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
  OS_EXITCRITICAL();

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
  GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA,
                     GPIO_PIN_TYPE_STD_WPU);

  // Enable interrupts associated with the switch
  //GPIOPinIntEnable(GPIO_PORTF_BASE, GPIO_PIN_1);
  
  // Set priority for the button interrupt.
  //if(priority < 8)
  //{
  //	IntPrioritySet(INT_TIMER2A,(unsigned char)priority);
  //}
  //else
  //{
	//  return FAIL; 
  //}
  //GPIOPinIntClear(GPIO_PORTF_BASE, GPIO_PIN_1);
  //IntEnable(INT_GPIOF); 
  //IntEnable(INT_TIMER2A);
  return SUCCESS;
}

//***********************************************************************
//
// OS_AddDownTask initializes an interrupt to occur on PE1,
// the down switch.
//
// \param task is the function to be executed when the switch is pressed
// \param priority is the priority for the switch interrupt.
//
// \return SUCCESS if priority is within valid limits, FAIL otherwise.
//
//***********************************************************************
int 
OS_AddDownTask(void(*task)(void), unsigned long priority)
{
  // Assign the task function pointer to the global pointer to be
  // accessed in the ISR.
  DownTask = task;
  
  // Enable GPIO PortE module
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

  // Make the switch pin an input
  GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_1);
  GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA,
                     GPIO_PIN_TYPE_STD_WPU);

  // Enable interrupts associated with the switch
  //GPIOPinIntEnable(GPIO_PORTE_BASE, GPIO_PIN_1);
  
  // Set priority for the button interrupt.
  //if(priority < 8)
  //{
  //	IntPrioritySet(INT_TIMER2A,(unsigned char)priority);
  //}
  //else
  //{
  //	  return FAIL; 
  //}
  //GPIOPinIntClear(GPIO_PORTE_BASE, GPIO_PIN_1);
  //IntEnable(INT_GPIOE); 
  //IntEnable(INT_TIMER2A);

  return SUCCESS;
}

//***********************************************************************
//
// OS_RemoveButtonTask removes the interrupt occuring on PE1,
// the down switch.
//
// \param task is the function to be executed when the switch is pressed
// \param priority is the priority for the switch interrupt.
//
// \return SUCCESS if priority is within valid limits, FAIL otherwise.
//
//***********************************************************************

OS_RemoveButtonTask(void(*task)(void))
{
  // Remove the task pointer associated with the down button
  DownTask = NULL;
  
  // Disable GPIO PortF module
  //SysCtlPeripheralDisable(SYSCTL_PERIPH_GPIOE);

  // Disable interrupts associated with the switch
  GPIOPinIntDisable(GPIO_PORTE_BASE, GPIO_PIN_1);
  
  GPIOPinIntClear(GPIO_PORTE_BASE, GPIO_PIN_1);
  //IntDisable(INT_GPIOE); 
  
  return SUCCESS;

}

//***********************************************************************
//
// OS_AddPeriodicThread 
//
// \param task is a pointer to the function to be executed at a periodic rate
// \param period is the period to be loaded to timer register
// \param priority is the priority of the task to be used in the NVIC
//
// \return the numerical ID for the periodic thread. Error code FAIL returned
// if \param priority is out of acceptable range.
//
//***********************************************************************
int 
OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority)
{
  //Enter critical 
  long sr = 0;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();

  //Initialization for Jitter calculation:
  MaxJitterA = 0;       // OS_Time in 20ns units
  MinJitterA = 10000000;
  MaxJitterB = 0;       // OS_Time in 20ns units
  MinJitterB = 10000000;

  //Check for timer availiability
  OS_bWait(&PeriodicTimerMutex);
  if(TimerAFree){
    TimerAFree = 0;  //False
	OS_bSignal(&PeriodicTimerMutex);
	PeriodicTaskA = task;
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
	TimerDisable(TIMER3_BASE, TIMER_A);
    // Set the global timer configuration.
    HWREG(TIMER3_BASE + 0x00000000) = (TIMER_CFG_16_BIT_PAIR|TIMER_CFG_A_PERIODIC) >> 24;
    // Set the configuration of the A and B timers.  Note that the B timer
    // configuration is ignored by the hardware in 32-bit modes.
    HWREG(TIMER3_BASE + 0x00000004) = (TIMER_CFG_16_BIT_PAIR|TIMER_CFG_A_PERIODIC) & 255;
    TimerLoadSet(TIMER3_BASE, TIMER_A, period);
	PeriodTimerA = (long)period;
    TimerIntEnable(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    IntEnable(INT_TIMER3A);                                                                                
    if(priority < 8){
      IntPrioritySet(INT_TIMER3A,(((unsigned char)priority)<<5)&0xF0);
    }
    else{
//      IntMasterEnable();
	   OS_EXITCRITICAL();
	   return FAIL; 
    }
    TimerEnable(TIMER3_BASE, TIMER_A);
	//Exit critical 
//    IntMasterEnable();
    OS_EXITCRITICAL();
	return SUCCESS;
  }
  else if(TimerBFree){
    TimerBFree = 0;  //False
	OS_bSignal(&PeriodicTimerMutex);
	PeriodicTaskB = task;
	TimerDisable(TIMER3_BASE, TIMER_B);
    // Set the global timer configuration.
    HWREG(TIMER3_BASE + 0x00000000) = (TIMER_CFG_16_BIT_PAIR|TIMER_CFG_B_PERIODIC) >> 24;
    // Set the configuration of the A and B timers.  Note that the B timer
    // configuration is ignored by the hardware in 32-bit modes.
    HWREG(TIMER3_BASE + 0x00000008) = ((TIMER_CFG_16_BIT_PAIR|TIMER_CFG_B_PERIODIC)>> 8) & 255;
    TimerLoadSet(TIMER3_BASE, TIMER_B, period);
	PeriodTimerB = (long)period;
    TimerIntEnable(TIMER3_BASE, TIMER_TIMB_TIMEOUT);
    TimerIntClear(TIMER3_BASE, TIMER_TIMB_TIMEOUT);
    IntEnable(INT_TIMER3B);                                                                                
    if(priority < 8){
      IntPrioritySet(INT_TIMER3B,(((unsigned char)priority)<<5)&0xF0);
    }
    else{
//       IntMasterEnable();
      OS_EXITCRITICAL();
	  return FAIL; 
    }
    TimerEnable(TIMER3_BASE, TIMER_B);
	//Exit critical 
//    IntMasterEnable();
    OS_EXITCRITICAL();
	return SUCCESS;
  }
  
  OS_bSignal(&PeriodicTimerMutex);
  //Exit critical 
  OS_EXITCRITICAL();
  return FAIL;
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
  long sr = 0;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();
  // Put the thread to sleep by loading a value into the sleep counter
  CurrentThread->sleepCount = sleepCount;

  // Pass control to the next thread
  OS_EXITCRITICAL();

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
  long sr = 0;
  unsigned long timeIoff;
  TCB * searchPtr = ThreadList;
  OS_ENTERCRITICAL();
  // Start the search pointer at the beginning of the list


  // Search the list and find the thread that points to the
  // one to be removed.
  while((searchPtr->next) != CurrentThread)
  {
    searchPtr = searchPtr->next;
  }

  // Do not kill the thread if it is the last element in the list
  if(CurrentThread->next != CurrentThread)
  {
    // Remove the current thread from the linked list by assigning
    // the pointer of the previous thread to the thread ahead of the
    // element to be removed. 
    searchPtr->next = CurrentThread->next;

    // Reassign the beginning of the list if removing first element 
    if(ThreadList == CurrentThread)
    {
      ThreadList = CurrentThread->next;
    }
	// Indicate to AddThread that this spot is open
    CurrentThread->id = DEAD;
  }

  OS_EXITCRITICAL();

  // Move on to the next thread
  TriggerPendSV();

  // Wait for the PendSV to take effect (very short)
  while(1);
}

//***********************************************************************
//
// OS_Id
//
//***********************************************************************
unsigned char
OS_Id(void)
{
  return CurrentThread->id;
}

//***********************************************************************
//
// OS_Fifo_Init
//
//***********************************************************************
void
OS_Fifo_Init(unsigned int size)
{
  long sr = 0;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();

  PutPt = GetPt = &Fifo[0];
  FifoSize = size;
  OS_InitSemaphore(&fifoDataReady,0);
  

  OS_EXITCRITICAL();  
}

//***********************************************************************
//
// OS_Fifo_Get
//
//***********************************************************************
unsigned int
OS_Fifo_Get(unsigned long * dataPtr)
{
  if(PutPt == GetPt ){
    return FAIL;// Empty if PutPt=GetPt
  }
  else{
  OS_Wait(&fifoDataReady);
  *dataPtr = *(GetPt++);
    if(GetPt==&Fifo[FifoSize]){
      GetPt = &Fifo[0]; // wrap
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
OS_Fifo_Put(unsigned long data)
{
  dataType volatile *nextPutPt;
  nextPutPt = PutPt+1;
  if(nextPutPt ==&Fifo[FifoSize]){
    nextPutPt = &Fifo[0]; // wrap
  }
  if(nextPutPt == GetPt){
    return FAIL; // Failed, fifo full
  }
  else{
    *(PutPt) = data; // Put
    PutPt = nextPutPt; // Success, update
	OS_Signal(&fifoDataReady);
    return SUCCESS;
  }
}

//***********************************************************************
//
// OS_MailBox_Init
//
//***********************************************************************
void
OS_MailBox_Init(void)
{
  MailBox1 = 0;
}

//***********************************************************************
//
// OS_MailBox_Send
//
//***********************************************************************
void
OS_MailBox_Send(unsigned long data)
{
  MailBox1 = data;
}

//***********************************************************************
//
// OS_MailBox_Recv
//
//***********************************************************************
unsigned long
OS_MailBox_Recv(void)
{
  return MailBox1;
}

//***********************************************************************
//
// OS_MsTime
//
// \param none
//
// This function returns the current value of the SysTick timer used to 
// switch threads.
//
// \return counter value.
//
//***********************************************************************
unsigned long OS_Time(void)
{
  return TimerValueGet(TIMER1_BASE, TIMER_A);
}

//***********************************************************************
//
// OS_TimeDifference returns the time difference in usec.
//
//***********************************************************************
long OS_TimeDifference(unsigned long time1, unsigned long time2)
//                                   thisTime          	  LastTime
{
  if(time2 >= time1)
  {
  	return (long)(time2-time1);
  }
  else
  {
    return (long)(time2 + (MAX_TCNT-time1));      
  } 
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
  GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);  
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
  long sr = 0;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();

  (semaPt->value) = value;

  OS_EXITCRITICAL();
}

//***********************************************************************
//
//   OS_Signal signals a given semaphore.
//
//***********************************************************************

void 
OS_Signal(Sema4Type *semaPt)
{
  TCB * temp = ThreadList;  // pointer to all threads in the threadlist
  TCB * toUnblock = NULL;          // pointer to thread to unblock
  unsigned long highest_priority = 100; 
  long sr = 0;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();
  (semaPt->value)++;
  if((semaPt->value) <= 0)
  {
     do
     {
       if(((temp->BlockPt) == semaPt) && (temp->priority < highest_priority)) 
       {
         toUnblock = temp;   // If a thread is blocked by this semaphore and 
                             // is the highest priority, unblock the thread
         highest_priority = toUnblock->priority;
       }
       temp = temp->next;
     }
     while(temp != ThreadList);
     if(toUnblock->BlockPt != NULL && toUnblock != NULL)
     {
       toUnblock->BlockPt = NULL;
     }
  }
  OS_EXITCRITICAL();
}

//***********************************************************************
//
//   OS_Wait waits for a given semaphore.
//
//***********************************************************************

void 
OS_Wait(Sema4Type *semaPt)
{
  long sr;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();
  (semaPt->value)--;
  if((semaPt->value) < 0)
  {
     CurrentThread->BlockPt = semaPt;
     OS_Suspend();
  }
  OS_EXITCRITICAL();
}

//***********************************************************************
//
//   OS_bSignal signals binary semaphore. 
//
//***********************************************************************

void 
OS_bSignal(Sema4Type *semaPt)
{
  (semaPt->value) = 1;
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
  SysTickPeriodSet(period);

  // Set Systick priority to high, PendSV priority to low
  IntPrioritySet(FAULT_SYSTICK,(((unsigned char)0)<<5)&0xF0);
  IntPrioritySet(FAULT_PENDSV,(((unsigned char)7)<<5)&0xF0);
  
  // Enable the SysTick module  
  SysTickEnable();

  return SUCCESS;
}

//***********************************************************************
//
// Timer 3A Interrupt handler, executes the user defined period task.
//
//***********************************************************************
long timeCheck;
void
Timer3AIntHandler(void)
{
  int index;
  static unsigned long LastTime;  // time at previous interrupt  
  static unsigned long thisTime;         // time at current interrupt
  long jitter;                    // time between measured and expected

  thisTime = OS_Time();
  CumulativeRunTime += (OS_TimeDifference(thisTime, CumLastTime)*CLOCK_PERIOD)/1000;	//in ms
  CumLastTime = thisTime;

  if(EventIndex < NUM_EVENTS)
  {
    RunTimeProfile[EventIndex][0] = CumulativeRunTime;
    RunTimeProfile[EventIndex][1] = PER_THREAD_START;
	EventIndex++;
  }

  if(NumSamples < RUNLENGTH){
    // Jitter calculation:
    thisTime = OS_Time();       // current time, 20 ns
    timeCheck = OS_TimeDifference(thisTime, LastTime);
    jitter = ((OS_TimeDifference(thisTime, LastTime)-PeriodTimerA)*CLOCK_PERIOD)/1000;  // in usec
	if(!firstJitterA){
	  if(jitter > MaxJitterA){
        MaxJitterA = jitter;
      }
      if(jitter < MinJitterA){
        MinJitterA = jitter;
      }        // jitter should be 0
      index = jitter+JITTERSIZE/2;   // us units
      if(index<0)index = 0;
      if(index>=JitterSize)index = JITTERSIZE-1;
      JitterHistogramA[index]++; 
	}
	firstJitterA = 0;
  LastTime = thisTime;
  }
  // Execute the periodic thread
  TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
  PeriodicTaskA();
    
  thisTime = OS_Time();
  CumulativeRunTime += ((OS_TimeDifference(thisTime, CumLastTime)*CLOCK_PERIOD)/1000);	//in ms
  CumLastTime = thisTime;
  
  if(EventIndex < NUM_EVENTS)
  {
    RunTimeProfile[EventIndex][0] = CumulativeRunTime;
    RunTimeProfile[EventIndex][1] = PER_THREAD_END;
	EventIndex++;
  } 

}

//***********************************************************************
//
// Timer 3B Interrupt handler, executes the user defined period task.
//
//***********************************************************************
void
Timer3BIntHandler(void)
{
  static unsigned long LastTime;  // time at previous interrupt  
  static unsigned long thisTime;         // time at current interrupt 
  long jitter;                    // time between measured and expected
  int index;
 
  thisTime = OS_Time();
  CumulativeRunTime += ((OS_TimeDifference(thisTime, CumLastTime)*CLOCK_PERIOD)/1000);	//in ms
  CumLastTime = thisTime;
  
  if(EventIndex < NUM_EVENTS)
  {
    RunTimeProfile[EventIndex][0] = CumulativeRunTime;
    RunTimeProfile[EventIndex][1] = PER_THREAD_START;
	EventIndex++;
  }

  if(NumSamples < RUNLENGTH){
    // Jitter calculation:
    thisTime = OS_Time();       // current time, 20 ns
    timeCheck = OS_TimeDifference(thisTime, LastTime);
    jitter = ((OS_TimeDifference(thisTime, LastTime)-PeriodTimerB)*CLOCK_PERIOD)/1000;  // in usec
	if(!firstJitterB){
	  if(jitter > MaxJitterB){
        MaxJitterB = jitter;
      }
      if(jitter < MinJitterB){
        MinJitterB = jitter;
      }        // jitter should be 0
      index = jitter+JITTERSIZE/2;   // us units
      if(index<0)index = 0;
      if(index>=JitterSize)index = JITTERSIZE-1;
      JitterHistogramB[index]++; 
      LastTime = thisTime;
	}
	firstJitterB = 0;
  }
  // Execute Periodic task
  TimerIntClear(TIMER3_BASE, TIMER_TIMB_TIMEOUT);
  PeriodicTaskB();

  thisTime = OS_Time();
  CumulativeRunTime += ((OS_TimeDifference(thisTime, CumLastTime)*CLOCK_PERIOD)/1000);	//in ms
  CumLastTime = thisTime;
  
  if(EventIndex < NUM_EVENTS)
  {
    RunTimeProfile[EventIndex][0] = CumulativeRunTime;
    RunTimeProfile[EventIndex][1] = PER_THREAD_END;
	EventIndex++;
  }   
}

//***********************************************************************
//
// Timer 2A Interrupt handler, executes the user defined period task.
//
//***********************************************************************
void
Timer2IntHandler(void)
{

}

//***********************************************************************
//
// SysTick handler, enables PendSV for thread switching.
//
//***********************************************************************

void
SysTickThSwIntHandler(void)
{   
  unsigned long ulData, ulDelta; 
  long sr = 0;
  unsigned long timeIoff;
  OS_ENTERCRITICAL();

    //
    // Read the state of the push buttons.
    //
    ulData = (GPIOPinRead(GPIO_PORTE_BASE, (GPIO_PIN_0 | GPIO_PIN_1 |
                                            GPIO_PIN_2 | GPIO_PIN_3)) |
              (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1) << 3));

    //
    // Determine the switches that are at a different state than the debounced
    // state.
    //
    ulDelta = ulData ^ g_ucSwitches;

    //
    // Increment the clocks by one.
    //
    g_ucSwitchClockA ^= g_ucSwitchClockB;
    g_ucSwitchClockB = ~g_ucSwitchClockB;

    //
    // Reset the clocks corresponding to switches that have not changed state.
    //
    g_ucSwitchClockA &= ulDelta;
    g_ucSwitchClockB &= ulDelta;

    //
    // Get the new debounced switch state.
    //
    g_ucSwitches &= g_ucSwitchClockA | g_ucSwitchClockB;
    g_ucSwitches |= (~(g_ucSwitchClockA | g_ucSwitchClockB)) & ulData;

    //
    // Determine the switches that just changed debounced state.
    //
    ulDelta ^= (g_ucSwitchClockA | g_ucSwitchClockB);
  
  //If the button is still pressed, execute the user task.
  if((ulDelta & 0x10) && !(g_ucSwitches & 0x10) && (ButtonTask != NULL))
  {
    ButtonTask();
  }
  if((ulDelta & 0x08) && !(g_ucSwitches & 0x08) && (RightTask != NULL))
  {
    RightTask();
  }
  if((ulDelta & 0x04) && !(g_ucSwitches & 0x04) && (LeftTask != NULL))
  {
    LeftTask();
  }
  if((ulDelta & 0x02) && !(g_ucSwitches & 0x02) && (DownTask != NULL))
  {
    DownTask();
  }
  if((ulDelta & 0x01) && !(g_ucSwitches & 0x01) && (UpTask != NULL))
  {
    UpTask();
  }
  //Wait for the user to release the button
  //while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
  
  //Debounce for 10ms
  //SysCtlDelay((SysCtlClockGet()/1000)*10);

  //Re-read switch to make sure it is unpressed.
  //while(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));

  //Clear the interrupt
  /*GPIOPinIntClear(GPIO_PORTF_BASE, GPIO_PIN_1);
  GPIOPinIntClear(GPIO_PORTE_BASE, GPIO_PIN_1);
  GPIOPinIntClear(GPIO_PORTE_BASE, GPIO_PIN_1);
  GPIOPinIntClear(GPIO_PORTE_BASE, GPIO_PIN_1);
  GPIOPinIntClear(GPIO_PORTE_BASE, GPIO_PIN_1);*/


  //TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
  //GPIOPinIntEnable(GPIO_PORTF_BASE, GPIO_PIN_1);

  OS_EXITCRITICAL();  
  
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
  TCB * TempPt;
  static unsigned long RunPriorityLevel;
  long sr;
  unsigned long timeIoff;
  static unsigned long thisTime;
  OS_ENTERCRITICAL();
  
  //Determine the priority level to run	and update sleeping threads
  RunPriorityLevel = 100;     //Initialize to rediculously low priority
  TempPt = ThreadList;
  do
  {
    // Find lowest active priority level
    if((TempPt->BlockPt == NULL)&&(TempPt->priority < RunPriorityLevel)&&(TempPt->sleepCount == 0))
	{
        RunPriorityLevel = TempPt->priority;
	}
	  // Decrement sleep counter on sleeping threads
    if(TempPt->sleepCount > 0)
	{
      (TempPt->sleepCount)--;
	}
	TempPt = TempPt->next;
  }while(TempPt != ThreadList);


  // Find the next thread that is not...
  // (1) Sleeping
  // (2) Blocked or
  // (3) Too low in priority
  NextThread = CurrentThread;
  do
  {
    NextThread = NextThread->next;
  }while(((NextThread->sleepCount != 0)||(NextThread->BlockPt != NULL)||(NextThread->priority > RunPriorityLevel))&&(NextThread!=CurrentThread));
  
  HWREG(NVIC_ST_CURRENT) = 0;

  thisTime = OS_Time();
  CumulativeRunTime += ((OS_TimeDifference(thisTime, CumLastTime)*CLOCK_PERIOD)/1000);	//in ms
  CumLastTime = thisTime;

  if(EventIndex < NUM_EVENTS)
  {
    RunTimeProfile[EventIndex][0] = CumulativeRunTime;
    RunTimeProfile[EventIndex][1] = FOREGROUND_THREAD_START;
	  EventIndex++;
  }
  OS_EXITCRITICAL();
 
  SwitchThreads();  
}

//***********************************************************************
//
// Select switch interrupt handler.  Executes task set by OS_AddButtonTask.
//
//***********************************************************************
void
SwitchIntHandler(void)
{

}
 
//******************************EOF**************************************




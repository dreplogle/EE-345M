//*****************************************************************************
//
// Filename: OS.c
// Authors: Dustin Replogle, Katy Loeffler   
// Initial Creation Date: February 3, 2011 
// Description: Header file for OS.h    
// Lab Number. 01    
// TA: Raffaele Cetrulo      
// Date of last revision: February 8, 2011    
// Hardware Configuration: default
//
//*****************************************************************************

#define SUCCESS 1
#define FAIL 0
#define MAX_THREAD_SW_PER_MS 1000
#define MIN_THREAD_SW_PER_MS 1

typedef struct tcb{
  unsigned long stackPtr;
  struct tcb * next;
  unsigned char id;
  unsigned char sleepState;
  unsigned long priority;
  unsigned char blockedState;
}TCB;

typedef struct Sema4Type{
  unsigned short value;
}Sema4Type;

//*************************************************************
//
// Function Prototypes
//
//*************************************************************

extern int OS_AddPeriodicThread(void(*task)(void), 
  	  	  	      unsigned long period, 
  	  	  	  	unsigned long priority);
extern int OS_PerThreadSwitchInit(unsigned long period);
extern void OS_ClearMsTime(void);
extern long OS_MsTime(void);
extern void OS_DebugProfileInit(void);
extern void OS_DebugB0Set(void);
extern void OS_DebugB1Set(void);
extern void OS_DebugB0Clear(void);
extern void OS_DebugB1Clear(void);
extern void OS_InitSemaphore(Sema4Type *semaPt, unsigned int value);
extern void OS_Signal(Sema4Type *semaPt);
extern void OS_Wait(Sema4Type *semaPt);
extern void OS_bSignal(Sema4Type *semaPt);
extern void OS_bWait(Sema4Type *semaPt);

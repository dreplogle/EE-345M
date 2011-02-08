//*************************************************************
//
// OS.h
//
// Created: 2/3/2011 by Katy Loeffler
//
//*************************************************************

#define SUCCESS 1
#define FAIL 0
#define MAX_THREAD_SW_PER_MS 1000
#define MIN_THREAD_SW_PER_MS 1

typedef struct tcb{
	unsigned long stackPtr;
	struct tcb * next;
	unsigned char id;
	unsigned char sleepState;
	unsigned long prority;
	unsigned char blockedState;
}TCB;

//*************************************************************
//
// Function Prototypes
//
//*************************************************************

extern int OS_AddPeriodicThread(void(*task)(void), 
							    unsigned long period, 
								unsigned long priority);
extern int OS_PerThreadSwitchInit(unsigned long period,
								  unsigned long priority);
extern void OS_ClearMsTime(void);
extern long OS_MsTime(void);
extern void OS_DebugProfileInit(void);
extern void OS_DebugB0Set(void);
extern void OS_DebugB1Set(void);
extern void OS_DebugB0Clear(void);
extern void OS_DebugB1Clear(void);

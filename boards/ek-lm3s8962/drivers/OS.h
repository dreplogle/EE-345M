//*************************************************************
//
// OS.h
//
// Created: 2/3/2011 by Katy Loeffler
//
//*************************************************************

#define SUCCESS 1
#define FAIL 0

//*************************************************************
//
// Function Prototypes
//
//*************************************************************

extern int OS_AddPeriodicThread(void(*task)(void), 
							    unsigned long period, 
								unsigned long priority);
extern void OS_ClearMsTime(void);
extern long OS_MsTime(void);
extern void OS_DebugProfileInit(void);
extern void OS_DebugB0Set(void);
extern void OS_DebugB1Set(void);
extern void OS_DebugB0Clear(void);
extern void OS_DebugB1Clear(void);

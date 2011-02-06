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

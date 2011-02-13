//*****************************************************************************
//
// OS.h
//
//*****************************************************************************

#define SUCCESS 1
#define FAIL 0
#define DEAD 0xFF
#define MAX_NUM_OS_THREADS 10
#define STACK_SIZE 128    			//Stack size in bytes
#define MAX_THREAD_SW_PER_MS 1000
#define MIN_THREAD_SW_PER_MS 1

typedef struct tcb{
  unsigned char * stackPtr;
  struct tcb * next;
  unsigned char id;
  unsigned char sleepState;
  unsigned long priority;
  unsigned char blockedState;
}TCB;

typedef struct Sema4Type{
  unsigned short value;
}Sema4Type;


//*****************************************************************************
//
// Function Prototypes
//
//*****************************************************************************

extern int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority);
extern int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned char id);
extern void OS_Init(void);
extern unsigned char * OS_StackInit(unsigned char * ThreadStkPtr, void(*task)(void));
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





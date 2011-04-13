//*****************************************************************************
//
// OS.h
//
//*****************************************************************************

#define SUCCESS 1
#define FAIL 0
#define DEAD 0xFF
#define BLOCKED 1
#define UNBLOCKED 0
#define MAX_NUM_OS_THREADS 5
#define STACK_SIZE 2048 			//Stack size in bytes
#define MAX_THREAD_SW_PER_MS 1000
#define MIN_THREAD_SW_PER_MS 1
#define MAX_OS_FIFOSIZE 128 		// can be any size
#define CLOCK_PERIOD 20  			// clock period in ns
#define MAX_TCNT  0x0EE6B280			// @50Mhz, this is 5 seconds
#define JITTERSIZE 64
#define NUM_EVENTS 100
#define PER_THREAD_START 0x04
#define PER_THREAD_END   0x08
#define FOREGROUND_THREAD_START 0x03

#define TIME_1MS 50000		  		// #clock cycles per ms in 50MHz mode
#define TIMESLICE TIME_1MS*2  		//Thread switching period in ms
#define PERIOD TIME_1MS/2       	// 2kHz sampling period in system time units
// 10-sec finite time experiment duration 
#define RUNLENGTH 10000   // display results and quit when NumSamples==RUNLENGTH

typedef struct tcb{
  unsigned char * stackPtr;
  struct tcb * next;
  unsigned char id;
  unsigned long sleepCount;
  unsigned long priority;
  struct Sema4Type * BlockPt;
}TCB;

typedef struct Sema4Type{
  short value;
}Sema4Type;


//*****************************************************************************
//
// Function Prototypes
//
//*****************************************************************************

extern void OS_Init(void);
extern int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority);
extern int OS_AddButtonTask(void(*task)(void), unsigned long priority);
extern int OS_AddDownTask(void(*task)(void), unsigned long priority);
extern int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority);
extern void OS_Launch(unsigned long period);
extern void OS_Sleep(unsigned long period);
extern void OS_Suspend(void);
extern void OS_Kill(void);
extern unsigned char OS_Id(void);
extern void OS_Fifo_Init(unsigned int size);
extern unsigned int OS_Fifo_Get(unsigned long * dataPtr);
extern unsigned int OS_Fifo_Put(unsigned long data);
extern void OS_MailBox_Init(void);
extern void OS_MailBox_Send(unsigned long data);
extern unsigned long OS_MailBox_Recv(void);
extern unsigned long OS_Time(void);
extern long OS_TimeDifference(unsigned long time1, unsigned long time2);
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





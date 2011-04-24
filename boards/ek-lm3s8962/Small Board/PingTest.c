#include "inc/hw_types.h"
#include "drivers/ping.h"
#include "driverlib/timer.h"
#include "inc/hw_timer.h"
#include "inc/hw_memmap.h"
#include "drivers/os.h"
#include "inc/hw_ints.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"


unsigned long NumCreated;



void Tachometer_InputCapture(void){}//delete this function when you get the real tachometer input
//capture function



// ******** Ping ************
// Initializes Ping sensor by calling Ping_Init.
// Called by main program.
// Inputs: none
// Outputs: none
int Ping(void) //Add code to test Ping functions
{


	Ping_Init(TIMER2_BASE, TIMER_A); //Must do this after OS_AddPeriodicThread in order
	//to set the correct timer 2A prescale


}




unsigned long Count1 = 0;
void Thread1(void)
{
	while(1)
	{
		Count1++;
	}
}

unsigned long Count2 = 0;
void Thread2(void)
{
	while(1)
	{
		Count2++;
	}
}

unsigned long Count3 = 0;
void Thread3(void)
{
	while(1)
	{
		Count3++;
	}
}

void Interpreter(void);

unsigned long testFifoData = 0;
void FifoProducer(void)
{
	TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
	Ping_Fifo_Put(testFifoData);
	testFifoData++;
}


unsigned long testFifoGet[100];
unsigned long testFifoGetIndex = 0;
void FifoConsumer(void)
{
	unsigned char data = Ping_Fifo_Get();
	if ((data != 0) && (testFifoGetIndex < 100))
	{
		testFifoGet[testFifoGetIndex] = data;
		testFifoGetIndex++;
	}
}
int fifomain(void)
{
	unsigned char priority;
	Ping_Fifo_Init();

	SysCtlClockSet(SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
	TimerConfigure(TIMER2_BASE, TIMER_CFG_16_BIT_PAIR | TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC);

	TimerPrescaleSet(TIMER2_BASE, TIMER_A, 0);
	TimerLoadSet(TIMER2_BASE, TIMER_A, 50000); 
	
	//Set timer to interrupt and set priority
	IntEnable(INT_TIMER2A);
	priority = 0;
	priority = priority << 5;
	IntPrioritySet(INT_TIMER2A, priority);
	TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT); 

	//Enable the timer    
	TimerEnable(TIMER2_BASE, TIMER_A);

	while(1)
	{
		FifoConsumer();
	}
}

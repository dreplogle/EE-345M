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

#define PING_PERIOD 50000 //This is in units of 2000 ns increments

void Tachometer_InputCapture(void){}//delete this function when you get the real tachometer input
//capture function

int main(void) //Add code to test Ping functions
{
	unsigned char priority;

	//Initialize timer 0 so that input capture can be used
 	 SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

	//Initialize timer 1 so that OS_Time can be used
	// Enable Timer1 module
 	 SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

 	 // Configure Timer1 as a 32-bit periodic timer.
 	 TimerConfigure(TIMER1_BASE, TIMER_CFG_32_BIT_PER);
 	 TimerLoadSet(TIMER1_BASE, TIMER_A, MAX_TCNT);

	 TimerEnable(TIMER1_BASE, TIMER_A);

	SysCtlClockSet(SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);

	Ping_Init(TIMER2_BASE, TIMER_A); //Must do this after OS_AddPeriodicThread in order
	//to set the correct timer 2A prescale

	TimerConfigure(TIMER2_BASE, TIMER_CFG_16_BIT_PAIR | TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC);

	TimerPrescaleSet(TIMER2_BASE, TIMER_A, 0);
	TimerLoadSet(TIMER2_BASE, TIMER_A, PING_PERIOD); 
	
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
		pingConsumer();
	}
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
int OSTestmain(void)
{
	OS_Init();
	OS_Fifo_Init(512);
	OS_AddThread(&Thread1, 1024, 1);
	OS_AddThread(&Thread2, 1024, 1);
	OS_AddThread(&Thread3, 1024, 1);
//	OS_AddThread(&Interpreter, 1024, 4);
	OS_Launch(TIMESLICE);

}

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

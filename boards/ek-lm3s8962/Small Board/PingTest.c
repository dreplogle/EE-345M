#include "inc/hw_types.h"
#include "drivers/ping.h"
#include "driverlib/timer.h"
#include "inc/hw_timer.h"
#include "inc/hw_memmap.h"
#include "drivers/os.h"

unsigned long NumCreated;

#define PING_PERIOD 50000 //This is in units of 2000 ns increments

int main(void) //Add code to test Ping functions
{
	OS_Init();
	OS_AddPeriodicThread(&pingProducer, PING_PERIOD, 0);

	Ping_Init(TIMER3_BASE, TIMER_A); //Must do this after OS_AddPeriodicThread in order
	//to set the correct timer 3A prescale

	OS_AddThread(&pingConsumer, 800, 0);
	OS_Launch(TIMESLICE);

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

int OSTestMain(void)
{
	OS_Init();
	OS_AddThread(&Thread1, 800, 1);
	OS_AddThread(&Thread2, 800, 1);
	OS_AddThread(&Thread3, 800, 1);
	OS_Launch(TIMESLICE);

}

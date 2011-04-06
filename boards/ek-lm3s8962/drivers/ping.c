#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_sysctl.h"
#include "hw_timer.h"
#include "os.h"
#include "timer.h"
#include "gpio.h"
#include "sysctl.h"
#include "interrupt.h"
#include "hw_ints.h"
#include <stdio.h>

#define SYSCTL_RCGC2_R     (*((volatile unsigned long *)0x400FE108)) 
#define GPIOA_AFSEL_R		(*((volatile unsigned long *)0x40004420)) 
#define FIVE_USEC 125 //Uses a 40 ns clock period
#define PIN_6_WRITE 0x40 
#define PIN_5_WRITE 0x20
#define MAX_16_BIT_TCNT 0xFFFF
#define NUMBER_OF_INCS_IN_USEC 25 
#define SPEED_OF_SOUND 34029 //speed of sound in 10 nm/us units
#define NUMBER_OF_NM_IN_MM 1000000
#define CCP1_TIMER_PRESCALE 0
#define MAX_DISTANCE 446

unsigned long Ping_Data_Lost = 0;
Sema4Type Ping_Fifo_Available;
unsigned long NumSamples = 0;
unsigned long DataLost = 0;

//*****************************************************************************
//
// Ping FIFO
//
//*****************************************************************************
#define PING_BUF_SIZE   1024	     /*** Must be a power of 2 (2,4,8,16,32,64,128,256,512,...) ***/

#if PING_BUF_SIZE < 2
#error PING_BUF_SIZE is too small.  It must be larger than 1.
#elif ((PING_BUF_SIZE & (PING_BUF_SIZE-1)) != 0)
#error PING_BUF_SIZE must be a power of 2.
#endif


struct buf_st {
  unsigned int in;                                // Next In Index
  unsigned int out;                               // Next Out Index
  short buf [PING_BUF_SIZE];                           // Buffer
};

struct buf_st pingbuf = { 0, 0, };
#define PING_BUFLEN ((unsigned short)(pingbuf.in - pingbuf.out))



// ******** Ping_Fifo_Init ************
// Initializes Ping data FIFO
// Inputs: size of the FIFO
// Outputs: none
void Ping_Fifo_Init(void){
//	OS_InitSemaphore(&Ping_Fifo_Available, 0);
	pingbuf.in = 0;
  	pingbuf.out = 0;
	
}

unsigned long DebugPingBufLen = 0;
// ******** Ping_Fifo_Put ************
// Puts one value to Ping data FIFO
// Inputs: buffer data entry
// Outputs: 0 if buffer is full, 1 if put is successful
int Ping_Fifo_Put(unsigned long data){
	struct buf_st *p = &pingbuf;

    
	// If the buffer is full, return an error value
  	if (PING_BUFLEN >= PING_BUF_SIZE)
    	return (0);
                                                  
  	p->buf [p->in & (PING_BUF_SIZE - 1)] = data;           // Add data to the transmit buffer.
  	p->in++;


//	OS_Signal(&Ping_Fifo_Available);
	return 1;

}

// ******** Ping_Fifo_Get ************
// Retreives one value from Ping data FIFO
// Inputs:  none
// Outputs: buffer entry
unsigned long Ping_Fifo_Get(void){ 
  	struct buf_st *p = &pingbuf;
//	OS_Wait(&Ping_Fifo_Available);
	DebugPingBufLen = PING_BUFLEN;
	if (PING_BUFLEN == 0)
	{
		return 0;
	}
  	return (p->buf [(p->out++) & (PING_BUF_SIZE - 1)]);
}



#define PING_TIMER_PRESCALE 49

void Ping_Init(unsigned long periodicTimer, unsigned long subTimer)
{
	unsigned long delay;

	//Initialize Ping FIFO
	Ping_Fifo_Init();

	//Enable Port A
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	delay = SYSCTL_RCGC2_R;    // allow time to finish  

	//Configure PA6 to be an output
	GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_6, GPIO_DIR_MODE_OUT);
	GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_6, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

	//Configure PA5 to be an output
	GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_5, GPIO_DIR_MODE_OUT);
	GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_5, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

	//Set prescale of periodic timers
	TimerPrescaleSet(periodicTimer, subTimer, PING_TIMER_PRESCALE);


}


void pingProducer(void)
{
	unsigned long startTime = OS_Time();
	unsigned long endTime = OS_Time();

	TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

	//send a 5 us pulse	on PA6
	GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_6, GPIO_DIR_MODE_OUT);

	IntMasterDisable();
	GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_6, PIN_6_WRITE);
	while ( OS_TimeDifference(endTime, startTime) < FIVE_USEC)
	{
		endTime = OS_Time();
	}
	GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_6, 0);
	IntMasterEnable();

	
	//make PA6 an input and set it to input capture
//	GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_6, GPIO_DIR_MODE_IN);
	GPIOA_AFSEL_R |= PIN_6_WRITE;


	//make Timer 0B an input capture timer and interrupt on edges
	TimerConfigure(TIMER0_BASE, TIMER_CFG_16_BIT_PAIR | TIMER_CFG_B_CAP_TIME);
	TimerControlEvent(TIMER0_BASE, TIMER_B, TIMER_EVENT_BOTH_EDGES);
	TimerLoadSet(TIMER0_BASE, TIMER_B, 0xFFFF);
	TimerIntEnable(TIMER0_BASE, TIMER_CAPB_EVENT);
	TimerPrescaleSet(TIMER0_BASE, TIMER_B, CCP1_TIMER_PRESCALE);


	//Enable capture interrupts and timer 0B
	IntEnable(INT_TIMER0B);
	TimerEnable(TIMER0_BASE, TIMER_B);

	NumSamples++;

}

static unsigned char fallingEdge = 0;
unsigned short risingEdgeTime = 0;
unsigned short fallingEdgeTime = 0;

void pingInterruptHandler(void)
{
	unsigned long pulseWidth = 0;
	unsigned char dataPutFlag = 0;

	//Acknowledge interrupt
	TimerIntClear(TIMER0_BASE, TIMER_CAPB_EVENT);


	


	//Get rising edge time if rising edge caused interrupt
	if (!fallingEdge)
	{
		risingEdgeTime = TimerValueGet(TIMER0_BASE, TIMER_B);
		GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_5, PIN_5_WRITE);

		//Set flag to look for falling edge
		fallingEdge = 1;
	}
	else //else get falling edge time, reset falling edge flag, and return pulse width
	{
		fallingEdgeTime = TimerValueGet(TIMER0_BASE, TIMER_B);
		GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_5, 0);
		fallingEdge = 0;	
	

		//Give the time difference (aka pulse width) to the consumer through a FIFO
		//The following code is derived from the OS_TimeDifference code created
		//by Dustin Replogle and Katy Loeffler
		if(risingEdgeTime >= fallingEdgeTime)
	    {
	  		pulseWidth =  (long)(risingEdgeTime-fallingEdgeTime);
	    }
	    else
	    {
	    	pulseWidth =  (long)(risingEdgeTime + (MAX_16_BIT_TCNT-fallingEdgeTime));      
	    } 
		dataPutFlag	= Ping_Fifo_Put(pulseWidth);
		if(dataPutFlag == 0)
		{
			Ping_Data_Lost++;
			DataLost = Ping_Data_Lost;
		}
	}
}

unsigned long distance = 0;
//int CANTransmitFIFO(unsigned char *pucData, unsigned long ulSize);
unsigned char distanceBuffer[10];
unsigned long DebugPulseWidth = 0;
void pingConsumer(void)
{
	unsigned long pulseWidth;
	unsigned long pulseWidthUSec;
	unsigned long tempDistance;

	while(1)
	{
		pulseWidth = Ping_Fifo_Get();
		if (pulseWidth > 0 && pulseWidth < 65535)
		{
			DebugPulseWidth = pulseWidth;
			pulseWidthUSec = pulseWidth / NUMBER_OF_INCS_IN_USEC;
			tempDistance = pulseWidthUSec * SPEED_OF_SOUND;
			tempDistance = tempDistance / NUMBER_OF_NM_IN_MM;
			tempDistance = tempDistance * 10;
			tempDistance = tempDistance / 2;
			distance = tempDistance;
		
			//Transmit by CAN
		//	sprintf( (char*) &distanceBuffer, "%ul",distance);
		//	CANTransmitFIFO( (unsigned char*) &distanceBuffer, 3);
		}
		if (pulseWidth >= 65536)//Error, thinks rising edge is falling edge and vice versa
		{
			distance = MAX_DISTANCE;

			IntMasterDisable();			

			//Disable pending ping interrupts
		    IntDisable(INT_TIMER0B);
			TimerIntDisable(TIMER0_BASE, TIMER_CAPB_EVENT);

			//Reset the fifo
			Ping_Fifo_Init();

			//Get the interrupts to look for a falling edge again
			fallingEdge = 0;

			//Clear pending ping interrupts 
			TimerIntClear(TIMER0_BASE, TIMER_CAPB_EVENT);



			//Clear port A6
			GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_6, GPIO_DIR_MODE_OUT);
			GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_6, 0);

			IntMasterEnable();
		}
	}
}




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
#define GPIO_AFSEL_R		(*((volatile unsigned long *)0x40007420)) 
#define FIVE_USEC 250
#define PIN_4_WRITE 0x10 
#define MAX_16_BIT_TCNT 
#define NUMBER_OF_CYCLES_IN_USEC 50
#define SPEED_OF_SOUND 34029 //speed of sound in 10 nm/us units
#define NUMBER_OF_NM_IN_MM 1000000

unsigned long Ping_Data_Lost = 0;
Sema4Type Ping_Fifo_Available;

//*****************************************************************************
//
// Ping FIFO
//
//*****************************************************************************
#define PING_BUF_SIZE   16	     /*** Must be a power of 2 (2,4,8,16,32,64,128,256,512,...) ***/

#if PING_BUF_SIZE < 2
#error PING_BUF_SIZE is too small.  It must be larger than 1.
#elif ((PING_BUF_SIZE & (PING_BUF_SIZE-1)) != 0)
#error PING_BUF_SIZE must be a power of 2.
#endif


struct buf_st {
  unsigned int in;                                // Next In Index
  unsigned int out;                               // Next Out Index
  char buf [PING_BUF_SIZE];                           // Buffer
};

static struct buf_st pingbuf = { 0, 0, };
#define PING_BUFLEN ((unsigned short)(pingbuf.in - pingbuf.out))



// ******** Ping_Fifo_Init ************
// Initializes Ping data FIFO
// Inputs: size of the FIFO
// Outputs: none
void Ping_Fifo_Init(void){
	OS_InitSemaphore(&Ping_Fifo_Available, 0);
	pingbuf.in = 0;
  	pingbuf.out = 0;
	
}

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

	OS_Signal(&Ping_Fifo_Available);
	return 1;

}

// ******** Ping_Fifo_Get ************
// Retreives one value from Ping data FIFO
// Inputs:  none
// Outputs: buffer entry
unsigned long Ping_Fifo_Get(void){ 
  	struct buf_st *p = &pingbuf;
	OS_Wait(&Ping_Fifo_Available);
  	return (p->buf [(p->out++) & (PING_BUF_SIZE - 1)]);
}





void Ping_Init(void)
{
	unsigned long delay;

	//Initialize Ping FIFO
	Ping_Fifo_Init();

	//Enable Port D
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);


	//Configure PD4 to be an output
	delay = SYSCTL_RCGC2_R;    // allow time to finish  
	GPIODirModeSet(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_DIR_MODE_OUT);
	GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);


}


void pingProducer(void)
{
	unsigned long startTime = OS_Time();
	unsigned long endTime = OS_Time();


	//send a 5 us pulse	on PD4
	IntMasterDisable();
	GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, PIN_4_WRITE);
	while ( OS_TimeDifference(endTime, startTime) < FIVE_USEC)
	{
		endTime = OS_Time();
	}
	GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0);
	IntMasterEnable();

	
	//make PD4 an input and set it to input capture
	GPIODirModeSet(GPIO_PORTD_BASE, GPIO_PIN_4, GPIO_DIR_MODE_IN);
	GPIO_AFSEL_R = PIN_4_WRITE;


	//make Timer 0B an input capture timer and interrupt on edges
	TimerConfigure(TIMER0_BASE, TIMER_CFG_B_CAP_TIME);
	TimerControlEvent(TIMER0_BASE, TIMER_B, TIMER_EVENT_BOTH_EDGES);
	TimerIntEnable(TIMER0_BASE, TIMER_CAPB_EVENT);


	//Enable capture interrupts and timer 0B
	IntEnable(INT_TIMER0B);
	TimerEnable(TIMER0_BASE, TIMER_B);

}

static unsigned char fallingEdge = 0;
static unsigned long risingEdgeTime = 0;
static unsigned long fallingEdgeTime = 0;

void pingInterruptHandler(void)
{
	unsigned long pulseWidth = 0;

	//Acknowledge interrupt
	TimerIntClear(TIMER0_BASE, TIMER_CAPB_EVENT);


	//Set flag to look for falling edge
	fallingEdge = 1;


	//Get rising edge time if rising edge caused interrupt
	if (!fallingEdge)
	{
		risingEdgeTime = TimerValueGet(TIMER0_BASE, TIMER_B);
	}
	else //else get falling edge time, reset falling edge flag, and return pulse width
	{
		fallingEdgeTime = TimerValueGet(TIMER0_BASE, TIMER_B);
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

		if(!Ping_Fifo_Put(pulseWidth));
		{
			Ping_Data_Lost++;
		}
	}
}

unsigned long distance = 0;
int CANTransmitFIFO(unsigned char *pucData, unsigned long ulSize);
unsigned char distanceBuffer[10];

void pingConsumer(void)
{
	unsigned long pulseWidth = Ping_Fifo_Get();
	unsigned long pulseWidthUSec = pulseWidth / NUMBER_OF_CYCLES_IN_USEC;
	unsigned long tempDistance = pulseWidthUSec * SPEED_OF_SOUND;
	tempDistance = tempDistance / NUMBER_OF_NM_IN_MM;
	tempDistance = tempDistance * 10;
	distance = tempDistance;

	//Transmit by CAN
	sprintf( (char*) &distanceBuffer, "%ul",distance);
	CANTransmitFIFO( (unsigned char*) &distanceBuffer, 3);
}




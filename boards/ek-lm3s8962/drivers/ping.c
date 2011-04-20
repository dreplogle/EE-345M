#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_sysctl.h"
#include "hw_timer.h"
#include "timer.h"
#include "gpio.h"
#include "sysctl.h"
#include "interrupt.h"
#include "hw_ints.h"
#include "string.h"
#include "driverlib/can.h"
#include <string.h>
#include "math.h"




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
#define MAX_DISTANCE 3000
#define PING_PERIOD 50000 //This is in units of 2000 ns increments
#define CAN_FIFO_SIZE           (8 * 8)

unsigned long Ping_Data_Lost = 0;
//Sema4Type Ping_Fifo_Available;
unsigned long NumSamples = 0;
unsigned long DataLost = 0;



// ******** CAN_Send ************
// Sends information on the CAN
// Inputs: "data" is a pointer to the data to be sent
// Outputs: none
void CAN_Send(unsigned char *data);



unsigned long OS_Time(void)
{
  return TimerValueGet(TIMER1_BASE, TIMER_B);
}







//***********************************************************************
//
// OS_TimeDifference returns the time difference in usec.
// inputs: "time1" is the second time, "time2" is the first time
// outputs: returns the difference between "time1" and "time2"
//
//***********************************************************************
long OS_TimeDifference(unsigned long time1, unsigned long time2)
//                                   thisTime          	  LastTime
{
  if(time2 >= time1)
  {
  	return (long)(time2-time1);
  }
  else
  {
    return (long)(time2 + (MAX_16_BIT_TCNT-time1));      
  } 
}



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

static unsigned char fallingEdge = 0;
unsigned short risingEdgeTime = 0;
unsigned short fallingEdgeTime = 0;


struct buf_st {
  unsigned int in;                                // Next In Index
  unsigned int out;                               // Next Out Index
  unsigned long buf [PING_BUF_SIZE];                           // Buffer
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





unsigned long distance = 0;
unsigned char distanceBuffer[CAN_FIFO_SIZE];
unsigned long DebugPulseWidth = 0;
#define PING_SAMPLING_RATE 10
struct PING_STATS{
  short average;
  short stdev;
  short maxdev;
};
struct PING_STATS PING_Stats;
long ping_stats[PING_SAMPLING_RATE];
int ping_stats_index = 0;



// ******** pingConsumer ************
// Gets pulse width data from the Ping fifo
// and converts it into distance
// Inputs: none
// Outputs: none
void pingConsumer(void)  //make this an interrupt
{
	unsigned long pulseWidth;
	unsigned long pulseWidthUSec;
	unsigned long tempDistance;
	long sum;
 	unsigned short max,min;
	int i;

		pulseWidth = Ping_Fifo_Get();
		if (pulseWidth > 0 && pulseWidth < 19000000)
		{
			DebugPulseWidth = pulseWidth;
			pulseWidthUSec = pulseWidth / NUMBER_OF_INCS_IN_USEC;
			tempDistance = pulseWidthUSec * SPEED_OF_SOUND;
			tempDistance = tempDistance / NUMBER_OF_NM_IN_MM;
			tempDistance = tempDistance * 10;
			tempDistance = tempDistance / 2;
			distance = tempDistance;
		


			//Transmit by CAN
			distanceBuffer[0] = 'p';
			memcpy(&distanceBuffer[1], &distance, 4);
//			CAN_Send(distanceBuffer);



			//Take measurements
			if (ping_stats_index < PING_SAMPLING_RATE)
			{
				ping_stats[ping_stats_index] = distance;
				ping_stats_index++;
			}
			else
			{
				ping_stats_index = 0;

				//Average = sum of all values/number of values
				//Maximum deviation = max value - min value
				max = ping_stats[0];
				min = ping_stats[0];
				sum = 0;
				for(i = 0; i < PING_SAMPLING_RATE; i++){
			      sum += ping_stats[i];
				  if(ping_stats[i] <= min) min = ping_stats[i];
				  if(ping_stats[i] > max) max = ping_stats[i];	  
				}
			    PING_Stats.average = (sum/PING_SAMPLING_RATE);
				PING_Stats.maxdev = (max - min);
			
				//Standard deviation = sqrt(sum of (each value - average)^2 / number of values)
				sum = 0;
				for(i = 0; i < PING_SAMPLING_RATE; i++){
			      sum +=(ping_stats[i]-PING_Stats.average)*(ping_stats[i]-PING_Stats.average);	  
				}
				sum = sum/PING_SAMPLING_RATE;
				PING_Stats.stdev = sqrt(sum);
			}

		}
		if (pulseWidth >= 19000000)//Error, thinks rising edge is falling edge and vice versa
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






unsigned long NumOfWrapArounds = 0;

// ******** pingProducer ************
// Starts a Ping distance measurement
// by the Ping sensor
// Inputs: none
// Outputs: none
void pingProducer(void)
{
	unsigned long startTime = OS_Time();
	unsigned long endTime = OS_Time();

	pingConsumer();

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


/*********************************************************/
/*                    Dan's Edit						 */
/*********************************************************/
 
	TimerDisable(TIMER0_BASE, TIMER_B);	
	HWREG(TIMER0_BASE + TIMER_O_CFG) = 0x04;
	HWREG(TIMER0_BASE + TIMER_O_TBMR) = 0x07;
	HWREG(TIMER0_BASE + TIMER_O_CTL) |= 0x0C00; //0x0C?
	HWREG(TIMER0_BASE + TIMER_O_TBILR) |= 0xFFFF;

	TimerIntEnable(TIMER0_BASE, TIMER_CAPB_EVENT);
	//TimerPrescaleSet(TIMER0_BASE, TIMER_B, CCP1_TIMER_PRESCALE);
	TimerEnable(TIMER0_BASE, TIMER_B);
	IntEnable(INT_TIMER0B);

/*********************************************************/
/*                   Aravind's Original				 	 */
/*********************************************************/

//	//make Timer 0B an input capture timer and interrupt on edges
//	TimerConfigure(TIMER0_BASE, TIMER_CFG_16_BIT_PAIR | TIMER_CFG_B_CAP_TIME);
//
//
//// configure for capture mode
//	TimerControlEvent(TIMER0_BASE, TIMER_B, TIMER_EVENT_BOTH_EDGES);
//	TimerLoadSet(TIMER0_BASE, TIMER_B, 0xFFFF);
//	TimerIntEnable(TIMER0_BASE, TIMER_CAPB_EVENT);
//	TimerPrescaleSet(TIMER0_BASE, TIMER_B, CCP1_TIMER_PRESCALE);
//
//	//Enable capture interrupts and timer 0B
//	IntEnable(INT_TIMER0B);
//	TimerEnable(TIMER0_BASE, TIMER_B);

	fallingEdge = 0;


	NumSamples++;

}




void PingTimer1BHandler(void)
{
	 TimerIntClear(TIMER1_BASE,  TIMER_TIMB_TIMEOUT);
	 NumOfWrapArounds++;
}






// ******** pingInterruptHandler ************
// Called by input capture interrupts created by
// the Ping sensor. Puts pulse width of the Ping
// sensor pulse in the Ping fifo.
// Inputs: none
// Outputs: none
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


		//make Timer 1B cause a timeout interrupt
		TimerDisable(TIMER1_BASE, TIMER_B);
		TimerIntEnable(TIMER1_BASE, TIMER_TIMB_TIMEOUT);
		TimerLoadSet(TIMER1_BASE, TIMER_B, MAX_16_BIT_TCNT);


		//Enable timer 1B and its interrupts
		IntEnable(INT_TIMER1B);
		TimerEnable(TIMER1_BASE, TIMER_B);

		//Clear wrap-arounds
		NumOfWrapArounds = 0;
	}
	else //else get falling edge time, reset falling edge flag, and return pulse width
	{
		fallingEdgeTime = TimerValueGet(TIMER0_BASE, TIMER_B);
		GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_5, 0);
	

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
		

		pulseWidth = pulseWidth + (NumOfWrapArounds * MAX_16_BIT_TCNT);
		
		 
		dataPutFlag	= Ping_Fifo_Put(pulseWidth);
		if(dataPutFlag == 0)
		{
			Ping_Data_Lost++;
			DataLost = Ping_Data_Lost;
		}

		//Disable timer 0B
		TimerDisable(TIMER0_BASE, TIMER_B);

	}


	//Set flag to look for opposite edge
	fallingEdge ^= 0x1;
}


#define PING_TIMER_PRESCALE 49



// ******** Ping_Init ************
// Initializes all timers and ports that
// are related to the Ping sensor.
// Inputs: "periodicTimer" is the timer used for the periodic interrupt
// that calls pingProducer. "subTimer" is the sub-timer used for the periodic
// interrupt that calls pingProducer.
// Outputs: none
void Ping_Init(unsigned long periodicTimer, unsigned long subTimer)
{
	unsigned long delay;
	unsigned char priority;

	 //Initialize timer 0 so that input capture can be used
 	 SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

	 //Initialize timer 1 so that OS_Time can be used
	 // Enable Timer1 module
 	 SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);

 	 // Configure Timer1 as a 16-bit periodic timer.
 	 TimerConfigure(TIMER1_BASE, TIMER_CFG_B_PERIODIC);
 	 TimerLoadSet(TIMER1_BASE, TIMER_B, MAX_16_BIT_TCNT);

	 TimerEnable(TIMER1_BASE, TIMER_B);

	SysCtlClockSet(SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
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
	TimerConfigure(TIMER2_BASE, TIMER_CFG_16_BIT_PAIR | TIMER_CFG_A_PERIODIC | TIMER_CFG_B_PERIODIC);
	TimerLoadSet(TIMER2_BASE, TIMER_A, PING_PERIOD); 
	
	//Set timer to interrupt and set priority
	IntEnable(INT_TIMER2A);
	priority = 0;
	priority = priority << 5;
	IntPrioritySet(INT_TIMER2A, priority);
	TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT); 

	//Enable the timer    
	TimerEnable(TIMER2_BASE, TIMER_A);	

		


}


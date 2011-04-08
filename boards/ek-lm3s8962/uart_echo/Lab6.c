//*****************************************************************************
//
// Lab5.c - user programs, File system, stream data onto disk
// Jonathan Valvano, March 16, 2011, EE345M
//     You may implement Lab 5 without the oLED display
//*****************************************************************************
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
#include <stdio.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/systick.h"
#include "driverlib/fifo.h"
#include "driverlib/adc.h"
#include "drivers/OS.h"
#include "drivers/OSuart.h"
#include "drivers/rit128x96x4.h"
#include "lm3s8962.h"
#include "math.h"
#include "drivers/can_fifo.h"
#include "drivers/tachometer.h"
#include "drivers/Analog_Comp.h"

unsigned long NumCreated;   // number of foreground threads created
unsigned long NumSamples;   // incremented every sample
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished

extern unsigned long JitterHistogramA[];
extern unsigned long JitterHistogramB[];
extern long MaxJitterA;
extern long MinJitterA;

unsigned short SoundVFreq = 1;
unsigned short SoundVTime = 0;
unsigned short FilterOn = 1;

struct sensors{
	unsigned long ping;
	unsigned long tach;
	long IR;
}Sensors;

int Running;                // true while robot is running

#define GPIO_PF0  (*((volatile unsigned long *)0x40025004))
#define GPIO_PF1  (*((volatile unsigned long *)0x40025008))
#define GPIO_PF2  (*((volatile unsigned long *)0x40025010))
#define GPIO_PF3  (*((volatile unsigned long *)0x40025020))
#define GPIO_PG1  (*((volatile unsigned long *)0x40026008))
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
// PF0/PWM0 is debugging output on Systick
// PF2/LED1 is debugging output 
// PF3/LED0 is debugging output 
// PG1/PWM1 is debugging output


//******** Producer *************** 
// The Producer in this lab will be called from your ADC ISR
// A timer runs at 1 kHz, started by your ADC_Collect
// The timer triggers the ADC, creating the 1 kHz sampling
// Your ADC ISR runs when ADC data is ready
// Your ADC ISR calls this function with a 10-bit sample 
// sends data to the Robot, runs periodically at 1 kHz
// inputs:  none
// outputs: none
void Producer(unsigned short data){  
  if(Running){
    if(OS_Fifo_Put(data)){     // send to Robot
      NumSamples++;
    } else{ 
      DataLost++;
    } 
  }
}
 
//******** IdleTask  *************** 
// foreground thread, runs when no other work needed
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
unsigned long Idlecount=0;
void IdleTask(void){
  while(1) { 
    Idlecount++;        // debugging 
  }
}

//******** Interpreter **************
// your intepreter from Lab 4 
// foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
extern void Interpreter(void); 

//************ButtonPush*************
// Called when Select Button pushed
// background threads execute once and return
void ButtonPush(void){
  int i;
  unsigned char data[CAN_FIFO_SIZE];
  for(i=0; i<CAN_FIFO_SIZE; i++)
  {
    data[i] = i;
  }
  
  CAN_Send(data);
}
//************DownPush*************
// Called when Down Button pushed
// background threads execute once and return
void DownPush(void){
  CAN_Receive();
}

//*************GetIR***************
// Background thread for IR sensor,
// called when ADC finishes a conversion
// and generates an interrupt.
void GetIR(unsigned short data){  
  if(RawIR_Fifo_Put(data)){     
    NumSamples++;
  } else{ 
    DataLost++;
  } 
}

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 
#define IR_SAMPLING_RATE	2000              // in Hz
AddFifo(RawIR_, 32, unsigned short, 1, 0);   // Raw IR data, ADC samples
struct IR_STATS{
  short average;
  short stdev;
  short maxdev;
};
struct IR_STATS IR_Stats;
long stats[IR_SAMPLING_RATE];

void IRSensor(void){
  unsigned short i, ADCin;
  long sampleOut, data[3];
  static unsigned int newest = 0;
  long sum;
  unsigned short max,min;
  

  ADC_Collect(0, IR_SAMPLING_RATE, &GetIR); //ADC sample on channel 0, 20Hz
  
  for(;;){
	data[2] = data[1];
	data[1] = data[0];
    while(!RawIR_Fifo_Get(&ADCin));
	data[0] = ((long)ADCin*7836 - 166052)/1024; //((1/cm)*65535) = ((7836*x-166052)/1024
	data[0] = 65535/data[0];  //cm = 65535/data[0] from last operation
  
    //3-Element median filter
    if((data[0]<=data[1]&&data[0]>=data[2])||(data[0]<=data[2]&&data[0]>=data[1])) sampleOut = data[0];
    else if((data[1]<=data[0]&&data[1]>=data[2])||(data[1]<=data[2]&&data[1]>=data[0])) sampleOut = data[1];
    else if((data[2]<=data[1]&&data[2]>=data[0])||(data[2]<=data[0]&&data[2]>=data[1])) sampleOut = data[2];  
    else sampleOut = 0xFF;       // Median finding error  
	if(sampleOut > 80)  sampleOut = 80;  //Cap at maximum end of the range 
	if(sampleOut < 10)  sampleOut = 10;  //Cap at minimum end of range  

	stats[newest] = sampleOut;
	newest++;
	if(newest >= IR_SAMPLING_RATE) newest = 0;

	//Average = sum of all values/number of values
	//Maximum deviation = max value - min value
	max = 0;
	min = 0xFFFF;
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum += stats[i];
	  if(stats[i] < min) min = stats[i];
	  if(stats[i] > max) max = stats[i];	  
	}
    IR_Stats.average = (sum/IR_SAMPLING_RATE);
	IR_Stats.maxdev = (max - min);

	//Standard deviation = sqrt(sum of (each value - average)^2 / number of values)
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum +=(stats[i]-IR_Stats.average)*(stats[i]-IR_Stats.average);	  
	}
	sum = sum/IR_SAMPLING_RATE;
	IR_Stats.stdev = sqrt(sum);
	Sensors.IR = IR_Stats.average;

  }
}

void Display(void){
	while(1){
  	oLED_Message(0, 3, "IR: ", Sensors.IR);
	//SysCtlDelay(SysCtlClockGet()/100);
	oLED_Message(0, 2, "Tach: ", Sensors.tach);
	//SysCtlDelay(SysCtlClockGet()/100);
	oLED_Message(0, 1, "Ping: ", Sensors.ping);
	//SysCtlDelay(SysCtlClockGet()/100);
	
	oLED_Message(1, 0, "IR Avg", IR_Stats.average);
	oLED_Message(1, 1, "IR StdDev", IR_Stats.stdev);
	oLED_Message(1, 2, "IR MaxDev", IR_Stats.maxdev);
	}
}

//*******************lab 6 main **********
int main(void){       
  OS_Init();           // initialize, disable interrupts
  Running = 0;         // robot not running
  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;

//********initialize communication channels
  OS_Fifo_Init(512);    // ***note*** 4 is not big enough*****

  //Tachometer_Init(2);

//*******attach background tasks***********
  OS_AddButtonTask(&ButtonPush,2);
  OS_AddDownTask(&DownPush,3);

  OS_BumperInit();
//  CAN_Init();
  Comparator_Capture_Init();

  NumCreated = 0 ;
// create initial foreground threads
//  NumCreated += OS_AddThread(&CAN,128,2); 
  NumCreated += OS_AddThread(&IRSensor,128,2);  // runs when nothing useful to do
  NumCreated += OS_AddThread(&Interpreter,128,2);

//  NumCreated += OS_AddThread(&IdleTask,128,2);  // runs when nothing useful to do
//  NumCreated += OS_AddThread(&Display,128,2);
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
 
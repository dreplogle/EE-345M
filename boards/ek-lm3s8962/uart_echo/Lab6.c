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
#include "drivers/edisk.h"
#include "drivers/efile.h"

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
  if(Running==0){
    Running = 1;  // prevents you from starting two robot threads
    NumCreated += OS_AddThread(&IdleTask,128,1);  // start a 20 second run
  }
}
//************DownPush*************
// Called when Down Button pushed
// background threads execute once and return
void DownPush(void){

}

//*************GetIR***************
// Background thread for IR sensor,
// called when ADC finishes a conversion
// and generates an interrupt.
void GetIR(unsigned short data){  
  if(Running){
    if(IR_Fifo_Put(data)){     
      NumSamples++;
    } else{ 
      DataLost++;
    } 
  }
}

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 
AddFifo(IR_, 32, unsigned short, 1, 0);   // UARTTx Buffer

void IRSensor(void){
  unsigned short data[3], i, sampleOut;

  ADC_Collect(0, 20, &GetIR); //ADC sample on channel 0, 20Hz
  
  for(;;){
    for(i = 0; i<3; i++){	      // Get 3 samples for median filter 
      while(!IR_Fifo_Get(&data[i]));
    }
   
    //3-Element median filter
    if((data[0]<=data[1]&&data[0]>=data[2])||(data[0]<=data[2]&&data[0]>=data[1])) sampleOut = data[0];
    else if((data[1]<=data[0]&&data[1]>=data[2])||(data[1]<=data[2]&&data[1]>=data[0])) sampleOut = data[1];
    else if((data[2]<=data[1]&&data[2]>=data[0])||(data[2]<=data[0]&&data[2]>=data[1])) sampleOut = data[2];  
    else sampleOut = 0xFF;       // Median finding error

    //CAN_Out(sampleOut)
  }
}

//*******************lab 5 main **********
int main(void){        // lab 5 real main
  OS_Init();           // initialize, disable interrupts
  Running = 0;         // robot not running
  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;

//********initialize communication channels
  OS_Fifo_Init(512);    // ***note*** 4 is not big enough*****
  ADC_Open();
  ADC_Collect(0, 1000, &Producer); // start ADC sampling, channel 0, 1000 Hz 

//*******attach background tasks***********
  OS_AddButtonTask(&ButtonPush,2);
  OS_AddDownTask(&DownPush,3);
  //OS_AddPeriodicThread(disk_timerproc,TIME_1MS,5);

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,2); 
  NumCreated += OS_AddThread(&IdleTask,128,7);  // runs when nothing useful to do
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
} 
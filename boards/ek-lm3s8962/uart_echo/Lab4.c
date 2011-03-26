//*****************************************************************************
//
// Lab3.c - user programs
// Jonathan Valvano, Feb 17, 2011, EE345M
//
//*****************************************************************************
// feel free to adjust these includes as needed
// You are free to change the syntax/organization of this file
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
#include <stdio.h>
#include <string.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/adc.h"
#include "driverlib/fifo.h"
#include "drivers/rit128x96x4.h"
#include "string.h"
#include "drivers/OS.h"
#include "drivers/OSuart.h"
#include "lm3s8962.h"
#include "drivers/OSuart.h"

unsigned long NumCreated;   // number of foreground threads created
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished
unsigned long NumSamples;   // incremented every sample
unsigned long DataLost;     // data sent by Producer, but not received by Consumer

extern unsigned long JitterHistogramA[];
extern unsigned long JitterHistogramB[];
extern long MaxJitterA;
extern long MinJitterA;

unsigned short SoundVFreq = 1;
unsigned short SoundVTime = 0;
unsigned short FilterOn = 1;
long SoundDump[200];
unsigned int SoundDumpInd;
unsigned int Trigger = 0;

Sema4Type MailBoxFull;
Sema4Type MailBoxEmpty;
Sema4Type SoundReady;
Sema4Type SoundRead;

#define GPIO_B0 (*((volatile unsigned long *)(0x40005004)))
#define GPIO_B1 (*((volatile unsigned long *)(0x40005008)))
#define GPIO_B2 (*((volatile unsigned long *)(0x40005010)))
#define GPIO_B3 (*((volatile unsigned long *)(0x40005020)))

// 10-sec finite time experiment duration 
#define RUNLENGTH 10000   // display results and quit when NumSamples==RUNLENGTH
long x[256],y[256];         // input and output arrays for FFT
long xFilt[256];
short data[256];
void cr4_fft_256_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);
long hanning[256] = {0,0,1,1,2,4,6,8,10,12,15,19,22,26,30,35,39,44,50,55,
                    61,67,73,80,87,94,101,109,117,125,134,142,151,160,169,
                    179,189,198,208,219,229,240,251,262,273,284,295,307,318,
                    330,342,354,366,378,390,402,415,427,440,452,465,477,490,
                    503,515,528,540,553,566,578,591,603,615,628,640,652,664,
                    676,688,700,712,723,735,746,757,768,779,790,800,810,821,
					831,840,850,859,868,877,886,895,903,911,919,926,933,941,
					947,954,960,966,972,977,982,987,992,996,1000,1004,1007,1010,
					1013,1015,1017,1019,1021,1022,1023,1024,1024,1024,1024,1023,
					1022,1021,1019,1017,1015,1013,1010,1007,1004,1000,996,992,
					987,982,977,972,966,960,954,947,941,933,926,919,911,903,895,
					886,877,868,859,850,840,831,821,810,800,790,779,768,757,746,
					735,723,712,700,688,676,664,652,640,628,615,603,591,578,566,
					553,540,528,515,503,490,477,465,452,440,427,415,402,390,378,
					366,354,342,330,318,307,295,284,273,262,251,240,229,219,208,
					198,189,179,169,160,151,142,134,125,117,109,101,94,87,80,73,
					67,61,55,50,44,39,35,30,26,22,19,15,12,10,8,6,4,2,1,1,0,0};


//------------------Task 1--------------------------------
// 2 kHz sampling ADC channel 1, using software start trigger
// background thread executed at 2 kHz
// 60-Hz notch IIR filter, assuming fs=2000 Hz
// y(n) = (256x(n) -503x(n-1) + 256x(n-2) + 498y(n-1)-251y(n-2))/256
short Filter(short data){
static short x[6]; // this MACQ needs twice
static short y[6];
static unsigned int n=3;   // 3, 4, or 5
  n++;
  if(n==6) n=3;     
  x[n] = x[n-3] = data;  // two copies of new data
  y[n] = (256*(x[n]+x[n-2])-503*x[1]+498*y[1]-251*y[n-2]+128)/256;
  y[n-3] = y[n];         // two copies of filter outputs too
  return y[n];
}

//-----------Audio Bandpass FIR Filter-------------------
short Filter51(short data){
static short x[51];
static unsigned int newest = 0;
static short h[51]={-7,0,-10,-7,-1,-20,-3,-13,-25,0,-36,
     -16,-15,-52,1,-53,-37,-4,-93,15,-66,-80,74,-244,214,
     954,214,-244,74,-80,-66,15,-93,-4,-37,-53,1,-52,-15,
     -16,-36,0,-25,-13,-3,-20,-1,-7,-10,0,-7};




long y = 0;   				//result

unsigned int n, xn;

  x[newest] = data;
  xn = newest; 				//start with x[n]
  newest++;
  if(newest == 51)
  {
    newest = 0;  
  }
  for(n = 0; n < 51; n++)
  {
    y = y + ((long)h[n]*(long)x[xn])/256;
	n++;
	xn++;
	if(xn == 51)
	{
	  xn = 0;
    }
  }
return (short)y;
}

//******** DAS *************** 
// background thread, calculates 60Hz notch filter
// runs 2000 times/sec
// inputs:  none
// outputs: none
unsigned short DASoutput;
void DAS(void){ 
unsigned short input;  
  if(NumSamples < RUNLENGTH){   // finite time run
    input = ADC_In(1);
    DASoutput = Filter(input);
    FilterWork++;        // calculation finished

  }
}


//  RIT128x96x4PlotPoint(long y); voltage vs. time points
//  RIT128x96x4PlotBar(long y);  voltage vs. time bars
//  void RIT128x96x4PlotdBfs(long y) voltage vs. frequency
//--------------end of Task 1-----------------------------

//------------------Task 2--------------------------------
// background thread executes with select button
// one foreground task created with button push
// foreground treads run for 2 sec and die
// ***********ButtonWork*************
void ButtonWork(void){
unsigned long myId = OS_Id(); 
  oLED_Message(1,0,"NumCreated =",NumCreated); 
  oLED_Message(1,1,"PIDWork    =",PIDWork);
  oLED_Message(1,2,"DataLost   =",DataLost);
  oLED_Message(1,3,"0.1u Jitter=",MaxJitterA-MinJitterA);
  OS_Kill();  // done
} 

//************ButtonPush*************
// Called when Select Button pushed
// Adds another foreground task
// background threads execute once and return
void ButtonPush(void){
  if(OS_AddThread(&ButtonWork,100,1)){
    NumCreated++; 
  }
}
//************DownPush*************
// Called when Down Button pushed
// Adds another foreground task
// background threads execute once and return
void DownPush(void){
  RIT128x96x4ShowPlot();
}
//--------------end of Task 2-----------------------------

//------------------Task 3--------------------------------
// hardware timer-triggered ADC sampling at 1 kHz
// Producer runs as part of ADC ISR
// Producer uses fifo to transmit 1000 samples/sec to Consumer
// every 64 samples, Consumer calculates FFT
// every 64 ms, consumer sends data to Display via mailbox
// Display thread updates oLED with measurement

//******** Producer *************** 
// The Producer in this lab will be called from your ADC ISR
// A timer runs at 10000 kHz, started by your ADC_Collect
// The timer triggers the ADC, creating the 10000 kHz sampling
// Your ADC ISR runs when ADC data is ready
// Your ADC ISR calls this function with a 10-bit sample 
// sends data to the consumer, runs periodically at 10000 kHz
// inputs:  none
// outputs: none
void Producer(unsigned short data){
  GPIO_B0 ^= 0x01;  
  if(NumSamples < RUNLENGTH){   // finite time run
//    NumSamples++;               // number of samples
    if(OS_Fifo_Put(data) == 0){ // send to consumer
      DataLost++;
    } 
  } 
}
void Display(void); 

//******** Consumer *************** 
// foreground thread, accepts data from producer
// calculates FFT, implements FIR filter
// inputs:  none
// outputs: none
void Consumer(void){ 
unsigned long data,DCcomponent; // 10-bit raw ADC sample, 0 to 1023
unsigned int i;
unsigned long t;  // time in ms
unsigned long myId = OS_Id(); 

  ADC_Collect(0, 10000, &Producer); // start ADC sampling, channel 0, 1000 Hz
//  NumCreated += OS_AddThread(&Display,128,0); 
  while(NumSamples < RUNLENGTH) {
    OS_Wait(&SoundRead); 
    for(t = 0; t < 256; t++){   // collect 256 ADC samples
      while(!OS_Fifo_Get(&data));   // get from producer    
      x[t] = data;           // real part is 0 to 1023, imaginary part is 0
	  if(FilterOn)
	  {
        xFilt[t] = (long)Filter51((short)x[t]);
	  }
	  else
	  {
	    xFilt[t] = (long)x[t];
	  }
    }
	for(i = 0; i < 256; i++)
	{
      xFilt[i] = ((xFilt[i]-423)*hanning[i])/1024;	// 423 is DC correction for 1.24 V out of 3 V.
	}

    cr4_fft_256_stm32(y,xFilt,256);  // complex FFT of last 256 ADC values
	OS_Signal(&SoundReady);
    DCcomponent = y[0]&0xFFFF; // Real part at frequency 0, imaginary part should be zero
    
//	OS_Wait(&MailBoxEmpty);
	OS_MailBox_Send(DCcomponent);
	GPIO_B1 ^= 0x02;
//	OS_Signal(&MailBoxFull);
	
//	OS_Sleep(15);
  }
  OS_Kill();  // done
}
//******** Display *************** 
// foreground thread, accepts data from consumer
// displays calculated results on the LCD
// inputs:  none                            
// outputs: none
void Display(void){ 
unsigned long data,voltage;
  oLED_Message(0,0,"Run length is",(RUNLENGTH)/1000);   // top half used for Display
  while(NumSamples < RUNLENGTH) { 
    oLED_Message(0,1,"Time left is",(RUNLENGTH-NumSamples)/1000);   // top half used for Display
    
	OS_Wait(&MailBoxFull);
	data = OS_MailBox_Recv();
	OS_Signal(&MailBoxEmpty);
    
	voltage = 3000*data/1024;               // calibrate your device so voltage is in mV
    oLED_Message(0,2,"v(mV) =",voltage);  
  } 
  OS_Kill();  // done
} 

//--------------end of Task 3-----------------------------

//------------------Task 4--------------------------------
// foreground thread that runs without waiting or sleeping
// it executes a digital controller 
//******** PID *************** 
// foreground thread, runs a PID controller
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
short IntTerm;     // accumulated error, RPM-sec
short PrevError;   // previous error, RPM
short Coeff[3];    // PID coefficients
short PID_stm32(short Error, short *Coeff);
short Actuator;
void PID(void){ 
short err;  // speed error, range -100 to 100 RPM
unsigned long myId = OS_Id(); 
  PIDWork = 0;
  IntTerm = 0;
  PrevError = 0;
  Coeff[0] = 384;   // 1.5 = 384/256 proportional coefficient
  Coeff[1] = 128;   // 0.5 = 128/256 integral coefficient
  Coeff[2] = 64;    // 0.25 = 64/256 derivative coefficient*
  while(NumSamples < RUNLENGTH) { 
    for(err = -1000; err <= 1000; err++){    // made-up data
      Actuator = PID_stm32(err,Coeff)/256;
    }
	GPIO_B2 ^= 0x04; 
    PIDWork++;        // calculation finished
  }
  OS_Kill();          // done
}
//--------------end of Task 4-----------------------------

//------------------Task 5--------------------------------
// UART background ISR performs serial input/output
// two fifos are used to pass I/O data to foreground
// Lab 1 interpreter runs as a foreground thread
// the UART driver should call OS_Wait(&RxDataAvailable) when foreground tries to receive
// the UART ISR should call OS_Signal(&RxDataAvailable) when it receives data from Rx
// similarly, the transmit channel waits on a semaphore in the foreground
// and the UART ISR signals this semaphore (TxRoomLeft) when getting data from fifo
// it executes a digital controller 
// your intepreter from Lab 1, with additional commands to help debug 
// foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
extern void Interpreter(void);    // just a prototype, link to your interpreter
// add the following commands, leave other commands, if they make sense
// 1) print performance measures 
//    time-jitter, number of data points lost, number of calculations performed
//    i.e., NumSamples, NumCreated, MaxJitter-MinJitter, DataLost, FilterWork, PIDwork


//--------------end of Task 5-----------------------------

void Jitter(void)   // prints jitter information (write this)
{
  char string[7], printInd[7];
  int i;
  OSuart_OutString(UART0_BASE,"Jitter Information:\n\r\n\r");
  OSuart_OutString(UART0_BASE,"Jitter for Periodic Task 1:\n\r");
  for(i = 0; i<JITTERSIZE; i++)
  {
    if(JitterHistogramA[i] != 0)
	{
	  Int2Str((i-32), printInd);
	  Int2Str(JitterHistogramA[i], string);
	  OSuart_OutString(UART0_BASE,printInd);
	  OSuart_OutString(UART0_BASE,": ");
      OSuart_OutString(UART0_BASE,string);
	  OSuart_OutString(UART0_BASE,"\n\r"); 
	}
  }
  OSuart_OutString(UART0_BASE,"Jitter for Periodic Task 2:\n\r");
  for(i = 0; i<JITTERSIZE; i++)
  {
    if(JitterHistogramB[i] != 0)
  	{
  	  Int2Str((i-32), printInd);
  	  Int2Str(JitterHistogramB[i], string);
  	  OSuart_OutString(UART0_BASE,printInd);
  	  OSuart_OutString(UART0_BASE,": ");
      OSuart_OutString(UART0_BASE,string);
  	  OSuart_OutString(UART0_BASE,"\n\r"); 
  	}
  }
    
}

//**************oLED Graphing Voltage vs. Freq/Time*****************
void SoundDisplay(void)
{

  static int index;
  char *string;
  for(;;){
    if(SoundVFreq)
    {
	    OS_Wait(&SoundReady);
	  RIT128x96x4PlotClear(0,1023);
		 
      for(index = 0; index < 128; index++)
   	  {
	    if(x[index] > 1000)
		{
		  Trigger = 1;
		}
	    data[index] = (short)y[index];
		if(data[index] < 0)
		{
		  data[index] = 0;
		}
		data[index] = data[index]&0x03FF;
        RIT128x96x4PlotdBfs((long)data[index]);
	    RIT128x96x4PlotNext();

	  }
	    if(Trigger){
		  for(index = 0; index < 64; index++){
		  if(SoundDumpInd < 200)
		  {
		    SoundDump[SoundDumpInd] = data[index];
		    SoundDumpInd++;
		  }
		}
		}
		
	  RIT128x96x4ShowPlot();
	  
      OS_Signal(&SoundRead);
    }
    else if(SoundVTime)
    {
	  OS_Wait(&SoundReady);
	  RIT128x96x4PlotClear(0,1023);
      for(index = 128; index < 256; index++) 
      {
	    if(x[index] > 600)
		{
		  Trigger = 1;
		}
        RIT128x96x4PlotPoint(x[index]);
		RIT128x96x4PlotNext();

		if(Trigger){
		  if(SoundDumpInd < 200)
		  {
		    SoundDump[SoundDumpInd] = x[index];
		    SoundDumpInd++;
		  }
		}
      }
	  RIT128x96x4ShowPlot();
	  OS_Signal(&SoundRead);
    }
  }
  
}

//*******************final user main DEMONTRATE THIS TO TA**********

int main(void){        // lab 3 real main
  OS_Init();           // initialize, disable interrupts

  OS_InitSemaphore(&MailBoxEmpty,1);
  OS_InitSemaphore(&MailBoxFull,0);
  OS_InitSemaphore(&SoundReady,0);
  OS_InitSemaphore(&SoundRead,1);

  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;
  SoundDumpInd = 0;

//********initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(64);    // ***note*** 4 is not big enough*****

//*******attach background tasks***********
  OS_AddButtonTask(&ButtonPush,2);
  OS_AddDownTask(&DownPush,3);
  OS_AddPeriodicThread(&DAS,PERIOD,1); // 2 kHz real time sampling

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddThread(&Consumer,128,1); 
  NumCreated += OS_AddThread(&SoundDisplay,128,1); 
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

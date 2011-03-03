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

Sema4Type MailBoxFull;
Sema4Type MailBoxEmpty;


unsigned char Buffer[100];  // Buffer size for interpreter input
unsigned int BufferPt = 0;	// Buffer pointer
unsigned short FirstSpace = 1; // Boolean to determine if first space has occured

// 10-sec finite time experiment duration 
#define RUNLENGTH 10000   // display results and quit when NumSamples==RUNLENGTH
long x[64],y[64];         // input and output arrays for FFT
void cr4_fft_64_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);


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
//--------------end of Task 1-----------------------------

//------------------Task 2--------------------------------
// background thread executes with select button
// one foreground task created with button push
// foreground treads run for 2 sec and die
// ***********ButtonWork*************
void ButtonWork(void){
unsigned long i;
unsigned long myId = OS_Id(); 
  oLED_Message(1,0,"NumCreated =",NumCreated); 
//  if(NumSamples < RUNLENGTH){   // finite time run
//    for(i=0;i<20;i++){  // runs for 2 seconds
//      OS_Sleep(50);     // set this to sleep for 0.1 sec
//    }
// }
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
  if(OS_AddThread(&ButtonWork,100,1)){
    NumCreated++; 
  }
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
// A timer runs at 1 kHz, started by your ADC_Collect
// The timer triggers the ADC, creating the 1 kHz sampling
// Your ADC ISR runs when ADC data is ready
// Your ADC ISR calls this function with a 10-bit sample 
// sends data to the consumer, runs periodically at 1 kHz
// inputs:  none
// outputs: none
void Producer(unsigned short data){  
  if(NumSamples < RUNLENGTH){   // finite time run
    NumSamples++;               // number of samples
    if(OS_Fifo_Put(data) == 0){ // send to consumer
      DataLost++;
    } 
  } 
}
void Display(void); 

//******** Consumer *************** 
// foreground thread, accepts data from producer
// calculates FFT, sends DC component to Display
// inputs:  none
// outputs: none
void Consumer(void){ 
unsigned long data,DCcomponent; // 10-bit raw ADC sample, 0 to 1023
unsigned long t;  // time in ms
unsigned long myId = OS_Id(); 
  ADC_Collect(0, 1000, &Producer); // start ADC sampling, channel 0, 1000 Hz
  NumCreated += OS_AddThread(&Display,128,0); 
  while(NumSamples < RUNLENGTH) { 
    for(t = 0; t < 64; t++){   // collect 64 ADC samples
      OS_Fifo_Get(&data);    // get from producer
      x[t] = data;             // real part is 0 to 1023, imaginary part is 0
    }
    cr4_fft_64_stm32(y,x,64);  // complex FFT of last 64 ADC values
    DCcomponent = y[0]&0xFFFF; // Real part at frequency 0, imaginary part should be zero
    
	OS_Wait(&MailBoxEmpty);
	OS_MailBox_Send(DCcomponent);
	OS_Signal(&MailBoxFull);
	
	OS_Sleep(15);
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
    PIDWork++;        // calculation finished
	OS_Sleep(2);
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
//*****************************************************************************
//
// Interpret input from the terminal. Supported functions include
// addition, subtraction, division, and multiplication in the post-fix format.
//
// \param nextChar specifies the next character to be put in the global buffer
//
// \return none.
//
//*****************************************************************************
void
OS_Interpret(unsigned char nextChar)
{
  char operator = Buffer[BufferPt - 2];
  char * token;
  char * last;
  char string[10];
  int a;
  long total = 0;
  short first = 1;
  short command, equation = 0; 
  switch(nextChar)
  {
    case '\b':
     if(BufferPt != 0)
	 {
	   BufferPt--;
	   Buffer[BufferPt] = '\0';
	 }
     break;
    case '=':
     FirstSpace = 1;
     Buffer[BufferPt] = '=';
     equation = 1;
     break;
    case '\r':
     command = 1;
	 //Buffer[BufferPt] = ' ';
     break;
    default:
     Buffer[BufferPt] = nextChar;
     BufferPt++;
     break;
  }

  if(equation == 1)
  {  
    switch(operator)
    {
      case '+':
        for ( token = strtok_r(Buffer, " ", &last); token; token = strtok_r(NULL , " ", &last) )
        {
          total = total + atoi(token);
        }
        Int2Str(total, string);
        OSuart_OutString(UART0_BASE, string);
        OSuart_OutString(UART0_BASE, "\r\n");
        break;  
          
        case '-':
        for ( token = strtok_r(Buffer, " ", &last); token; token = strtok_r(NULL , " ", &last) )
        {
          if(first)
          {
            total = atoi(token);
            first = 0;
          }
          else
          {
            total = total - atoi(token);
          }
        }
        Int2Str(total, string);
		OSuart_OutString(UART0_BASE, string);
        OSuart_OutString(UART0_BASE, "\r\n");
        break;
      
      case '*':
        for ( token = strtok_r(Buffer, " ", &last); token; token = strtok_r(NULL , " ", &last) )
        {
          if(first)
          {
            total = atoi(token);
            first = 0;
          }
          else
          {
            if(atoi(token) != 0)
            {
              total = total * atoi(token);
            }
          }
        }
        Int2Str(total, string);
		OSuart_OutString(UART0_BASE, string);
        OSuart_OutString(UART0_BASE, "\r\n");
        break;
      
      case '/':
        for ( token = strtok_r(Buffer, " ", &last); token; token = strtok_r(NULL , " ", &last) )
        {
          if(first)
          {
            total = atoi(token);
            first = 0;
          }
          else
          {
            if(atoi(token) != 0)
            {
              total = total / atoi(token);
            }
          }
        }
        Int2Str(total, string);
        OSuart_OutString(UART0_BASE, string);
        OSuart_OutString(UART0_BASE, "\r\n");
        break;
      
      case 't':
        Int2Str(OS_Time(), string);
        OSuart_OutString(UART0_BASE, string);
        OSuart_OutString(UART0_BASE, "\r\n");
        break;

      case 'j':
	    break;
	      
      
      default:
        break;
        
    }
	equation = 0;
    BufferPt = 0;
	memset(Buffer,'\0',100);
  }
  if(command == 1)
  {
    // Interpret all of the commands in the line
	//    time-jitter, number of data points lost, number of calculations performed
    //    i.e., NumSamples, NumCreated, MaxJitter-MinJitter, DataLost, FilterWork, PIDwork
    for ( token = strtok_r(Buffer, " ", &last); token; token = strtok_r(NULL , " ", &last) )
    {
	   //strcat(token, "\");
	   //string = "PIDWork";
	   // Display the number of samples
	   /*a = strcasecmp(token, "PIDWork");
	   Int2Str(a, string);
	   OSuart_OutString(UART0_BASE, string);*/
	   if(strcasecmp(token, "NumSamples") == 0)
	   {	 
		 Int2Str(NumSamples, string);
		 OSuart_OutString(UART0_BASE, " ="); 
		 OSuart_OutString(UART0_BASE, string);	
	   }

	   // Display number of samples created
	   if(strcasecmp(token, "NumCreated") == 0)
	   {	 
		 Int2Str(NumCreated, string); 
		 OSuart_OutString(UART0_BASE, " =");
		 OSuart_OutString(UART0_BASE, string);	
	   }

	   // Displays delta jitter
//	   if(strcasecmp(token, "MaxJitter-MinJitter") == 0)
//	   {	 
//		 Int2Str(MaxJitter-MinJitter, string);
//		 OSuart_OutString(UART0_BASE, " =");
//		 OSuart_OutString(UART0_BASE, string);	
//	   }
	   
	   // Display the amount of data lost
	   if(strcasecmp(token, "DataLost") == 0)
	   {	 
		 Int2Str(DataLost, string);
		 OSuart_OutString(UART0_BASE, " =");
		 OSuart_OutString(UART0_BASE, string);	
	   }

	   // Display the variable FilterWork
	   if(strcasecmp(token, "FilterWork") == 0)
	   {	 
		 Int2Str(FilterWork, string);
		 OSuart_OutString(UART0_BASE, " =");
		 OSuart_OutString(UART0_BASE, string);	
	   }

	   // Display the variable PIDWork
	   if(strcasecmp(token, "PIDWork") == 0)
	   {	 
		 Int2Str(PIDWork, string);
		 OSuart_OutString(UART0_BASE, " =");
		 OSuart_OutString(UART0_BASE, string);	
	   } 
    }
	  command = 0; 
  	BufferPt = 0;
	  memset(Buffer,'\0',100);
    OSuart_OutString(UART0_BASE, "\r\n");
  }
}  
unsigned long Count1;   // number of times thread1 loops
void Interpreter(void)
{
  unsigned char trigger;
  short fifo_status = 0;
  Count1 = 0;
  OSuart_Open();
  while(NumSamples<RUNLENGTH)
  {    
    fifo_status = UARTRxFifo_Get(&trigger);
    if(fifo_status == 1)
    OS_Interpret(trigger);
	OS_Sleep(3);   //Give PID a chance to run
	Count1++;
  }
  for(;;)
  {
    fifo_status = UARTRxFifo_Get(&trigger);
    if(fifo_status == 1)
    OS_Interpret(trigger);
	Count1++;
  }
}      
// 2) print debugging parameters 
//    i.e., x[], y[] 


//--------------end of Task 5-----------------------------

//*******************final user main DEMONTRATE THIS TO TA**********

int main(void){        // lab 3 real main
  OS_Init();           // initialize, disable interrupts

  OS_InitSemaphore(&MailBoxEmpty,1);
  OS_InitSemaphore(&MailBoxFull,0);

  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;

//********initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(64);    // ***note*** 4 is not big enough*****

//*******attach background tasks***********
  OS_AddButtonTask(&ButtonPush,2);
  OS_AddDownTask(&DownPush,3);
  OS_AddPeriodicThread(&DAS,PERIOD,1); // 2 kHz real time sampling

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,2); 
  NumCreated += OS_AddThread(&Consumer,128,1); 
  NumCreated += OS_AddThread(&PID,128,3); 
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

//+++++++++++++++++++++++++DEBUGGING CODE++++++++++++++++++++++++
// ONCE YOUR RTOS WORKS YOU CAN COMMENT OUT THE REMAINING CODE
// 
//*******************Initial TEST**********
// This is the simplest configuration, test this first
// run this with 
// no UART interrupts
// no SYSTICK interrupts
// no timer interrupts
// no select interrupts
// no ADC serial port or oLED output
// no calls to semaphores

unsigned long Count2;   // number of times thread2 loops
unsigned long Count3;   // number of times thread3 loops
unsigned long Count4;   // number of times thread4 loops
unsigned long Count5;   // number of times thread5 loops
void Thread1(void){
  Count1 = 0;          
  for(;;){
    Count1++;
    OS_Suspend();      // cooperative multitasking
  }
}
void Thread2(void){
  Count2 = 0;          
  for(;;){
    Count2++;
    OS_Suspend();      // cooperative multitasking
  }
}
void Thread3(void){
  Count3 = 0;          
  for(;;){
    Count3++;
    OS_Suspend();      // cooperative multitasking
  }
}
Sema4Type Free;       // used for mutual exclusion

int testmain1(void){       // testmain1
  OS_Init();          // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread1,128,1); 
  NumCreated += OS_AddThread(&Thread2,128,2); 
  NumCreated += OS_AddThread(&Thread3,128,3); 
  // Count1 runs, Count2 and Count3 are 0 because of starvation
  // change priority to equal to see round robin
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

//*******************Second TEST**********
// Once the initalize test runs, test this 
// no UART interrupts
// SYSTICK interrupts, with or without period established by OS_Launch
// no timer interrupts
// no select switch interrupts
// no ADC serial port or oLED output
// no calls to semaphores
void Thread1b(void){
  Count1 = 0;          
  for(;;){
    Count1++;
  }
}
void Thread2b(void){
  Count2 = 0;          
  for(;;){
    Count2++;
  }
}
void Thread3b(void){
  Count3 = 0;          
  for(;;){
    Count3++;
  }
}
int testmain2(void){  // testmain2
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread1b,128,1); 
  NumCreated += OS_AddThread(&Thread2b,128,2); 
  NumCreated += OS_AddThread(&Thread3b,128,3); 
  // Count1 runs, Count2 and Count3 are 0 because of starvation
  // change priority to equal to see round robin
  // counts are larger than testmain1
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}

//*******************Third TEST**********
// Once the second test runs, test this 
// no UART1 interrupts
// SYSTICK interrupts, with or without period established by OS_Launch
// Timer2 interrupts, with or without period established by OS_AddPeriodicThread
// PortF GPIO interrupts, active low
// no ADC serial port or oLED output
// tests the spinlock semaphores, tests Sleep and Kill
Sema4Type Readyc;        // set in background
int Lost;
void BackgroundThread1c(void){   // called at 1000 Hz
  Count1++;
  OS_Signal(&Readyc);
}
void Thread5c(void){
  for(;;){
    OS_Wait(&Readyc);
    Count5++;   // Count2 + Count5 should equal Count1 
    Lost = Count1-Count5-Count2;
  }
}
void Thread2c(void){
  OS_InitSemaphore(&Readyc,0);
  Count1 = 0;    // number of times signal is called      
  Count2 = 0;    
  Count5 = 0;    // Count2 + Count5 should equal Count1  
  NumCreated += OS_AddThread(&Thread5c,128,3); 
  OS_AddPeriodicThread(&BackgroundThread1c,TIME_1MS,0); 
  for(;;){
    OS_Wait(&Readyc);
    Count2++;   // Count2 + Count5 should equal Count1, Count5 may be 0
  }
}

void Thread3c(void){
  Count3 = 0;          
  for(;;){
    Count3++;
  }
}
void Thread4c(void){ int i;
  for(i=0;i<64;i++){
    Count4++;
    OS_Sleep(10);
  }
  OS_Kill();
  Count4 = 0;
}
void BackgroundThread5c(void){   // called when Select button pushed
  NumCreated += OS_AddThread(&Thread4c,128,3); 
}
      
int testmain3(void){   // Testmain3
  Count4 = 0;          
  OS_Init();           // initialize, disable interrupts
// Count2 + Count5 should equal Count1 (Count5 may be zero)
  NumCreated = 0 ;
  OS_AddButtonTask(&BackgroundThread5c,2);
  NumCreated += OS_AddThread(&Thread2c,128,2); 
  NumCreated += OS_AddThread(&Thread3c,128,3); 
  NumCreated += OS_AddThread(&Thread4c,128,3); 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;  // this never executes
}

//*******************Fourth TEST**********
// Once the third test runs, run this example
// Count1 should exactly equal Count2
// Count3 should be very large
// Count4 increases by 640 every time select is pressed
// NumCreated increase by 1 every time select is pressed

// no UART interrupts
// SYSTICK interrupts, with or without period established by OS_Launch
// Timer interrupts, with or without period established by OS_AddPeriodicThread
// Select switch interrupts, active low
// no ADC serial port or oLED output
// tests the spinlock semaphores, tests Sleep and Kill
Sema4Type Readyd;        // set in background
void BackgroundThread1d(void){   // called at 1000 Hz
static int i=0;
  i++;
  if(i==50){
    i = 0;         //every 50 ms
    Count1++;
    OS_bSignal(&Readyd);
  }
}
void Thread2d(void){
  OS_InitSemaphore(&Readyd,0);
  Count1 = 0;          
  Count2 = 0;          
  for(;;){
    OS_bWait(&Readyd);
    Count2++;     
  }
}
void Thread3d(void){
  Count3 = 0;          
  for(;;){
    Count3++;
  }
}
void Thread4d(void){ int i;
  for(i=0;i<640;i++){
    Count4++;
    OS_Sleep(1);
  }
  OS_Kill();
}
void BackgroundThread5d(void){   // called when Select button pushed
  NumCreated += OS_AddThread(&Thread4d,128,3); 
}
int Testmain4(void){   // Testmain4
  Count4 = 0;          
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  OS_AddPeriodicThread(&BackgroundThread1d,PERIOD,0); 
  OS_AddButtonTask(&BackgroundThread5d,2);
  NumCreated += OS_AddThread(&Thread2d,128,2); 
  NumCreated += OS_AddThread(&Thread3d,128,3); 
  NumCreated += OS_AddThread(&Thread4d,128,3); 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;  // this never executes
}


//******************* Lab 3 Preparation 2**********
// Modify this so it runs with your RTOS (i.e., fix the time units to match your OS)
// run this with 
// UART0, 115200 baud rate, used to output results 
// SYSTICK interrupts, period established by OS_Launch
// first timer interrupts, period established by first call to OS_AddPeriodicThread
// second timer interrupts, period established by second call to OS_AddPeriodicThread
// Select no interrupts
// Down no interrupts
unsigned long CountA;   // number of times Task A called
unsigned long CountB;   // number of times Task B called
unsigned long Count1;   // number of times thread1 loops
#define GPIO_PB0  (*((volatile unsigned long *)0x40005004))
#define GPIO_PB1  (*((volatile unsigned long *)0x40005008))
#define GPIO_PB2  (*((volatile unsigned long *)0x40005010))

//*******PseudoWork*************
// simple time delay, simulates user program doing real work
// Input: amount of work in 100ns units (free free to change units
// Output: none
long timeDif;
void PseudoWork(unsigned short work){
unsigned long startTime;
  startTime = OS_Time();    // time in 100ns units
  timeDif = OS_TimeDifference(OS_Time(),startTime); 
  while(timeDif <= (long)work)
  {
    timeDif = OS_TimeDifference(OS_Time(),startTime);
  } 
}
void Thread6(void){  // foreground thread
  Count1 = 0;          
  for(;;){
    Count1++; 
    GPIO_PB0 ^= 0x01;        // debugging toggle bit 0  CREATES A LOT OF JITTER!
  }
}

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

void Thread7(void){  // foreground thread
  OSuart_OutString(UART0_BASE,"\n\rEE345M/EE380L, Lab 3 Preparation 2\n\r");
  OS_Sleep(5000);   // 10 seconds        
  Jitter();         // print jitter information
  OSuart_OutString(UART0_BASE,"\n\r\n\r");
  OS_Kill();
}
#define workA 200       // {5,50,500 us} work in Task A
#define counts1us 50    // number of OS_Time counts per 1us
void TaskA(void){       // called every {1000, 2990us} in background
  GPIO_PB1 = 0x02;      // debugging profile  
  CountA++;
  PseudoWork(workA*counts1us); //  do work (100ns time resolution)
  GPIO_PB1 = 0x00;      // debugging profile  
}
#define workB 125       // 250 us work in Task B
void TaskB(void){       // called every pB in background
  GPIO_PB2 = 0x04;      // debugging profile  
  CountB++;
  PseudoWork(workB*counts1us); //  do work (100ns time resolution)
  GPIO_PB2 = 0x00;      // debugging profile  
}

int testmain5(void){       // Testmain5
  volatile unsigned long delay;
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread6,128,2); 
  NumCreated += OS_AddThread(&Thread7,128,1); 
  OS_AddPeriodicThread(&TaskA,(TIME_1MS*111)/100,0);           // 1 ms, higher priority
  OS_AddPeriodicThread(&TaskB,TIME_1MS,1);         // 2 ms, lower priority
  OS_Launch(TIMESLICE); // 2ms, doesn't return, interrupts enabled in here
  return 0;             // this never executes
}


//******************* Preparation 4**********
// Modify this so it runs with your RTOS used to test blocking semaphores
// run this with 
// UART0, 115200 baud rate,  used to output results 
// SYSTICK interrupts, period established by OS_Launch
// first timer interrupts, period established by first call to OS_AddPeriodicThread
// second timer interrupts, period established by second call to OS_AddPeriodicThread
// select interrupts, 
// down no interrupts
Sema4Type s;            // test of this counting semaphore
unsigned long SignalCount1;   // number of times s is signaled
unsigned long SignalCount2;   // number of times s is signaled
unsigned long SignalCount3;   // number of times s is signaled
unsigned long WaitCount1;     // number of times s is successfully waited on
unsigned long WaitCount2;     // number of times s is successfully waited on
unsigned long WaitCount3;     // number of times s is successfully waited on
#define MAXCOUNT 20000
void OutputThread(void){  // foreground thread
  char sigStr[7], waitStr[7];
  OSuart_OutString(UART0_BASE,"\n\rEE345M/EE380L, Lab 3 Preparation 4\n\r");
  while(SignalCount1+SignalCount2+SignalCount3<100*MAXCOUNT){
    OS_Sleep(1000);   // 1 second
    OSuart_OutString(UART0_BASE,".");
  }       
  OSuart_OutString(UART0_BASE," done\n\r");
  Int2Str(SignalCount1+SignalCount2+SignalCount3, sigStr);
  Int2Str(WaitCount1+WaitCount2+WaitCount3, waitStr);
  OSuart_OutString(UART0_BASE,"Signalled = ");
  OSuart_OutString(UART0_BASE,sigStr);
  OSuart_OutString(UART0_BASE,"Waited = ");
  OSuart_OutString(UART0_BASE,waitStr);

  OS_Kill();
}
void Wait1(void){  // foreground thread
  for(;;){
    OS_Wait(&s);    // three threads waiting
    WaitCount1++; 
  }
}
void Wait2(void){  // foreground thread
  for(;;){
    OS_Wait(&s);    // three threads waiting
    WaitCount2++; 
  }
}
void Wait3(void){   // foreground thread
  for(;;){
    OS_Wait(&s);    // three threads waiting
    WaitCount3++; 
  }
}
void Signal1(void){      // called every 799us in background
  if(SignalCount1<MAXCOUNT){
    OS_Signal(&s);
    SignalCount1++;
  }
}
// edit this so it changes the periodic rate
void Signal2(void){       // called every 1111us in background
  if(SignalCount2<MAXCOUNT){
    OS_Signal(&s);
    SignalCount2++;
//    if((SignalCount2%1000)<500){
//      TIMER2_TAILR_R += 3;  // increasing period
//	} else{
//	  TIMER2_TAILR_R -= 3;  // decreasing period
//	}
  }
}
void Signal3(void){       // foreground
  while(SignalCount3<98*MAXCOUNT){
    OS_Signal(&s);
    SignalCount3++;
  }
  OS_Kill();
}

long add(const long n, const long m){
static long result;
  result = m+n;
  return result;
}
int testmain6(void){      // Testmain6
  volatile unsigned long delay;
  OS_Init();           // initialize, disable interrupts
  delay = add(3,4);

  SignalCount1 = 0;   // number of times s is signaled
  SignalCount2 = 0;   // number of times s is signaled
  SignalCount3 = 0;   // number of times s is signaled
  WaitCount1 = 0;     // number of times s is successfully waited on
  WaitCount2 = 0;     // number of times s is successfully waited on
  WaitCount3 = 0;	  // number of times s is successfully waited on
  OS_InitSemaphore(&s,0);	 // this is the test semaphore
  OS_AddPeriodicThread(&Signal1,(799*TIME_1MS)/1000,0);   // 0.799 ms, higher priority
  OS_AddPeriodicThread(&Signal2,(1111*TIME_1MS)/1000,1);  // 1.111 ms, lower priority
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread6,128,6);    	// idle thread to keep from crashing
  NumCreated += OS_AddThread(&OutputThread,128,2); 	// results output thread
  NumCreated += OS_AddThread(&Signal3,128,2); 	// signalling thread
  NumCreated += OS_AddThread(&Wait1,128,2); 	// waiting thread
  NumCreated += OS_AddThread(&Wait2,128,2); 	// waiting thread
  NumCreated += OS_AddThread(&Wait3,128,2); 	// waiting thread
 
  OS_Launch(TIMESLICE);  // 1ms, doesn't return, interrupts enabled in here
  return 0;             // this never executes
}


//******************* Measurement of context switch time**********
// Run this to measure the time it takes to perform a task switch
// UART0 not needed 
// SYSTICK interrupts, period established by OS_Launch
// first timer not needed
// second timer not needed
// select not needed, 
// down not needed
// logic analyzer on PF0 for systick interrupt
//                on PB0 to measure context switch time
int testmain7(void){   // testmain7
  volatile unsigned long delay;
  OS_Init();           // initialize, disable interrupts
//  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB; // activate port B
//  delay = SYSCTL_RCGC2_R;     // allow time to finish activating
//  GPIO_PORTB_DIR_R |= 0x07;   // make PB2, PB1, PB0 out
//  GPIO_PORTB_DEN_R |= 0x07;   // enable digital I/O on PB2, PB1, PB0
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread6,128,1); // look at Port B0 on scope 
  OS_Launch(TIME_1MS/10);  // 0.1ms, doesn't return, interrupts enabled in here
  return 0;             // this never executes
}






//*****************************************************************************
//
// Lab2 Tester Code.  This file includes four main testing programs, and 
// associated functions.
//
//*****************************************************************************


#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/fifo.h"
#include "driverlib/adc.h"
#include "drivers/rit128x96x4.h"
#include "drivers/OS.h"
#include "drivers/OSuart.h"
#include "string.h"
#include "ctype.h"

unsigned long NumCreated;   // number of foreground threads created
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished
volatile unsigned long NumSamples;   // incremented every sample
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
long MaxJitter;             // largest time jitter between interrupts in usec
long MinJitter;             // smallest time jitter between interrupts in usec
unsigned long JitterHistogram[JITTERSIZE]={0,};
unsigned char Buffer[100];  // Buffer size for interpreter input
unsigned int BufferPt = 0;	// Buffer pointer
unsigned short FirstSpace = 1; // Boolean to determine if first space has occured

#define GPIO_B0 (*((volatile unsigned long *)(0x40005004)))
#define GPIO_B1 (*((volatile unsigned long *)(0x40005008)))
#define GPIO_B2 (*((volatile unsigned long *)(0x40005010)))
#define GPIO_B3 (*((volatile unsigned long *)(0x40005020)))

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
void DAS(void)
{ 
  int index;
  static int i = 0;
  unsigned short input;
  static unsigned long LastTime;  // time at previous ADC sample  
  unsigned long thisTime;         // time at current ADC sample
  long jitter;                    // time between measured and expected
    if(NumSamples < RUNLENGTH){   // finite time run
	/*
	GPIO_B0 ^= 0x01;
      input = ADC_In(1);    
	  thisTime = OS_Time();       // current time, 20 ns
      DASoutput = Filter(input);
      FilterWork++;        // calculation finished
      if(FilterWork>1){    // ignore timing of first interrupt
        jitter = ((OS_TimeDifference(thisTime,LastTime)-PERIOD)*CLOCK_PERIOD)/1000;  // in usec
        if(jitter > MaxJitter){
          MaxJitter = jitter;
        }
        if(jitter < MinJitter){
          MinJitter = jitter;
        }        // jitter should be 0
        index = jitter+JITTERSIZE/2;   // us units
        if(index<0)index = 0;
        if(index>=JitterSize)index = JITTERSIZE-1;
        JitterHistogram[index]++; 
      }
      LastTime = thisTime; 
	  */
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
  if(NumSamples < RUNLENGTH){   // finite time run
    for(i=0;i<20;i++){  // runs for 2 seconds
      //OS_Sleep(20);     // set this to sleep for 0.1 sec
    }
  }
  oLED_Message(1,1,"PIDWork    =",PIDWork);
  oLED_Message(1,2,"DataLost   =",DataLost);
  oLED_Message(1,3,"Jitter(us) =",MaxJitter-MinJitter);
  OS_Kill();  // done
} 

//************ButtonPush*************
// Called when Select Button pushed
// Adds another foreground task
// background threads execute once and return
void ButtonPush(void){
  if(OS_AddThread(&ButtonWork,100,4)){
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
  GPIO_B1 ^= 0x02;
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
    OS_MailBox_Send(DCcomponent);
	GPIO_B2 ^= 0x04; 
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
    data = OS_MailBox_Recv();
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
    GPIO_B3 ^= 0x08; 
    for(err = -1000; err <= 1000; err++){    // made-up data
      Actuator = PID_stm32(err,Coeff)/256;
    }
    PIDWork++;        // calculation finished
  }
  for(;;){ }          // done
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
void Interpreter(void);    // just a prototype, link to your interpreter
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
	   if(strcasecmp(token, "MaxJitter-MinJitter") == 0)
	   {	 
		 Int2Str(MaxJitter-MinJitter, string);
		 OSuart_OutString(UART0_BASE, " =");
		 OSuart_OutString(UART0_BASE, string);	
	   }
	   
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

void Interpreter(void)
{
  unsigned char trigger;
  short fifo_status = 0;
  OSuart_Open();
  for(;;)
  {    
    fifo_status = UARTRxFifo_Get(&trigger);
    if(fifo_status == 1)
    OS_Interpret(trigger);
  }
}

      
// 2) print debugging parameters 
//    i.e., x[], y[] 
//--------------end of Task 5-----------------------------
unsigned long Count1;   // number of times thread1 loops
void Thread1b(void){
  Count1 = 0;          
  for(;;){
    Count1++;
  }
}

//*******************final user main DEMONTRATE THIS TO TA**********
int Mainmain(void){ 

  // Set the clocking to run from PLL at 50 MHz 
  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);

  OS_Init();           // initialize, disable interrupts

  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;
  MaxJitter = 0;       // OS_Time in 20ns units
  MinJitter = 10000000;

//********initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(32);    // ***note*** 4 is not big enough*****

//*******attach background tasks***********
  OS_AddButtonTask(&ButtonPush,2);
  
  OS_AddPeriodicThread(&DAS,PERIOD,0); // 2 kHz real time sampling

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,2); 
  NumCreated += OS_AddThread(&Consumer,128,1); 
  NumCreated += OS_AddThread(&PID,128,3);
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  while(1);
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

int testmain1(void){ 
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread1,128,1); 
  NumCreated += OS_AddThread(&Thread2,128,2); 
  NumCreated += OS_AddThread(&Thread3,128,3); 
 
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

void Dummy1(void){
  Count4++;
}

void Dummy2(void){
  Count5++;
}
int main(void){  // testmain2
  OS_Init();           // initialize, disable interrupts
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread1b,128,1); 
  NumCreated += OS_AddThread(&Thread2b,128,1); 
  NumCreated += OS_AddThread(&Thread3b,128,3);
  OS_AddPeriodicThread(&Dummy1, PERIOD, 1);
  OS_AddPeriodicThread(&Dummy2, PERIOD, 2); 
 
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
    Count2++;   // Count2 + Count5 should equal Count1
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
      
int Testmain3(void){   // Testmain3
  Count4 = 0;          
  OS_Init();           // initialize, disable interrupts

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
int testmain4(void){   // Testmain4
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

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


//******** Robot *************** 
// foreground thread, accepts data from producer
// inputs:  none
// outputs: none
void Robot(void){   
unsigned long data;      // ADC sample, 0 to 1023
unsigned long voltage;   // in mV,      0 to 3000
unsigned long time = 0;      // in 10msec,  0 to 1000 
unsigned long t=0;
unsigned int i;
  //OS_ClearMsTime();
  char string[100];    
  DataLost = 0;          // new run with no lost data 
  OSuart_OutString(UART0_BASE, "Robot running...");
  eFile_Create("Robot");
  eFile_RedirectToFile("Robot");
  OSuart_OutString(UART0_BASE, "time(sec)\tdata(volts)\n\r");
  for(i = 1000; i > 0; i--){
    t++;
    time+=OS_Time()/10000;            // 10ms resolution in this OS
    while(!OS_Fifo_Get(&data));        // 1000 Hz sampling get from producer
    voltage = (300*data)/1024;   // in mV
    sprintf(string, "%0u.%02u\t\t%0u.%03u\n\r",time/100,time%100,voltage/1000,voltage%1000);
    OSuart_OutString(UART0_BASE, string);
  }
  eFile_EndRedirectToFile();
  OSuart_OutString(UART0_BASE, "done.\n\r");										    
  Running = 0;                // robot no longer running
  OS_Kill();
}
  




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
  unsigned long data;      // ADC sample, 0 to 1023
  unsigned long voltage;   // in mV,      0 to 3000
  unsigned long time = 0;      // in 10msec,  0 to 1000 
  unsigned long t=0;
  char string[100];
  unsigned int i;    
  DataLost = 0;          // new run with no lost data 
  OSuart_OutString(UART0_BASE, "Robot running 2...");
  eFile_Create("Robot2");
  eFile_RedirectToFile("Robot2");
  OSuart_OutString(UART0_BASE, "time(sec)\tdata(volts)\n\r");
  for(i = 1000; i > 0; i--){
    t++;
    time+=OS_Time()/10000;            // 10ms resolution in this OS
    while(!OS_Fifo_Get(&data));        // 1000 Hz sampling get from producer
    voltage = (300*data)/1024;   // in mV
    sprintf(string, "%0u.%02u\t\t%0u.%03u\n\r",time/100,time%100,voltage/1000,voltage%1000);
    OSuart_OutString(UART0_BASE, string);
  }

  eFile_EndRedirectToFile();
  OSuart_OutString(UART0_BASE, "done 2.\n\r");
}


//******** Interpreter **************
// your intepreter from Lab 4 
// foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
extern void Interpreter(void); 
// add the following commands, remove commands that do not make sense anymore
// 1) format 
// 2) directory 
// 3) print file
// 4) delete file
// execute   eFile_Init();  after periodic interrupts have started

//************ButtonPush*************
// Called when Select Button pushed
// background threads execute once and return
void ButtonPush(void){
  if(Running==0){
    Running = 1;  // prevents you from starting two robot threads
    NumCreated += OS_AddThread(&Robot,128,1);  // start a 20 second run
	NumCreated += OS_AddThread(&IdleTask,128,1);  // start a 20 second run
  }
}
//************DownPush*************
// Called when Down Button pushed
// background threads execute once and return
void DownPush(void){

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
  OS_AddPeriodicThread(disk_timerproc,TIME_1MS,5);

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,2); 
//  NumCreated += OS_AddThread(&IdleTask,128,7);  // runs when nothing useful to do
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}


//*****************test programs*************************
unsigned char buffer[512];
#define MAXBLOCKS 100
void diskError(char* errtype, unsigned long n){
  char string[100];
  OSuart_OutString(UART0_BASE, errtype);
  sprintf(string," disk error %u",n);
  OSuart_OutString(UART0_BASE, string);
//  OS_Kill();
}
void TestDisk(void){  DSTATUS result;  unsigned short block;  int i; unsigned long n;
  // simple test of eDisk
  char string[100];
  OSuart_OutString(UART0_BASE, "\n\rEE345M/EE380L, Lab 5 eDisk test\n\r");
  result = eDisk_Init(0);  // initialize disk
  if(result) diskError("eDisk_Init",result);
  OSuart_OutString(UART0_BASE, "Writing blocks\n\r");
  n = 1;    // seed
  for(block = 0; block < MAXBLOCKS; block++){
    for(i=0;i<512;i++){
      n = (16807*n)%2147483647; // pseudo random sequence
      buffer[i] = 0xFF&n;        
    }
    GPIO_PF3 = 0x08;     // PF3 high for 100 block writes
    if(eDisk_WriteBlock(buffer,block))diskError("eDisk_WriteBlock",block); // save to disk
    GPIO_PF3 = 0x00;     
  }  
  OSuart_OutString(UART0_BASE, "Reading blocks\n\r");
  n = 1;  // reseed, start over to get the same sequence
  for(block = 0; block < MAXBLOCKS; block++){
    GPIO_PF2 = 0x04;     // PF2 high for one block read
    if(eDisk_ReadBlock(buffer,block))diskError("eDisk_ReadBlock",block); // read from disk
    GPIO_PF2 = 0x00;
    for(i=0;i<512;i++){
      n = (16807*n)%2147483647; // pseudo random sequence
      if(buffer[i] != (0xFF&n)){
        sprintf(string, "Read data not correct, block=%u, i=%u, expected %u, read %u\n\r",block,i,(0xFF&n),buffer[i]);
        OSuart_OutString(UART0_BASE, string);
        OS_Kill();
      }      
    }
  }
  sprintf(string,  "Successful test of %i blocks\n\r",MAXBLOCKS);
  OSuart_OutString(UART0_BASE, string);
  OS_Kill();
}
void RunTest(void){
  NumCreated += OS_AddThread(&TestDisk,128,1);  
}

void TestFile(void){   int i; char data; DSTATUS result; 
  OSuart_OutString(UART0_BASE, "\n\rEE345M/EE380L, Lab 5 eFile test\n\r");
  // simple test of eFile
  result = eDisk_Init(0);  // initialize disk
  if(result) diskError("eDisk_Init",result);
  if(eFile_Init())              diskError("eFile_Init",0); 
//  if(eFile_Format())            diskError("eFile_Format",0); 
  eFile_Directory();
  if(eFile_ROpen("file1"))      diskError("eFile_ROpen",0);
  eFile_Directory();
  for(i=0;i<1000;i++){
    if(eFile_ReadNext(&data))   diskError("eFile_ReadNext",i);
    OSuart_OutChar(UART0_BASE, data);
	SysCtlDelay(SysCtlClockGet()/10000);
  }
  eFile_Directory();	
  if(eFile_Create("file1"))     diskError("eFile_Create",0);
  if(eFile_WOpen("file1"))      diskError("eFile_WOpen",0);
  for(i=0;i<1000;i++){
    if(eFile_Write('a'+i%26))   diskError("eFile_Write",i);
    if(i%52==51){
      if(eFile_Write('\n'))     diskError("eFile_Write",i);  
      if(eFile_Write('\r'))     diskError("eFile_Write",i);
    }
  }
  if(eFile_WClose())            diskError("eFile_Close",0);
  eFile_Directory();
  if(eFile_Create("file2"))     diskError("eFile_Create",0);
  if(eFile_WOpen("file2"))      diskError("eFile_WOpen",0);
  for(i=0;i<1000;i++){
    if(eFile_Write('a'+i%26))   diskError("eFile_Write",i);
    if(i%52==51){
      if(eFile_Write('\n'))     diskError("eFile_Write",i);  
      if(eFile_Write('\r'))     diskError("eFile_Write",i);
    }
  }
  if(eFile_WClose())            diskError("eFile_Close",0);
  eFile_Directory();
/*  if(eFile_ROpen("file1"))      diskError("eFile_ROpen",0);
  eFile_Directory();
  for(i=0;i<1000;i++){
    if(eFile_ReadNext(&data))   diskError("eFile_ReadNext",i);
    OSuart_OutChar(UART0_BASE, data);
	SysCtlDelay(SysCtlClockGet()/10000);
  }
  eFile_Directory();
  if(eFile_Delete("file1"))     diskError("eFile_Delete",0);
//  eFile_Directory();	 */
  OSuart_OutString(UART0_BASE, "Successful test of creating a file\n\r");
  OS_Kill();
}
//******************* test main1 **********
// SYSTICK interrupts, period established by OS_Launch
// Timer interrupts, period established by first call to OS_AddPeriodicThread
int testmain1(void){   // testmain1
  OS_Init();           // initialize, disable interrupts
//*******attach background tasks***********
  OS_AddPeriodicThread(&disk_timerproc,TIME_1MS,0);   // time out routines for disk
  OS_AddButtonTask(&RunTest,2);
  
  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&TestFile,128,1);  
  NumCreated += OS_AddThread(&IdleTask,128,1); 
 
  OS_Launch(TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}



//******************* test main2 **********
// SYSTICK interrupts, period established by OS_Launch
// Timer interrupts, period established by first call to OS_AddPeriodicThread
int testmain2(void){ 
  OS_Init();           // initialize, disable interrupts

//*******attach background tasks***********
  OS_AddPeriodicThread(&disk_timerproc,10*TIME_1MS,0);   // time out routines for disk
  
  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&TestFile,128,1);  
  NumCreated += OS_AddThread(&IdleTask,128,3); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}

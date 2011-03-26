//*****************************************************************************
//
// Filename: OSuart.c 
// Authors: Dustin Replogle, Katy Loeffler   
// Initial Creation Date: January 26, 2011    	 
// Lab Number. 04    
// TA: Raffaele Cetrulo      
// Date of last revision: March 24, 2011      
// Hardware Configuration: default
//
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
#include "driverlib/adc.h"
#include "driverlib/fifo.h"
#include "drivers/rit128x96x4.h"
#include "string.h"
#include "drivers/OS.h"
#include "drivers/OSuart.h"

// Global Variables
  AddFifo(UARTRx, 256, unsigned char, 1, 0);   // UARTRx Buffer
  AddFifo(UARTTx, 256, unsigned char, 1, 0);   // UARTTx Buffer

// Private Function Prototypes
void printSound(void);
void printTime(void);

#define NUM_EVENTS 100
#define PER_THREAD_START 0x04
#define PER_THREAD_END   0x08
#define FOREGROUND_THREAD_START 0x03
#define GPIO_B3 (*((volatile unsigned long *)(0x40005020)))

// Private Functions
void UARTSend(const unsigned char *pucBuffer, unsigned long ulCount);
unsigned char Buffer[100];  // Buffer size for interpreter input
unsigned int BufferPt = 0;	// Buffer pointer
unsigned short FirstSpace = 1; // Boolean to determine if first space has occured

extern unsigned long NumCreated;   // number of foreground threads created
extern unsigned long NumSamples;   // incremented every sample
extern unsigned long PIDWork;      // current number of PID calculations finished
extern unsigned long FilterWork;   // number of digital filter calculations finished
extern unsigned long DataLost;     // data sent by Producer, but not received by Consumer
extern unsigned long CumulativeRunTime;
extern unsigned long TimeIbitDisabled;
extern unsigned long RunTimeProfile[NUM_EVENTS][2];
extern int EventIndex;
extern long SoundDump[100];
extern unsigned long CumLastTime;  // time at previous interrupt
extern unsigned short FilterOn;
extern short data[64];
extern unsigned short SoundVFreq;
extern unsigned short SoundVTime;  


//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

//*****************************************************************************
//
// The UART interrupt handler.
//
//*****************************************************************************
void
UARTIntHandler(void)
{
  unsigned long ulStatus;
  unsigned char uartData;
    //
    // Get the interrrupt status.
    //
    ulStatus = UARTIntStatus(UART0_BASE, true);
  //
    // Clear the asserted interrupts.
    //
    UARTIntClear(UART0_BASE, ulStatus);

  if(ulStatus == UART_INT_RX || ulStatus == UART_INT_RT)
  {
    //
    // Loop while there are characters in the receive FIFO.
    //
    while(UARTCharsAvail(UART0_BASE))
    {
      //
      // Read the next character from the UART, write it back to the UART,
      // and place it in the receive SW FIFO.
      //
      uartData =  (char)UARTCharGetNonBlocking(UART0_BASE);
      if(uartData != '\r')
	    {
	  	  UARTCharPutNonBlocking(UART0_BASE,uartData); //echo data
	    }

      if(!UARTRxFifo_Put(uartData))
      {
		    //error 
      }
    }
  }

  if(ulStatus == UART_INT_TX)
  {
    //
    // If there is room in the HW FIFO and there is data in the SW FIFO,
    // then move data from the SW to the HW FIFO.
    //
    while(UARTSpaceAvail(UART0_BASE) && UARTTxFifo_Get(&uartData)) 
    {
      UARTCharPut(UART0_BASE,uartData);
    }
  }        
}    


//*****************************************************************************
//
// Send a string to the UART.
//
//*****************************************************************************
void
UARTSend(const unsigned char *pucBuffer, unsigned long ulCount)
{
    //
    // Loop while there are more characters to send.
    //
    while(ulCount--)
    {
        //
        // Write the next character to the UART.
        //
        UARTCharPutNonBlocking(UART0_BASE, *pucBuffer++);
    }
}
extern void Jitter(void);


//*****************************************************************************
//
// Interpret input from the terminal. Supported functions include
// addition, subtraction, division, and multiplication in the post-fix format.
//
// Typing the command 'list' will display all of the active commands on the 
// console.
//
// \param nextChar specifies the next character to be put in the global buffer
//
// \return none.
//
//*****************************************************************************
void
OSuart_Interpret(unsigned char nextChar)
{
  char operator = Buffer[BufferPt - 2];
  char * token, * last;
  char string[10];
  long total = 0;
  short first = 1;
  short command, equation, cmdptr = 0; 
  short event = 0;
  const short numcommands = 14;
  char * commands[numcommands] = {"NumSamples", "NumCreated", "DataLost", "FilterWork", "PIDWork", "Ibit", 
                                  "cleartime", "timedump", "cleardump", "togglefilter", "fftdump", "jitter",
                                  "togglegraph", "sounddump"};
  char * descriptions[numcommands] = {" - Display NumSamples\r\n", " - Display NumCreated\r\n", " - Display DataLost\r\n",
                                      " - Display FilterWork\r\n", " - Display PIDWork\r\n", " - Display % time i bit disabled\r\n",
                                      " - clear the thread timestamps\r\n", " - dump timestamps\r\n", " - clear thread dumps\r\n", 
                                      " - Toggles FIR filter on and off\r\n", " - dump fft results\r\n", " - output Jitter command\r\n",
                                      " - Toggles graph between V vs. T and V vs. Freq\r\n"};
  switch(nextChar)
  {
    case '\x7F':
     if(BufferPt != 0)
	   {
	    BufferPt--;
	    Buffer[BufferPt] = '\0';
	   }
     break;
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
    token = strtok_r(Buffer, " ", &last);
    do
    {
	   if(strcasecmp(token, "list") == 0)
	   {	 
		  OSuart_OutString(UART0_BASE, "\r\n");
      for(event = 0; event < numcommands; event++)
      {
        
        OSuart_OutString(UART0_BASE, commands[event]);
        OSuart_OutString(UART0_BASE, descriptions[event]);
      }
	   }
     // Display the number of samples
	   if(strcasecmp(token, commands[cmdptr]) == 0)         //numsamples
	   {	 
		  Int2Str(NumSamples, string);
		  OSuart_OutString(UART0_BASE, " ="); 
		  OSuart_OutString(UART0_BASE, string);	
	   }
     cmdptr++;
	   // Display number of samples created
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //numcreated
	   {	 
		 Int2Str(NumCreated, string); 
		 OSuart_OutString(UART0_BASE, " =");
		 OSuart_OutString(UART0_BASE, string);	
	   }
     cmdptr++;                                                //datalost
	   if(strcasecmp(token, commands[cmdptr]) == 0)
	   {	 
		  Int2Str(DataLost, string);
		  OSuart_OutString(UART0_BASE, " =");
		  OSuart_OutString(UART0_BASE, string);	
	   }
     cmdptr++;
	   // Display the variable FilterWork
	   if(strcasecmp(token, commands[cmdptr]) == 0)         //filterwork
	   {	 
		  Int2Str(FilterWork, string);
		  OSuart_OutString(UART0_BASE, " =");
		  OSuart_OutString(UART0_BASE, string);	
	   }
     cmdptr++;

	   // Display the variable PIDWork
	   if(strcasecmp(token, commands[cmdptr]) == 0)            //pidwork
	   {	 
  		 Int2Str(PIDWork, string);
  		 OSuart_OutString(UART0_BASE, " =");
  		 OSuart_OutString(UART0_BASE, string);	
	   }
     cmdptr++;
     if(strcasecmp(token, commands[cmdptr]) == 0)                //ibit
	   {	 
      sprintf(string, "%u", (100*TimeIbitDisabled)/CumulativeRunTime);
		  OSuart_OutString(UART0_BASE, "\r\nPercentage of time I bit is disabled = ");
		  OSuart_OutString(UART0_BASE, string);	
	   }
     cmdptr++;                                                  
     if(strcasecmp(token, commands[cmdptr]) == 0)           //cleartime
	   {	 
      TimeIbitDisabled = 0;
      sprintf(string, "%u", TimeIbitDisabled);
		  OSuart_OutString(UART0_BASE, "\r\nTimeIbitDisabled = ");
		  OSuart_OutString(UART0_BASE, string);	
	   }
     cmdptr++;
     if(strcasecmp(token, commands[cmdptr]) == 0)             //timedump
	   {	 
      printTime(); 
     }
     cmdptr++;
     if(strcasecmp(token, commands[cmdptr]) == 0)            //cleardump
     {
       for(event = 0; event < NUM_EVENTS; event++)
       {
         RunTimeProfile[event][0] = 0;
         RunTimeProfile[event][1] = 0;
       }
       CumulativeRunTime = 0;
       EventIndex = 0;
       CumLastTime = 0;
     }
     cmdptr++;
     if(strcasecmp(token, commands[cmdptr]) == 0)        //togglefilter
     {
        FilterOn ^= 0x1;

     }
     cmdptr++;
     if(strcasecmp(token, commands[cmdptr]) == 0)        //fftdump
     {
       OSuart_OutString(UART0_BASE, commands[cmdptr]);
       for(event = 0; event < 64; event++)
       {
//          sprintf(string, "\r\n%i. ", event);
//          OSuart_OutString(UART0_BASE, string);
		  OSuart_OutString(UART0_BASE, "\r\n");
          sprintf(string, "%hi", data[event]);
          OSuart_OutString(UART0_BASE, string);
       }
     }
     cmdptr++; 
     if(strcasecmp(token, commands[cmdptr]) == 0)        //Jitter
     {
       Jitter();
     }
     cmdptr++;
     if(strcasecmp(token, commands[cmdptr]) == 0)        //togglegraph
     {
       SoundVFreq ^= 0x1;
       SoundVTime ^= 0x1;
     } 
     cmdptr++;
     if(strcasecmp(token, commands[cmdptr]) == 0)        //sound dump
     {
       printSound();
     } 

          
     token = strtok_r(NULL , " ", &last);  	
     } 
     while(token);
     cmdptr = 0;
     command = 0; 
  	 BufferPt = 0;
	   memset(Buffer,'\0',100);
     OSuart_OutString(UART0_BASE, "\r\n"); 
   }
  }  

void printSound(void)
{
  char string[50];
  short event;
  OSuart_OutString(UART0_BASE, "\r\n");
  for(event = 0; event < 200; event++) 
  {
    Int2Str(SoundDump[event], string);
	OSuart_OutString(UART0_BASE,string);
	OSuart_OutString(UART0_BASE, "\r\n");
    SysCtlDelay(SysCtlClockGet()/1000);
  }
}

void printTime(void)
{
  char string[50];
  short event;
  for(event = 0; event < NUM_EVENTS; event++) 
  {
     sprintf(string, "\r\n%i. ", event);
	 OSuart_OutString(UART0_BASE, string);
     if(RunTimeProfile[event][1] == PER_THREAD_START)
     {
       sprintf(string,"Periodic Thread Started at ");
     }
        if(RunTimeProfile[event][1] == PER_THREAD_END)
        {
           sprintf(string,"Periodic Thread Ended at ");
        }        
        if(RunTimeProfile[event][1] == FOREGROUND_THREAD_START)
        {
           sprintf(string,"Foreground Thread Started at ");
        }
        OSuart_OutString(UART0_BASE, string);
        sprintf(string, "%i.", RunTimeProfile[event][0]);
		    OSuart_OutString(UART0_BASE, string);
        SysCtlDelay(SysCtlClockGet()/500);

   }
}

void Interpreter(void)
{
  unsigned char trigger;
  short fifo_status = 0;
  OSuart_Open();
  while(NumSamples<RUNLENGTH)
  {  
    GPIO_B3 ^= 0x08;  
    fifo_status = UARTRxFifo_Get(&trigger);
    if(fifo_status == 1)
    OSuart_Interpret(trigger);
	OS_Sleep(3);   //Give PID a chance to run
  }
  for(;;)
  {
    fifo_status = UARTRxFifo_Get(&trigger);
    if(fifo_status == 1)
    OSuart_Interpret(trigger);
  }
}      


//*****************************************************************************
//
// Load a string into the UART SW transmit FIFO for output to the console, then
// write the beginning of the string to the HW FIFO and enable TX interrupts.
// This functions outputs a string to the console through interrupts.
//
// \param ulBase specifies the UART port being used
// \param string is a pointer to a string to be output to the console
//
// \return none.
//
//*****************************************************************************
void 
OSuart_OutString(unsigned long ulBase, char *string)
{  
  int i = 0;
  unsigned char uartData;
  //
    // Check the arguments.
    //
    ASSERT(UARTBaseValid(ulBase));
  while(string[i])
  {
    if(!UARTTxFifo_Put(string[i]))
    {
      oLED_Message(0, 0, "UART TX", 0);
      oLED_Message(0, 1, "FIFO FULL", 0);
    }
     i++;
  }
  
  //
  // Disable the TX interrupt while loading the HW TX FIFO.
  //
  UARTIntDisable(ulBase, UART_INT_TX);

  //
  //  Load the initial segment of the string into the HW FIFO
  //
  while(UARTSpaceAvail(ulBase) && UARTTxFifo_Get(&uartData)) 
  {
    UARTCharPut(ulBase,uartData);
  }
     
  //
  //  Enable TX interrupts so that an interrupt will occur when
  //  the TX FIFO is nearly empty (interrupt level set in main program).
  //
  UARTIntEnable(ulBase, UART_INT_TX);
}




//*****************************************************************************
//
// UART_Open initializes the UART interface.
//
//*****************************************************************************
void
OSuart_Open(void)
{
  UARTRxFifo_Init();
 
  // Enable the peripherals used by this example.
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

  // Set GPIO A0 and A1 as UART pins.
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

  // Configure the UART for 115,200, 8-N-1 operation.
  UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));

  // Enable transmit and recieve FIFOs and set levels
  UARTFIFOEnable(UART0_BASE);
  UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);
    
  // Enable the UART interrupt.
  IntEnable(INT_UART0);
  UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT | UART_INT_TX);
  IntPrioritySet(INT_UART0,(((unsigned char)5)<<5)&0xF0);

}






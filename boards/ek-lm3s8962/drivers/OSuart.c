//*****************************************************************************
//
// Filename: uart_echo.c 
// Authors: Dustin Replogle, Katy Loeffler   
// Initial Creation Date: January 26, 2011 
// Description: Main function for Lab 01. Exercises the ADC and the interpreter
//   as well as a periodic interrupt.   	 
// Lab Number. 01    
// TA: Raffaele Cetrulo      
// Date of last revision: February 9, 2011 (for style modifications)      
// Hardware Configuration: default
//
// Modules used:
//		1. GPTimers:
//			a. GPTimer3 is used for periodic tasks (see OS_AddPeriodicThread)
//			b. GPTimer1 is used for sleeping threads.
//			c. GPTimer0 is used for ADC triggering (see ADC_Collect)
//      2. SysTick: The period of the SysTick is used to dictate the TIMESLICE
//		   for thread switching.  Systick handler causes thread switch.
//		3. ADC: all 4 channels may be accessed.
//		4. UART: UART0 is used for communication with a console.
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
#include "drivers/eFile.h"

// Global Variables
  AddFifo(UARTRx, 256, unsigned char, 1, 0);   // UARTRx Buffer
  AddFifo(UARTTx, 256, unsigned char, 1, 0);   // UARTTx Buffer

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
extern unsigned long DataLost;     // data sent by Producer, but not received by Consumer
extern unsigned long RunTimeProfile[NUM_EVENTS][2];
extern int EventIndex;

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>UART Echo (uart_echo)</h1>
//!
//! This example application utilizes the UART to echo text.  The first UART
//! (connected to the FTDI virtual serial port on the evaluation board) will be
//! configured in 115,200 baud, 8-n-1 mode.  All characters received on the
//! UART are transmitted back to the UART.
//
//*****************************************************************************

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
  const short numcommands = 10;
  unsigned char data;
  char * commands[numcommands] = {"NumSamples", "NumCreated", "DataLost","openSD", "format", "dir", "createfile", "writefile", "printfile", "deletefile"};
  char * descriptions[numcommands] = {" - Display NumSamples\r\n", " - Display NumCreated\r\n", " - Display DataLost\r\n", " - Open SD card\r\n", 
                                      " - Format entire disk\r\n", " - Print contents of directory\r\n", " - Create a new file\r\n",
									  " - Write to file\r\n", " - Print contents of file\r\n", " - Delete file\r\n"};
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
		SysCtlDelay(SysCtlClockGet()/1000);
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
		  OSuart_OutString(UART0_BASE, string);	      //"format", "dir", "printfile", "deletefile"
	   }
	 cmdptr++;
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //openSD
	   {
  		 if(eDisk_Init(0)) 			   diskError("eDisk_Init",0);	 
         if(eFile_Init())              diskError("eFile_Init",0); 	
	   }
	 cmdptr++;
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //format
	   {	 
         if(eFile_Format())            diskError("eFile_Format",0);
		 OSuart_OutString(UART0_BASE, "\r\nFormat Complete"); 	
	   }
     cmdptr++;
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //dir
	   {	 
         eFile_Directory();	
	   }
	 cmdptr++;
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //createfile
	   {
	     token = strtok_r(NULL , " ", &last);	 
         if(eFile_Create(token))     diskError("eFile_Create",0);	
	   }
	 cmdptr++;
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //writefile
	   {
	     OSuart_OutString(UART0_BASE, "\r\nType '#' to end redirection/close file\r\n\r\n");
	     token = strtok_r(NULL , " ", &last);
         if(eFile_RedirectToFile(token)) OSuart_OutString(UART0_BASE, "Redirect Error");
		 if(eFile_EndRedirectToFile()) OSuart_OutString(UART0_BASE, "End Redirect Error"); 
		 OSuart_OutString(UART0_BASE, "\r\nWrite Complete\r\n");	
	   }
     cmdptr++;
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //printfile
	   {
	   OSuart_OutString(UART0_BASE, "\r\n\r\n");	 
       token = strtok_r(NULL , " ", &last);
       eFile_ROpen(token);
	   for(event = 0; event < 25; event++){
         if(eFile_ReadNext(&data))   diskError("eFile_ReadNext",0);
         OSuart_OutChar(UART0_BASE, data);
	     SysCtlDelay(SysCtlClockGet()/10000);
       }
       eFile_ReadNext(token);
       eFile_RClose();	
	   }
     cmdptr++;
	   if(strcasecmp(token, commands[cmdptr]) == 0)        //deletefile
	   {	 
       token = strtok_r(NULL , " ", &last);
       if(eFile_Delete(token))     diskError("eFile_Delete",0);       	
	   }
     cmdptr++;

      
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

OSuart_printTime(void)
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
//      oLED_Message(0, 0, "UART TX", 0);
//      oLED_Message(0, 1, "FIFO FULL", 0);
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
OSuart_OutChar(unsigned long ulBase, char string)
{  
  unsigned char uartData;
  //
    // Check the arguments.
    //
  ASSERT(UARTBaseValid(ulBase));
  if(!UARTTxFifo_Put(string))
  {
//    oLED_Message(0, 0, "UART TX", 0);
//    oLED_Message(0, 1, "FIFO FULL", 0);
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






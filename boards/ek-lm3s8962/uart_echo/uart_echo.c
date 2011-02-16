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

// Global Variables
  AddFifo(UARTRx, 32, unsigned char, 1, 0);   // UARTRx Buffer
  AddFifo(UARTTx, 32, unsigned char, 1, 0);   // UARTTx Buffer
  unsigned char Buffer[100];
  unsigned int BufferPt = 0;
  unsigned short FirstSpace = 1;

  int IntTerm;    //For PID.s
  int PrevError;  //For PID.s

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
  OS_DebugProfileInit();

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
      UARTCharPutNonBlocking(UART0_BASE,uartData); //echo data

      if(!UARTRxFifo_Put(uartData))
      {
        oLED_Message(0, 0, "UART RX", 0);
        oLED_Message(0, 1, "FIFO FULL", 0);  
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
UARTOutString(unsigned long ulBase, char *string)
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
// Interpret input from the terminal. Supported functions include
// addition, subtraction, division, and multiplication in the post-fix format.
//
// \param nextChar specifies the next character to be put in the global buffer
//
// \return none.
//
//*****************************************************************************
void UARTInterpreter(unsigned char nextChar)
{
  char operator = Buffer[BufferPt - 2];
  char * token;
  char * last;
  char string[10];
  long total = 0;
  short first = 1;
  short command, equation = 0; 
  switch(nextChar)
  {
    case '\b':
     BufferPt--;
     break;
    case '=':
      FirstSpace = 1;
      Buffer[BufferPt] = '=';
     equation = 1;
     break;
    case '\r':
     command = 1;
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
        UARTOutString(UART0_BASE, string);
        UARTOutString(UART0_BASE, "\r\n");
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
        UARTOutString(UART0_BASE, string);
        UARTOutString(UART0_BASE, "\r\n");
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
        UARTOutString(UART0_BASE, string);
        UARTOutString(UART0_BASE, "\r\n");
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
        UARTOutString(UART0_BASE, string);
        UARTOutString(UART0_BASE, "\r\n");
        break;
      
      case 't':
        Int2Str(OS_MsTime(), string);
        UARTOutString(UART0_BASE, string);
        UARTOutString(UART0_BASE, "tcks \r\n");
        break;    
      
      default:
        break;
        
    }
    BufferPt = 0;
    equation = 0;
  }

  if(command = 1)
  {
	 // In the future, this will contain commands from the serial terminal
  }


}

//*****************************************************************************
//
// Dummy functions for testing OS_AddPeriodicThread routine.
//
//*****************************************************************************
void
dummy(void)
{
  while(1);
}

//*****************************************************************************
//
// This example demonstrates how to send a string of data to the UART.
//
//*****************************************************************************
int
main_orig(void)
{
  unsigned short adc_buffer[6];
  unsigned char trigger;
  unsigned short adc_SingleSample;
  short fifo_status = 0;
  //UARTRxFifo_Init();
    
  //
  // Set the clocking to run directly from the crystal.
  //
  SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                 SYSCTL_XTAL_8MHZ);

  /*
  //
  // Initialize the OLED display and write status.
  //
  RIT128x96x4Init(1000000);
  while(1)
  {
  oLED_Message(1, 2, "ADCSample3", adc_buffer[2]);
  }

  
  //
  // Initialize the OLED display and write status.
  //
  RIT128x96x4Init(1000000);
 
  //
  // Enable the peripherals used by this example.
  //
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

  //
  // Enable processor interrupts.
  //
  IntMasterEnable();

  //
  // Set GPIO A0 and A1 as UART pins.
  //
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

  //
  // Configure the UART for 115,200, 8-N-1 operation.
  //
  UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));
  //
  // Enable transmit and recieve FIFOs and set levels
  //
  UARTFIFOEnable(UART0_BASE);
  UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);
    
  //
  // Enable the UART interrupt.
  //
  IntEnable(INT_UART0);
  UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT | UART_INT_TX);

  //
  // Prompt for text to be entered.
  //
  
  UARTOutString(UART0_BASE, "\r\nEnter some text:");
    
  //
  // Loop forever echoing data through the UART.
  //
  oLED_Message(1, 4, "RT OS LAB:", 1);
  ADC_Open();
  OS_AddPeriodicThread(&dummy, 1, 1);

  while(1)
  {    
    fifo_status = UARTRxFifo_Get(&trigger);
    if(fifo_status == 1)
    UARTInterpreter(trigger);
    
    adc_SingleSample = ADC_In(0);
    oLED_Message(0, 4, "ADC_In(0)", (long)adc_SingleSample);
    ADC_Collect(0, 1000, adc_buffer, 3);
    oLED_Message(1, 0, "ADCSample1", adc_buffer[0]);
    oLED_Message(1, 1, "ADCSample2", adc_buffer[1]);
    oLED_Message(1, 2, "ADCSample3", adc_buffer[2]);  
  }
  */
}






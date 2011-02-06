//*****************************************************************************
//
// uart_echo.c - Example for reading data from and writing data to the UART in
//               an interrupt driven fashion.
//
// Copyright (c) 2005-2011 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 6852 of the EK-LM3S8962 Firmware Package.
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
	AddFifo(UARTRx, 32, unsigned char, 1, 0); 	// UARTRx buffer
   	AddFifo(UARTTx, 32, unsigned char, 1, 0); 	// UARTTx buffer
	unsigned char * buffer[100];
	unsigned int buffer_pointer = 0;
	unsigned int first_space = 1;
	long cnt1 = 0; //For dummy function, for testing OS_AddPeriodicThread

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
	unsigned char UARTData;

    //
    // Get the interrrupt status.
    //
    ulStatus = UARTIntStatus(UART0_BASE, true);
	//
    // Clear the asserted interrupts.
    //
    UARTIntClear(UART0_BASE, ulStatus);

	if(ulStatus == UART_INT_RX)
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
			UARTData =  (char)UARTCharGetNonBlocking(UART0_BASE);
			UARTCharPutNonBlocking(UART0_BASE,UARTData); //echo data

			if(!UARTRxFifo_Put(UARTData))
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
		while(UARTSpaceAvail(UART0_BASE) && UARTTxFifo_Get(&UARTData)) 
		{
			UARTCharPut(UART0_BASE,UARTData);
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
	unsigned char UARTData;
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
	//	Load the initial segment of the string into the HW FIFO
	//
	while(UARTSpaceAvail(ulBase) && UARTTxFifo_Get(&UARTData)) 
	{
		UARTCharPut(ulBase,UARTData);
	}
   	
	//
	//  Enable TX interrupts so that an interrupt will occur when
	//  the TX FIFO is nearly empty (interrupt level set in main program).
	//
	UARTIntEnable(ulBase, UART_INT_TX);
}

void UARTSolve(void)
{
	char operator = *buffer[buffer_pointer - 2];
	char * token;
	int total = 0;
	switch(operator)
	{
		case '+':
		for ( token = strtok(buffer, " "); token != "\0"; token = strtok(buffer, " ") )
			total = total + (int)token;
		UARTOutString(UART0_BASE, total + "\r\n");
		break;  


	}
	/*char * string[12];
	unsigned int i;
	unsigned int j;
	for(i = 0; i < buffer_pointer; i++)
	{
		j = 0;
		while(buffer[i] != " ")
		{
		   string[j] = buffer[i];
		}	
	}
	string = strchr(buffer, 1);*/	
}

//*****************************************************************************
//
// Interpret input from the terminal
//
//*****************************************************************************
void UARTInterpreter(unsigned char nextChar)
{
   
   switch(nextChar)
   {
	   case '\b':
	   	buffer_pointer--;
		break;
	   case '=':
	    first_space = 1;
		UARTSolve();
	    break;
	   default:
	    buffer[buffer_pointer] = &nextChar;
		buffer_pointer++;
	   	break;
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
 	oLED_Message(0, 0, "Periodic Cnt", cnt1);
	cnt1++;
}

//*****************************************************************************
//
// This example demonstrates how to send a string of data to the UART.
//
//*****************************************************************************
int
main(void)
{
	unsigned short ADC_buffer[6];
	unsigned char trigger;
	unsigned short ADC_SingleSample;
	UARTRxFifo_Init();
	  
    //
    // Set the clocking to run directly from the crystal.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_8MHZ);

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
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);

    //
    // Prompt for text to be entered.
    //
    //UARTSend((unsigned char *)"Enter text: ", 12);
	UARTOutString(UART0_BASE, "\r\nEnter some text:");
    
	//
    // Loop forever echoing data through the UART.
    //
	oLED_Message(1, 4, "RT OS LAB:", 1);
	ADC_Open();
	OS_AddPeriodicThread(&dummy, 100, 1);

    while(1)
    {  	
		UARTRxFifo_Get(&trigger);
		UARTInterpreter(trigger);
		ADC_SingleSample = ADC_In(0);
		oLED_Message(0, 4, "ADC_In(0)", (long)ADC_SingleSample);
		ADC_Collect(0, 1, ADC_buffer, 3);
		oLED_Message(1, 0, "ADCSample1", ADC_buffer[0]);
		oLED_Message(1, 1, "ADCSample2", ADC_buffer[1]);
		oLED_Message(1, 2, "ADCSample3", ADC_buffer[2]);	
    }
}

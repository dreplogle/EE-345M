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

// Global Variables
	AddFifo(UARTRx, 8, long, 1, 0); 	// UARTRx defined as global Fifo for testing


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
	long UARTData;

    //
    // Get the interrrupt status.
    //
    ulStatus = UARTIntStatus(UART0_BASE, true);

    //
    // Clear the asserted interrupts.
    //
    UARTIntClear(UART0_BASE, ulStatus);

    //
    // Loop while there are characters in the receive FIFO.
    //
    while(UARTCharsAvail(UART0_BASE))
    {
        //
        // Read the next character from the UART and write it back to the UART.
        //
		UARTData =  UARTCharGetNonBlocking(UART0_BASE);
		if(!UARTRxFifo_Put(UARTData)){
			while(UARTRxFifo_Get(&UARTData)){
        		UARTCharPutNonBlocking(UART0_BASE,UARTData);
			}
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
// This example demonstrates how to send a string of data to the UART.
//
//*****************************************************************************
int
main(void)
{
	unsigned short ADC_sample = 0;
	unsigned short ADC_buffer[6];
	UARTRxFifo_Init();

	  
    //
    // Set the clocking to run directly from the crystal.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_8MHZ);
	//SysCtlClockSet(SYSCTL_SYSDIV_10 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
    //               SYSCTL_XTAL_16MHZ);


    //
    // Initialize the OLED display and write status.
    //
    RIT128x96x4Init(1000000);
   // RIT128x96x4StringDraw("UART Echo",            36,  0, 15);
   // RIT128x96x4StringDraw("Port:   Uart 0",       12, 16, 15);
   // RIT128x96x4StringDraw("Baud:   115,200 bps",  12, 24, 15);
   // RIT128x96x4StringDraw("Data:   8 Bit",        12, 32, 15);
   // RIT128x96x4StringDraw("Parity: None",         12, 40, 15);
   // RIT128x96x4StringDraw("Stop:   1 Bit",        12, 48, 15);
   // oLED_Message(0, 0, "Line0", 7);
   //	oLED_Message(0, 1, "Line1", 12);
//	oLED_Message(0, 2, "Line2", 76);
//	oLED_Message(0, 3, "Line3", 87);
//	oLED_Message(0, 4, "Line4", 1113);
//	oLED_Message(1, 0, "Line0", 7);
//	oLED_Message(1, 1, "Line1", 345);
//	oLED_Message(1, 2, "Line2", 17);
//	oLED_Message(1, 3, "Line3", 89);
//	oLED_Message(1, 4, "Line4", 1638);


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
    // Enable the UART interrupt.
    //
    IntEnable(INT_UART0);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);

    //
    // Prompt for text to be entered.
    //
    UARTSend((unsigned char *)"Enter text: ", 12);

    //
    // Loop forever echoing data through the UART.
    //
	ADC_Open();
	ADC_Collect(0, 10, ADC_buffer, 6);
	oLED_Message(0, 0, "Sample 1", (long)ADC_buffer[0]);
	oLED_Message(0, 1, "Sample 2", (long)ADC_buffer[1]);
	oLED_Message(0, 2, "Sample 3", (long)ADC_buffer[2]);
	oLED_Message(0, 3, "Sample 4", (long)ADC_buffer[3]);
	oLED_Message(1, 0, "Sample 5", (long)ADC_buffer[4]);
	oLED_Message(1, 1, "Sample 6", (long)ADC_buffer[5]);
    while(1)
    {
		
		ADC_sample = ADC_In(0);
		oLED_Message(0, 0, "ADC Ch0", (long)ADC_sample);
//		ADC_sample = ADC_In(1);
//		oLED_Message(0, 2, "ADC Ch1", (long)ADC_sample);
//		ADC_sample = ADC_In(2);
//		oLED_Message(1, 0, "ADC Ch2", (long)ADC_sample);
//		ADC_sample = ADC_In(3);
//		oLED_Message(1, 2, "ADC Ch3", (long)ADC_sample);
		SysCtlDelay(SysCtlClockGet() / 20);
    }
}

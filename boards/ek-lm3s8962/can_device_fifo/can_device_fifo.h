//*****************************************************************************
//
// can_fifo.c - This application uses the CAN controller to communicate with
//              device board using the CAN controllers FIFO mode.
//
// Copyright (c) 2009-2011 Texas Instruments Incorporated.  All rights reserved.
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

#ifndef _CAN_FIFO
#define _CAN_FIFO

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_can.h"
#include "inc/hw_types.h"
#include "driverlib/can.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "drivers/rit128x96x4.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>CAN FIFO mode example (can_fifo)</h1>
//!
//! This application uses the CAN controller in FIFO mode to communicate with
//! the CAN device board.  The CAN device board must have the can_device_fifo
//! example program loaded and running prior to starting this application.
//! This program expects the CAN device board to echo back the data that it
//! receives and this application will then compare the data received with the
//! transmitted data and look for differences.  This application will then
//! modify the data and continue transmitting and receiving data over the CAN
//! bus indefinitely.
//!
//! \note This application must be started after the can_device_fifo example
//! has started on the CAN device board.
//
//*****************************************************************************

//
// Size of the FIFOs allocated to the CAN controller.
//
#define CAN_FIFO_SIZE           (8 * 8)

//
// Message object used by the transmit message FIFO.
//
#define TRANSMIT_MESSAGE_ID     8

//
// Message object used by the receive message FIFO.
//
#define RECEIVE_MESSAGE_ID      11

//
// The number of FIFO transfers that cause a toggle of the LED.
//
#define TOGGLE_RATE             100

//
// The CAN bit rate.
//
#define CAN_BITRATE             250000

//*****************************************************************************
//
// The CAN controller interrupt handler.
//
//*****************************************************************************
void
CANIntHandler(void);

//*****************************************************************************
//
// This function configures the transmit FIFO and copies data into the FIFO.
//
//*****************************************************************************
int
CANTransmitFIFO(unsigned char *pucData, unsigned long ulSize);

//*****************************************************************************
//
// This function configures the receive FIFO and should only be called once.
//
//*****************************************************************************
int
CANReceiveFIFO(unsigned char *pucData, unsigned long ulSize);

//*****************************************************************************
//
// This function just handles toggling the LED periodically during data
// transfer.
//
//*****************************************************************************
void
ToggleLED(void);

#endif

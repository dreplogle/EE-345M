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
// This structure holds all of the state information for the CAN transfers.
//
#include "can.h"
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

//
// Size of the FIFOs allocated to the CAN controller.
//
#define CAN_FIFO_SIZE           (8 * 8)



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

void CAN_Send(void);
void CAN_Receive(void);
//*****************************************************************************
//
// This is the main loop for the application.
//
//*****************************************************************************
void
CAN(void);
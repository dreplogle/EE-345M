//*****************************************************************************
//
// OS_UART.h contains functions used by the OS to communicate through
// the UART port.
//
//*****************************************************************************

void OSuart_Send(const unsigned char *pucBuffer, unsigned long ulCount);
void OSuart_OutString(unsigned long ulBase, char *string);
void OSuart_Interpreter(unsigned char nextChar);
void OSuart_Open(void);

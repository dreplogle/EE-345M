//*****************************************************************************
//
// ir.h - IR driver file
//
//*****************************************************************************

//*************GetIR***************
// Background thread for IR sensor,
// called when ADC finishes a conversion
// and generates an interrupt.
void GetIR(unsigned short data);

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 
#define IR_SAMPLING_RATE	20               // in Hz
struct IR_STATS{
  short average;
  short stdev;
  short maxdev;
};

void IRSensor0(void);
void IRSensor1(void);
void IRSensor2(void);
void IRSensor3(void);
//*****************************************************************************
//
// ir.c - IR driver file
//
//*****************************************************************************

#include "drivers/ir.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "drivers/OS.h"
#include "drivers/OSuart.h"
#include "driverlib/fifo.h"
#include "uart_echo/lab7.h"

AddFifo(RawIR_, 32, unsigned short, 1, 0);   // Raw IR data, ADC samples
AddFifo(IR_, 32, unsigned short, 1, 0);   // IR data after median filter

extern unsigned long NumCreated;   // number of foreground threads created
extern unsigned long NumSamples;   // incremented every sample
extern unsigned long DataLost;     // data sent by Producer, but not received by Consumer
extern unsigned long PIDWork;      // current number of PID calculations finished
extern unsigned long FilterWork;   // number of digital filter calculations finished
extern struct sensors Sensors;
//*************GetIR***************
// Background thread for IR sensor,
// called when ADC finishes a conversion
// and generates an interrupt.
void GetIR(unsigned short data){  
  if(RawIR_Fifo_Put(data)){     
    NumSamples++;
  } else{ 
    DataLost++;
  } 
}

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 

struct IR_STATS IR_Stats;
long stats[IR_SAMPLING_RATE];

void IRSensor(void){
  unsigned short i, ADCin;
  long sampleOut, data[3];
  static unsigned int newest = 0;
  long sum;
  unsigned short max,min;
  

  ADC_Collect(0, IR_SAMPLING_RATE, &GetIR); //ADC sample on channel 0, 20Hz
  
  for(;;){
	data[2] = data[1];
	data[1] = data[0];
    while(!RawIR_Fifo_Get(&ADCin));
	data[0] = ((long)ADCin*7836 - 166052)/1024; //((1/cm)*65535) = ((7836*x-166052)/1024
	data[0] = 65535/data[0];  //cm = 65535/data[0] from last operation

	  
    //3-Element median filter
    if((data[0]<=data[1]&&data[0]>=data[2])||(data[0]<=data[2]&&data[0]>=data[1])) sampleOut = data[0];
    else if((data[1]<=data[0]&&data[1]>=data[2])||(data[1]<=data[2]&&data[1]>=data[0])) sampleOut = data[1];
    else if((data[2]<=data[1]&&data[2]>=data[0])||(data[2]<=data[0]&&data[2]>=data[1])) sampleOut = data[2];  
    else sampleOut = 0xFF;       // Median finding error

	if(IR_Fifo_Put(sampleOut)){     
      NumSamples++;
    } else{ 
      DataLost++;
    }

	stats[newest] = sampleOut;
	newest++;
	if(newest >= IR_SAMPLING_RATE) newest = 0;

	//Average = sum of all values/number of values
	//Maximum deviation = max value - min value
	max = 0;
	min = 0xFFFF;
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum += stats[i];
	  if(stats[i] < min) min = stats[i];
	  if(stats[i] > max) max = stats[i];	  
	}
    IR_Stats.average = (sum/IR_SAMPLING_RATE);
	IR_Stats.maxdev = (max - min);

	//Standard deviation = sqrt(sum of (each value - average)^2 / number of values)
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum +=(stats[i]-IR_Stats.average)*(stats[i]-IR_Stats.average);	  
	}
	sum = sum/IR_SAMPLING_RATE;
	IR_Stats.stdev = sqrt(sum);
	Sensors.IR = IR_Stats.average;

	//oLED_Message(0, 0, "IR Avg", IR_Stats.average);
	//oLED_Message(0, 1, "IR StdDev", IR_Stats.stdev);
	//oLED_Message(0, 2, "IR MaxDev", IR_Stats.maxdev);
  }
}

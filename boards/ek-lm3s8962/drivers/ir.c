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

AddFifo(RawIR0_, 32, unsigned short, 1, 0);   // Raw IR data, ADC samples
AddFifo(IR0_, 32, unsigned short, 1, 0);   // IR data after median filter
AddFifo(RawIR1_, 32, unsigned short, 1, 0);   // Raw IR data, ADC samples
AddFifo(IR1_, 32, unsigned short, 1, 0);   // IR data after median filter
AddFifo(RawIR2_, 32, unsigned short, 1, 0);   // Raw IR data, ADC samples
AddFifo(IR2_, 32, unsigned short, 1, 0);   // IR data after median filter
AddFifo(RawIR3_, 32, unsigned short, 1, 0);   // Raw IR data, ADC samples
AddFifo(IR3_, 32, unsigned short, 1, 0);   // IR data after median filter

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
void GetIR0(unsigned short data){  
  if(RawIR0_Fifo_Put(data)){     
    NumSamples++;
  } else{ 
    DataLost++;
  } 
}

//*************GetIR***************
// Background thread for IR sensor,
// called when ADC finishes a conversion
// and generates an interrupt.
void GetIR1(unsigned short data){  
  if(RawIR1_Fifo_Put(data)){     
    NumSamples++;
  } else{ 
    DataLost++;
  } 
}
//*************GetIR***************
// Background thread for IR sensor,
// called when ADC finishes a conversion
// and generates an interrupt.
void GetIR2(unsigned short data){  
  if(RawIR2_Fifo_Put(data)){     
    NumSamples++;
  } else{ 
    DataLost++;
  } 
}
//*************GetIR***************
// Background thread for IR sensor,
// called when ADC finishes a conversion
// and generates an interrupt.
void GetIR3(unsigned short data){  
  if(RawIR3_Fifo_Put(data)){     
    NumSamples++;
  } else{ 
    DataLost++;
  } 
}

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 

struct IR_STATS IR_Stats0;
long stats0[IR_SAMPLING_RATE];

void IRSensor0(void){
  unsigned short i, ADCin;
  long sampleOut, data[3];
  static unsigned int newest = 0;
  long sum;
  unsigned short max,min;
  

  ADC_Collect_All(IR_SAMPLING_RATE, &GetIR0, &GetIR1, &GetIR2, &GetIR3); //ADC sample on channel 0, 20Hz
  
  for(;;){
	data[2] = data[1];
	data[1] = data[0];
    while(!RawIR0_Fifo_Get(&ADCin));
	if(ADCin < 22){ADCin = 22;}
	data[0] = ((long)ADCin*7836 - 166052)/1024; //((1/cm)*65535) = ((7836*x-166052)/1024
	data[0] = 65535/data[0];  //cm = 65535/data[0] from last operation

	  
    //3-Element median filter
    if((data[0]<=data[1]&&data[0]>=data[2])||(data[0]<=data[2]&&data[0]>=data[1])) sampleOut = data[0];
    else if((data[1]<=data[0]&&data[1]>=data[2])||(data[1]<=data[2]&&data[1]>=data[0])) sampleOut = data[1];
    else if((data[2]<=data[1]&&data[2]>=data[0])||(data[2]<=data[0]&&data[2]>=data[1])) sampleOut = data[2];  
    else sampleOut = 0xFF;       // Median finding error

	if(IR0_Fifo_Put(sampleOut)){     
      NumSamples++;
    } else{ 
      DataLost++;
    }

	stats0[newest] = sampleOut;
	newest++;
	if(newest >= IR_SAMPLING_RATE) newest = 0;

	//Average = sum of all values/number of values
	//Maximum deviation = max value - min value
	max = 0;
	min = 0xFFFF;
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum += stats0[i];
	  if(stats0[i] < min) min = stats0[i];
	  if(stats0[i] > max) max = stats0[i];	  
	}
    IR_Stats0.average = (sum/IR_SAMPLING_RATE);
	IR_Stats0.maxdev = (max - min);

	//Standard deviation = sqrt(sum of (each value - average)^2 / number of values)
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum +=(stats0[i]-IR_Stats0.average)*(stats0[i]-IR_Stats0.average);	  
	}
	sum = sum/IR_SAMPLING_RATE;
	IR_Stats0.stdev = sqrt(sum);
	
	if(sampleOut > 80){sampleOut = 80;}
	if(sampleOut < 10){sampleOut = 10;}
	Sensors.ir_back_right = sampleOut;



	//oLED_Message(0, 0, "IR Avg", IR_Stats0.average);
	//oLED_Message(0, 1, "IR StdDev", IR_Stats0.stdev);
	//oLED_Message(0, 2, "IR MaxDev", IR_Stats0.maxdev);
  }
}

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 

struct IR_STATS IR_Stats1;
long stats1[IR_SAMPLING_RATE];

void IRSensor1(void){
  unsigned short i, ADCin;
  long sampleOut, data[3];
  static unsigned int newest = 0;
  long sum;
  unsigned short max,min;
  

 // ADC_Collect(1, IR_SAMPLING_RATE, &GetIR1); //ADC sample on channel 0, 20Hz
  
  for(;;){
	data[2] = data[1];
	data[1] = data[0];
    while(!RawIR1_Fifo_Get(&ADCin));
	if(ADCin < 22){ADCin = 22;}
	data[0] = ((long)ADCin*7836 - 166052)/1024; //((1/cm)*65535) = ((7836*x-166052)/1024
	data[0] = 65535/data[0];  //cm = 65535/data[0] from last operation

	  
    //3-Element median filter
    if((data[0]<=data[1]&&data[0]>=data[2])||(data[0]<=data[2]&&data[0]>=data[1])) sampleOut = data[0];
    else if((data[1]<=data[0]&&data[1]>=data[2])||(data[1]<=data[2]&&data[1]>=data[0])) sampleOut = data[1];
    else if((data[2]<=data[1]&&data[2]>=data[0])||(data[2]<=data[0]&&data[2]>=data[1])) sampleOut = data[2];  
    else sampleOut = 0xFF;       // Median finding error

	if(IR1_Fifo_Put(sampleOut)){     
      NumSamples++;
    } else{ 
      DataLost++;
    }

	stats1[newest] = sampleOut;
	newest++;
	if(newest >= IR_SAMPLING_RATE) newest = 0;

	//Average = sum of all values/number of values
	//Maximum deviation = max value - min value
	max = 0;
	min = 0xFFFF;
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum += stats1[i];
	  if(stats1[i] < min) min = stats1[i];
	  if(stats1[i] > max) max = stats1[i];	  
	}
    IR_Stats1.average = (sum/IR_SAMPLING_RATE);
	IR_Stats1.maxdev = (max - min);

	//Standard deviation = sqrt(sum of (each value - average)^2 / number of values)
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum +=(stats1[i]-IR_Stats1.average)*(stats1[i]-IR_Stats1.average);	  
	}
	sum = sum/IR_SAMPLING_RATE;
	IR_Stats1.stdev = sqrt(sum);

	if(sampleOut > 80){sampleOut = 80;}
	if(sampleOut < 10){sampleOut = 10;}

	Sensors.ir_side_right = sampleOut;


	//oLED_Message(0, 0, "IR Avg", IR_Stats1.average);
	//oLED_Message(0, 1, "IR StdDev", IR_Stats1.stdev);
	//oLED_Message(0, 2, "IR MaxDev", IR_Stats1.maxdev);
  }
}

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 

struct IR_STATS IR_Stats2;
long stats2[IR_SAMPLING_RATE];

void IRSensor2(void){
  unsigned short i, ADCin;
  long sampleOut, data[3];
  static unsigned int newest = 0;
  long sum;
  unsigned short max,min;
  

 // ADC_Collect(2, IR_SAMPLING_RATE, &GetIR2); //ADC sample on channel 0, 20Hz
  
  for(;;){
	data[2] = data[1];
	data[1] = data[0];
    while(!RawIR2_Fifo_Get(&ADCin));
	if(ADCin < 22){ADCin = 22;}
	data[0] = ((long)ADCin*7836 - 166052)/1024; //((1/cm)*65535) = ((7836*x-166052)/1024
	data[0] = 65535/data[0];  //cm = 65535/data[0] from last operation

	  
    //3-Element median filter
    if((data[0]<=data[1]&&data[0]>=data[2])||(data[0]<=data[2]&&data[0]>=data[1])) sampleOut = data[0];
    else if((data[1]<=data[0]&&data[1]>=data[2])||(data[1]<=data[2]&&data[1]>=data[0])) sampleOut = data[1];
    else if((data[2]<=data[1]&&data[2]>=data[0])||(data[2]<=data[0]&&data[2]>=data[1])) sampleOut = data[2];  
    else sampleOut = 0xFF;       // Median finding error

	if(IR2_Fifo_Put(sampleOut)){     
      NumSamples++;
    } else{ 
      DataLost++;
    }

	stats2[newest] = sampleOut;
	newest++;
	if(newest >= IR_SAMPLING_RATE) newest = 0;

	//Average = sum of all values/number of values
	//Maximum deviation = max value - min value
	max = 0;
	min = 0xFFFF;
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum += stats2[i];
	  if(stats2[i] < min) min = stats2[i];
	  if(stats2[i] > max) max = stats2[i];	  
	}
    IR_Stats2.average = (sum/IR_SAMPLING_RATE);
	IR_Stats2.maxdev = (max - min);

	//Standard deviation = sqrt(sum of (each value - average)^2 / number of values)
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum +=(stats2[i]-IR_Stats2.average)*(stats2[i]-IR_Stats2.average);	  
	}
	sum = sum/IR_SAMPLING_RATE;
	IR_Stats2.stdev = sqrt(sum);

	if(sampleOut > 80){sampleOut = 80;}
	if(sampleOut < 10){sampleOut = 10;}

	Sensors.ir_side_left = sampleOut;

	//oLED_Message(0, 0, "IR Avg", IR_Stats2.average);
	//oLED_Message(0, 1, "IR StdDev", IR_Stats2.stdev);
	//oLED_Message(0, 2, "IR MaxDev", IR_Stats2.maxdev);
  }
}

//************IR DAQ thread********
// Samples the ADC0 at 20Hz, recieves
// data in FIFO, filters data,
// sends data through CAN. 

struct IR_STATS IR_Stats3;
long stats3[IR_SAMPLING_RATE];

void IRSensor3(void){
  unsigned short i, ADCin;
  long sampleOut, data[3];
  static unsigned int newest = 0;
  long sum;
  unsigned short max,min;
  

 // ADC_Collect(3, IR_SAMPLING_RATE, &GetIR3); //ADC sample on channel 0, 20Hz
  
  for(;;){
	data[2] = data[1];
	data[1] = data[0];
    while(!RawIR3_Fifo_Get(&ADCin));
	if(ADCin < 22){ADCin = 22;}
	data[0] = ((long)ADCin*7836 - 166052)/1024; //((1/cm)*65535) = ((7836*x-166052)/1024
	data[0] = 65535/data[0];  //cm = 65535/data[0] from last operation

	  
    //3-Element median filter
    if((data[0]<=data[1]&&data[0]>=data[2])||(data[0]<=data[2]&&data[0]>=data[1])) sampleOut = data[0];
    else if((data[1]<=data[0]&&data[1]>=data[2])||(data[1]<=data[2]&&data[1]>=data[0])) sampleOut = data[1];
    else if((data[2]<=data[1]&&data[2]>=data[0])||(data[2]<=data[0]&&data[2]>=data[1])) sampleOut = data[2];  
    else sampleOut = 0xFF;       // Median finding error

	if(IR3_Fifo_Put(sampleOut)){     
      NumSamples++;
    } else{ 
      DataLost++;
    }

	stats3[newest] = sampleOut;
	newest++;
	if(newest >= IR_SAMPLING_RATE) newest = 0;

	//Average = sum of all values/number of values
	//Maximum deviation = max value - min value
	max = 0;
	min = 0xFFFF;
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum += stats3[i];
	  if(stats3[i] < min) min = stats3[i];
	  if(stats3[i] > max) max = stats3[i];	  
	}
    IR_Stats3.average = (sum/IR_SAMPLING_RATE);
	IR_Stats3.maxdev = (max - min);

	//Standard deviation = sqrt(sum of (each value - average)^2 / number of values)
	sum = 0;
	for(i = 0; i < IR_SAMPLING_RATE; i++){
      sum +=(stats3[i]-IR_Stats3.average)*(stats3[i]-IR_Stats3.average);	  
	}
	sum = sum/IR_SAMPLING_RATE;
	IR_Stats3.stdev = sqrt(sum);

	if(sampleOut > 80){sampleOut = 80;}
	if(sampleOut < 10){sampleOut = 10;}

	Sensors.ir_back_left = sampleOut;

	//oLED_Message(0, 0, "IR Avg", IR_Stats3.average);
	//oLED_Message(0, 1, "IR StdDev", IR_Stats3.stdev);
	//oLED_Message(0, 2, "IR MaxDev", IR_Stats3.maxdev);
  }
}

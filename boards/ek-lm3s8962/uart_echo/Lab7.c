//*****************************************************************************
//
// Lab7.c - user programs, File system, stream data onto disk
// Jonathan Valvano, March 16, 2011, EE345M
//     You may implement Lab 5 without the oLED display
//*****************************************************************************
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch
 
#include <stdio.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/systick.h"
#include "driverlib/fifo.h"
#include "driverlib/adc.h"
#include "drivers/OS.h"
#include "drivers/OSuart.h"
#include "drivers/rit128x96x4.h"
#include "lm3s8962.h"
#include "math.h"
#include "drivers/can_fifo.h"
#include "drivers/tachometer.h"
#include "drivers/ir.h"
#include "uart_echo/lab7.h"
#include "drivers/servo.h"

unsigned long NumCreated;   // number of foreground threads created
unsigned long NumSamples;   // incremented every sample
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished
#define MAX_SPEED 20
#define MIN_SPEED 0

extern unsigned long JitterHistogramA[];
extern unsigned long JitterHistogramB[];
extern unsigned long DebugAngle;
extern long MaxJitterA;
extern long MinJitterA;

unsigned short SoundVFreq = 1;
unsigned short SoundVTime = 0;
unsigned short FilterOn = 1;
struct sensors Sensors;
short SpeedLeft, SpeedRight = MAX_SPEED;

unsigned char motorBuffer[CAN_FIFO_SIZE];
int Running;                // true while robot is running

#define GPIO_PF0  (*((volatile unsigned long *)0x40025004))
#define GPIO_PF1  (*((volatile unsigned long *)0x40025008))
#define GPIO_PF2  (*((volatile unsigned long *)0x40025010))
#define GPIO_PF3  (*((volatile unsigned long *)0x40025020))
#define GPIO_PG1  (*((volatile unsigned long *)0x40026008))

// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
// PF0/PWM0 is debugging output on Systick
// PF2/LED1 is debugging output 
// PF3/LED0 is debugging output 
// PG1/PWM1 is debugging output


//******** Producer *************** 
// The Producer in this lab will be called from your ADC ISR
// A timer runs at 1 kHz, started by your ADC_Collect
// The timer triggers the ADC, creating the 1 kHz sampling
// Your ADC ISR runs when ADC data is ready
// Your ADC ISR calls this function with a 10-bit sample 
// sends data to the Robot, runs periodically at 1 kHz
// inputs:  none
// outputs: none
void Producer(unsigned short data){  
  if(Running){
    if(OS_Fifo_Put(data)){     // send to Robot
      NumSamples++;
    } else{ 
      DataLost++;
    } 
  }
}
 
//******** IdleTask  *************** 
// foreground thread, runs when no other work needed
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
unsigned long Idlecount=0;
void IdleTask(void){
  while(1) { 
    Idlecount++;        // debugging 
  }
}

//******** Interpreter **************
// your intepreter from Lab 4 
// foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
extern void Interpreter(void); 

//************ButtonPush*************
// Called when Select Button pushed
// background threads execute once and return
void ButtonPush(void){
  int i;
  unsigned char data[CAN_FIFO_SIZE];
  for(i=0; i<CAN_FIFO_SIZE; i++)
  {
    data[i] = i;
  }
  
  CAN_Send(data);
}
//************DownPush*************
// Called when Down Button pushed
// background threads execute once and return
void DownPush(void){
  CAN_Receive();
}

void Display(void){
	while(1){
  	oLED_Message(0, 0, "IR Front Left: ", Sensors.ir_front_left);
	oLED_Message(0, 1, "IR Side Left: ", Sensors.ir_side_left);
    oLED_Message(1, 0, "DebugAngle: ", DebugAngle);

	}
}

#define WALL_DIST   20 //cm

#define FRONT_LEFT_DIST 30
#define SIDE_LEFT_DIST 23

void CatBot(void){
  unsigned long i;
  SpeedLeft = 20;
  SpeedRight = 20;
  while(1){

	
   	//Transmit by CAN
	//	if(Sensors.ir_front_left <= 10 || Sensors.ir_back_left <= 10 && Sensors.ir_front_right > 10 && Sensors.ir_back_right > 10)
    //{
    //  SpeedLeft--;
    //}
	//	if(Sensors.ir_front_right <= 10 || Sensors.ir_back_right <= 10 && Sensors.ir_front_left > 10 && Sensors.ir_back_left > 10)
    //{
    //  SpeedRight--;
    //}
    //if(Sensors.ir_front_left > 30 && Sensors.ir_back_left > 30 && Sensors.ir_front_right > 30 && Sensors.ir_back_right > 30 &&
    //  Sensors.ir_front_left < 60 && Sensors.ir_back_left < 60 && Sensors.ir_front_right < 60 && Sensors.ir_back_right < 60)
    //{
    //  SpeedLeft, SpeedRight = MAX_SPEED;
    //}
    //SpeedLeft = 20; SpeedRight = 20;
    //if((long)Sensors.ir_front_left - (long)Sensors.ir_back_left > 5) { SpeedLeft = 19;}
	//else if((long)Sensors.ir_front_left - (long)Sensors.ir_back_left < -5) { SpeedRight = 19;}
	//else if((long)Sensors.ir_front_left - (long)Sensors.ir_back_left > 10) { SpeedLeft = 17;}
	//else if((long)Sensors.ir_front_left - (long)Sensors.ir_back_left < -10) { SpeedRight = 17;}
	//else if((long)Sensors.ir_front_left - (long)Sensors.ir_back_left > 20) { SpeedLeft = 12;}
	//else if((long)Sensors.ir_front_left - (long)Sensors.ir_back_left < -20) { SpeedRight = 12;} 
	
	//NOTE: Possibly change the distance with ping. This is really close.
	//NOTE: WALL_DIST*10 because ping is in terms of mm, where WALL_DIST 
	//	is in terms of cm
	//PING STRATEGY: If the ping recognizes Catbot to be too close to
	//	a wall
//	if(Sensors.ping < WALL_DIST*10)
//	{
//		if(Sensors.ir_side_right > WALL_DIST){SpeedLeft=10; SpeedRight=5;}
//		else if(Sensors.ir_side_left > WALL_DIST){SpeedRight=10; SpeedLeft=5;}
//
//	}
//	else
//	{
//		if(Sensors.ir_front_right < Sensors.ir_back_right){Servo_SetAngle(SERVO_MEDIUM_LEFT)}
//		else if (Sensors.ir_front_right > Sensors.ir_back_right) {Servo_SetAngle(SERVO_MEDIUM_RIGHT)}
		//if (Sensors.ir_front_left < Sensors.ir_side_left) {Servo_SetAngle(SERVO_FINE_RIGHT);}
		//else if (Sensors.ir_front_left > Sensors.ir_side_left) {Servo_SetAngle(SERVO_FINE_LEFT);}
		//else {Servo_SetAngle(SERVO_STRAIGHT);}
		if ((Sensors.ir_front_left < FRONT_LEFT_DIST) ) {Servo_SetAngle(SERVO_MEDIUM_RIGHT);}
		else if ( (Sensors.ir_front_left > FRONT_LEFT_DIST) ) {Servo_SetAngle(SERVO_MEDIUM_LEFT);}
		else if ( (Sensors.ir_front_left == FRONT_LEFT_DIST)	&& (Sensors.ir_side_left > SIDE_LEFT_DIST) ) {Servo_SetAngle(SERVO_MEDIUM_RIGHT);}
		else if ( (Sensors.ir_front_left == FRONT_LEFT_DIST)	&& (Sensors.ir_side_left < SIDE_LEFT_DIST) ) {Servo_SetAngle(SERVO_MEDIUM_LEFT);}
		else {Servo_SetAngle(SERVO_STRAIGHT);}
//	}


   
    motorBuffer[0] = 'A';
    motorBuffer[1] = SpeedLeft;
    motorBuffer[2] = SpeedRight;
	CAN_Send(motorBuffer);
	SysCtlDelay(SysCtlClockGet()/1000);

//	Servo_SetAngle(SERVO_SHARP_LEFT);
//    SysCtlDelay(SysCtlClockGet()/50);
//	Servo_SetAngle(SERVO_STRAIGHT);
//	SysCtlDelay(SysCtlClockGet()/50);
//    Servo_SetAngle(SERVO_SHARP_RIGHT);
//	SysCtlDelay(SysCtlClockGet()/50);
   


  }
}

unsigned char OnOffFlag;
unsigned long PWMduty;
void Fake_PWM(void){
  unsigned long totDutyPeriod = 21739;  //for 50Mhz and 45 timer prescale

  if(OnOffFlag){
    // Toggle bit high
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0x4);
	// Load pediod with duty cycle
	TimerLoadSet(TIMER3_BASE, TIMER_A, PWMduty);
	OnOffFlag = 0;
  }
  else{
	// Toggle bit low  
	GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);
	// Load period with total PWM period - duty cycle
	TimerLoadSet(TIMER3_BASE, TIMER_A, totDutyPeriod - PWMduty);
	OnOffFlag = 1; 
  }  
}
//*******************lab 6 main **********
int main(void){       
  OS_Init();           // initialize, disable interrupts
  Running = 0;         // robot not running
  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;

//********initialize communication channels
  OS_Fifo_Init(512);    // ***note*** 4 is not big enough*****

//*******attach background tasks***********
  OS_AddButtonTask(&ButtonPush,2);
  OS_AddDownTask(&DownPush,3);

  OS_BumperInit();
  CAN_Init();
  Servo_Init();
  OS_AddPeriodicThread(&Fake_PWM, 100, 1);

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&CAN,128,2); 
  NumCreated += OS_AddThread(&IRSensor0,128,2);  // runs when nothing useful to do
  NumCreated += OS_AddThread(&IRSensor1,128,2);  // runs when nothing useful to do
  NumCreated += OS_AddThread(&IRSensor2,128,2);  // runs when nothing useful to do
  NumCreated += OS_AddThread(&IRSensor3,128,2);  // runs when nothing useful to do
  NumCreated += OS_AddThread(&CatBot,128,2);
  NumCreated += OS_AddThread(&Display,128,2);
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
 
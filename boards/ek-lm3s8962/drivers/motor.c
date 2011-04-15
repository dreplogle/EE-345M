//*****************************************************************************
//
// Motor.c
//
//*****************************************************************************
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "gpio.h"
#include "inc/hw_sysctl.h"
#include "sysctl.h"
#include "pwm.h"




//The following code is copied from Professor Valvano's lecture 18 Program 6.15
#define PWM_ENABLE_R (*((volatile unsigned long *)0x40028008))
#define PWM_ENABLE_PWM0EN 0x00000001 // PWM0 Output Enable
#define PWM_0_CTL_R (*((volatile unsigned long *)0x40028040))
#define PWM_X_CTL_ENABLE 0x00000001 // PWM Block Enable
#define PWM_0_LOAD_R (*((volatile unsigned long *)0x40028050))
#define PWM_0_CMPA_R (*((volatile unsigned long *)0x40028058))
#define PWM_0_CMPB_R (*((volatile unsigned long *)0x4002805C))
#define PWM_0_GENA_R (*((volatile unsigned long *)0x40028060))
#define PWM_0_GENB_R (*((volatile unsigned long *)0x40028064))
//#define PWM_X_GENA_ACTCMPAD_ONE 0x000000C0 // Set the output signal to 1
//#define PWM_X_GENA_ACTLOAD_ZERO 0x00000008 // Set the output signal to 0
#define GPIOF_DEN_R (*((volatile unsigned long *)0x4002551C))

#define PWM_GENA_VALUE 0x8C
#define PWM_GENB_VALUE 0x80C
#define GPIO_PORTF_AFSEL_R (*((volatile unsigned long *)0x40025420))
#define SYSCTL_RCC_R (*((volatile unsigned long *)0x400FE060))
#define SYSCTL_RCC_USEPWMDIV 0x00100000 // Enable PWM Clock Divisor
#define SYSCTL_RCC_PWMDIV_M 0x000E0000 // PWM Unit Clock Divisor
#define SYSCTL_RCC_PWMDIV_2 0x00000000 // /2
#define SYSCTL_RCGC0_R (*((volatile unsigned long *)0x400FE100))
#define SYSCTL_RCGC0_PWM 0x00100000 // PWM Clock Gating Control
#define SYSCTL_RCGC2_R (*((volatile unsigned long *)0x400FE108))

#define PWM_ENABLE_PWM1EN 0x2
#define MOTOR_FORWARD 0
#define MOTOR_BACKWARD 1
#define LEFT_MOTOR 0
#define RIGHT_MOTOR 1
#define PIN_0_WRITE 0x1
#define PIN_1_WRITE 0x2





//*****************************************************************************
//
// Motor should be pulling both wheels back at half speed
//
//*****************************************************************************
void MotorBackward(void)
{

}

//*****************************************************************************
//
// Motor should be pushing both wheels at full speed ahead
//
//*****************************************************************************
void MotorForward(void)
{

}

//*****************************************************************************
//
// Left wheel goes half (maybe more, maybe less) speed, right wheel goes full speed
//
//*****************************************************************************
void MotorTurnLeft(void)
{

}

//*****************************************************************************
//
// Right wheel goes half (maybe more, maybe less) speed, left wheel goes full speed
//
//*****************************************************************************
void MotorTurnRight(void)
{

}

//*****************************************************************************
//
// Left wheel stops, right wheel at full speed
//
//*****************************************************************************
void MotorTurnHardLeft(void)
{

}

//*****************************************************************************
//
// Right wheel stops, left wheel at full speed
//
//*****************************************************************************
void MotorTurnHardRight(void)
{

}

//*****************************************************************************
//
// Right wheel back at full speed, left wheel back at fraction of full speed
//
//*****************************************************************************
void MotorTurnBackLeft(void)
{

}

//*****************************************************************************
//
// Left wheel back at full speed, right wheel back at fraction of full speed
//
//*****************************************************************************
void MotorTurnBackRight(void)
{

}




void MotorInit(void)
{
	volatile unsigned long delay = 0;

	//Initialize PE0 and PE1 to be outputs
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	delay = 1;
	GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_DIR_MODE_OUT);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
	GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_DIR_MODE_OUT);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);



	//The following code is copied from Professor Valvano's lecture 18 Program 6.15
	SYSCTL_RCGC0_R |= SYSCTL_RCGC0_PWM; // activate PWM
	SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
	delay = SYSCTL_RCGC2_R; // allow time to finish activating

	GPIOF_DEN_R |= 0x3; //enable digital output on PF0 and PF1
	GPIO_PORTF_AFSEL_R |= 0x01; // enable alt funct on PF0
	GPIO_PORTF_AFSEL_R |= 0x02; // enable alt funct on PF1


	SYSCTL_RCC_R |= SYSCTL_RCC_USEPWMDIV; // use PWM divider
	SYSCTL_RCC_R &= ~SYSCTL_RCC_PWMDIV_M; // clear PWM divider field
	SYSCTL_RCC_R += SYSCTL_RCC_PWMDIV_2; // configure for /2 divider

	PWM_0_CTL_R = 0; // countdown mode
	// 1: match compare value counting down
	// 0: re-loading
	PWM_0_GENA_R = PWM_GENA_VALUE;
	PWM_0_GENB_R = PWM_GENB_VALUE;
}




//The following code is copied from Professor Valvano's lecture 18 Program 6.15
// period is number of PWM clock cycles in one period (3<=period)
// duty is number of PWM clock cycles output is high (2<=duty<=period-1)
// PWM clock rate = processor clock rate/SYSCTL_RCC_PWMDIV
// = 6 MHz/2 = 3 MHz (in this example)

//duty = 2000 is very slow, duty = 9900 is very fast
void LeftMotorConfigure(unsigned short period, unsigned short duty){
	PWM_0_LOAD_R = period - 1; // cycles needed to count down to 0
	PWM_0_CMPA_R = duty - 1; // count value when output rises
	PWM_0_CTL_R |= PWM_X_CTL_ENABLE; // start PWM0
}



void RightMotorConfigure(unsigned short period, unsigned short duty){
	PWM_0_LOAD_R = period - 1; // cycles needed to count down to 0
	PWM_0_CMPB_R = duty - 1; // count value when output rises
	PWM_0_CTL_R |= PWM_X_CTL_ENABLE; // start PWM0
}

void setDutyCycle(unsigned char motor, unsigned short duty)
{
	if (motor == LEFT_MOTOR)
	{
		PWM_0_CMPA_R = duty - 1; // count value when output rises
	}
	else
	{
		PWM_0_CMPB_R = duty - 1; // count value when output rises
	}
}

unsigned char DebugDirection;

void setMotorDirection(unsigned char motor, unsigned char direction)
{
	if (motor == LEFT_MOTOR)
	{
		//These two lines must be done separately, or the shifting won't work properly
		direction = direction << PIN_0_WRITE;
		direction = direction >> 1;

		GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, direction);		
	}
	else
	{
		//These two lines must be done separately, or the shifting won't work properly
		DebugDirection = direction;
		direction = direction << PIN_1_WRITE;
		DebugDirection = direction;
		direction = direction >> 1;
		DebugDirection = direction;

		GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1, direction);	
	}

}

void LeftMotorStart(void)
{
	PWM_ENABLE_R |= PWM_ENABLE_PWM0EN; // enable PWM0
}

void RightMotorStart(void)
{
	 PWM_ENABLE_R |= PWM_ENABLE_PWM1EN; // enable PWM1
}

void LeftMotorStop(void)
{
	PWM_ENABLE_R &= ~(PWM_ENABLE_PWM0EN); // disable PWM0
} 

void RightMotorStop(void)
{
	PWM_ENABLE_R &= ~(PWM_ENABLE_PWM1EN); // disable PWM1
} 

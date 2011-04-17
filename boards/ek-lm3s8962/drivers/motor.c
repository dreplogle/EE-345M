//*****************************************************************************
//
// Motor.c
//
//*****************************************************************************
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "gpio.h"
#include "inc/hw_sysctl.h"
#include "driverlib/sysctl.h"
#include "driverlib/pwm.h"
#include "motor.h"

long Motor_DesiredSpeeds[2] = {0, };


static
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

static
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

// ********** Motor_GoForward ***********
// Sets direction and duty cycle of specified wheel
//   to go in the forward direction.
// Inputs:
//   motor_id - left or right motor
//   duty_cycle - duty cycle
// Outputs: none
static
void motorForward(unsigned char motor_id, unsigned char duty_cycle){
	setMotorDirection(motor_id, MOTOR_FORWARD);
	setDutyCycle(motor_id, duty_cycle);	
}

// ********** Motor_GoForward ***********
// Sets direction and duty cycle of specified wheel
//   to go in the backward direction.
// Inputs:
//   motor_id - left or right motor
//   duty_cycle - duty cycle
// Outputs: none
static
void motorBackward(unsigned char motor_id, unsigned char duty_cycle){
	setMotorDirection(motor_id, MOTOR_BACKWARD);
	setDutyCycle(motor_id, duty_cycle);
}


// ********** Motor_GoForward ***********
// Calculates and sets desired duty cycle to
//   move specified motor at desired rate.
// Inputs:
//   motor_id - left or right motor
//   speed - estimated speed from tachometer(in RPM)
// Outputs: none
// ** Code is based on Professor Valvano's lecture 20
void Motor_PID(unsigned char motor_id, unsigned long speed){
	long Error = 0, Up = 0, Ui = 0, U = 0;
	if (Motor_DesiredSpeeds[motor_id]){
		if (Motor_DesiredSpeeds[motor_id] > 0){
			Error = Motor_DesiredSpeeds[motor_id]-speed; // 0.1 RPM
		}
		else {
			Error = -Motor_DesiredSpeeds[motor_id]-speed; // 0.1 RPM backward
		}
		Up = (Kp*Error)/10000;
		Ui = Ui+(Ki*Error)/10000;
		if (Ui < 0)
			Ui = 0; // antireset windup
		if (Ui > 250)
			Ui = 250;
		U = Up+Ui;
		if (U < 0)
			U = 0; // minimum power
		if (U > 250)
			U = 250; // maximum power
	}
	else {
		Ui = U = 0; // Desired is 0
	}
	if (Motor_DesiredSpeeds[motor_id] > 0){
		motorForward(motor_id,(unsigned char)U);
	}
	else {
		motorBackward(motor_id,(unsigned char)U);
	}
}

//*****************************************************************************
//
// Motor should be pulling both wheels back at half speed
//
//*****************************************************************************
void Motor_GoBackward(void)
{
	Motor_DesiredSpeeds[LEFT_MOTOR] = -HALF_SPEED;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = -HALF_SPEED;
}

//*****************************************************************************
//
// Motor should be pushing both wheels at full speed ahead
//
//*****************************************************************************
void Motor_GoForward(void)
{	
	Motor_DesiredSpeeds[LEFT_MOTOR] = FULL_SPEED;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = FULL_SPEED;
}

//*****************************************************************************
//
// Left wheel goes half (maybe more, maybe less) speed, right wheel goes full speed
//
//*****************************************************************************
void Motor_TurnLeft(void)
{	
	Motor_DesiredSpeeds[LEFT_MOTOR] = HALF_SPEED;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = FULL_SPEED;
}

//*****************************************************************************
//
// Right wheel goes half (maybe more, maybe less) speed, left wheel goes full speed
//
//*****************************************************************************
void Motor_TurnRight(void)
{	  
	Motor_DesiredSpeeds[LEFT_MOTOR] = FULL_SPEED;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = HALF_SPEED;
}

//*****************************************************************************
//
// Left wheel stops, right wheel at full speed
//
//*****************************************************************************
void Motor_TurnHardLeft(void)
{  	  
	Motor_DesiredSpeeds[LEFT_MOTOR] = 0;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = FULL_SPEED;
}

//*****************************************************************************
//
// Right wheel stops, left wheel at full speed
//
//*****************************************************************************
void Motor_TurnHardRight(void)
{  	  
	Motor_DesiredSpeeds[LEFT_MOTOR] = FULL_SPEED;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = 0;
}

//*****************************************************************************
//
// Right wheel back at full speed, left wheel back at fraction of full speed
//
//*****************************************************************************
void Motor_TurnBackLeft(void)
{	 
	Motor_DesiredSpeeds[LEFT_MOTOR] = -HALF_SPEED;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = -FULL_SPEED;
}

//*****************************************************************************
//
// Left wheel back at full speed, right wheel back at fraction of full speed
//
//*****************************************************************************
void Motor_TurnBackRight(void)
{	  
	Motor_DesiredSpeeds[LEFT_MOTOR] = -FULL_SPEED;
	Motor_DesiredSpeeds[RIGHT_MOTOR] = -HALF_SPEED;
}

void Motor_Init(void)
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
void Motor_Configure(unsigned char motor_id, unsigned short period, unsigned short duty){
	if (motor_id == LEFT_MOTOR)
	{
		PWM_0_LOAD_R = period - 1; // cycles needed to count down to 0
		PWM_0_CMPA_R = duty - 1; // count value when output rises
		PWM_0_CTL_R |= PWM_X_CTL_ENABLE; // start PWM0
	}
	else
	{
		PWM_0_LOAD_R = period - 1; // cycles needed to count down to 0
		PWM_0_CMPB_R = duty - 1; // count value when output rises
		PWM_0_CTL_R |= PWM_X_CTL_ENABLE; // start PWM0
	}
}

void Motor_Start(unsigned char motor_id)
{   
	if (motor_id == LEFT_MOTOR)
	{
		PWM_ENABLE_R |= PWM_ENABLE_PWM0EN; // enable PWM0
	}
	else
	{
		PWM_ENABLE_R |= PWM_ENABLE_PWM1EN; // enable PWM1
	}
	
}

void Motor_Stop(unsigned char motor_id)
{
	if (motor_id == LEFT_MOTOR)
	{
		PWM_ENABLE_R &= ~(PWM_ENABLE_PWM0EN); // disable PWM0
	}
	else
	{
		PWM_ENABLE_R &= ~(PWM_ENABLE_PWM1EN); // disable PWM1
	}
	
}

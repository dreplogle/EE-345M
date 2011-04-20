//*****************************************************************************
//
// Motor.h
//
//*****************************************************************************
#ifndef _MOTOR
#define _MOTOR

#define PWM_ENABLE_PWM1EN 0x2
#define MOTOR_FORWARD 0
#define MOTOR_BACKWARD 1
#define LEFT_MOTOR 0
#define RIGHT_MOTOR 1
#define PIN_0_WRITE 0x1
#define PIN_1_WRITE 0x2

#define FULL_SPEED 1900
#define HALF_SPEED (FULL_SPEED/2)
#define MAX_POWER 250
#define MIN_DUTY_CYCLE 0
#define MAX_DUTY_CYCLE 9000



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

#define PWM_GENA_VALUE 0xC8
#define PWM_GENB_VALUE 0xC08
#define GPIO_PORTF_AFSEL_R (*((volatile unsigned long *)0x40025420))
#define SYSCTL_RCC_R (*((volatile unsigned long *)0x400FE060))
#define SYSCTL_RCC_USEPWMDIV 0x00100000 // Enable PWM Clock Divisor
#define SYSCTL_RCC_PWMDIV_M 0x000E0000 // PWM Unit Clock Divisor
#define SYSCTL_RCC_PWMDIV_2 0x00000000 // /2
#define SYSCTL_RCGC0_R (*((volatile unsigned long *)0x400FE100))
#define SYSCTL_RCGC0_PWM 0x00100000 // PWM Clock Gating Control
#define SYSCTL_RCGC2_R (*((volatile unsigned long *)0x400FE108))



// ********** Motor_GoForward ***********
// Calculates and sets desired duty cycle to
//   move specified motor at desired rate.
// Inputs:
//   motor_id - left or right motor
//   speed - estimated speed from tachometer(in RPM)
// Outputs: none
// ** Code is based on Professor Valvano's lecture 20
void Motor_PID(unsigned char motor_id, unsigned long speed);

//*****************************************************************************
//
// Motor should be pulling both wheels back at half speed
//
//*****************************************************************************
void Motor_GoBackward(void);

//*****************************************************************************
//
// Motor should be pushing both wheels at full speed ahead
//
//*****************************************************************************
void Motor_GoForward(void);

//*****************************************************************************
//
// Left wheel goes half (maybe more, maybe less) speed, right wheel goes full speed
//
//*****************************************************************************
void Motor_TurnLeft(void);

//*****************************************************************************
//
// Right wheel goes half (maybe more, maybe less) speed, left wheel goes full speed
//
//*****************************************************************************
void Motor_TurnRight(void);

//*****************************************************************************
//
// Left wheel stops, right wheel at full speed
//
//*****************************************************************************
void Motor_TurnHardLeft(void);

//*****************************************************************************
//
// Right wheel stops, left wheel at full speed
//
//*****************************************************************************
void Motor_TurnHardRight(void);

//*****************************************************************************
//
// Right wheel back at full speed, left wheel back at fraction of full speed
//
//*****************************************************************************
void Motor_TurnBackLeft(void);

//*****************************************************************************
//
// Left wheel back at full speed, right wheel back at fraction of full speed
//
//*****************************************************************************
void Motor_TurnBackRight(void);

void Motor_Init(void);

//The following code is copied from Professor Valvano's lecture 18 Program 6.15
// period is number of PWM clock cycles in one period (3<=period)
// duty is number of PWM clock cycles output is high (2<=duty<=period-1)
// PWM clock rate = processor clock rate/SYSCTL_RCC_PWMDIV
// = 6 MHz/2 = 3 MHz (in this example)

//duty = 2000 is very slow, duty = 9900 is very fast
void Motor_Configure(unsigned char motor_id, unsigned char direction, unsigned short period, unsigned short duty);

void Motor_Start(unsigned char motor_id);

void Motor_Stop(unsigned char motor_id);

void Motor_SetDesiredSpeed(unsigned char motor_id, unsigned short speed);

#endif

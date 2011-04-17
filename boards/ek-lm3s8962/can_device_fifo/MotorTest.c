#include "drivers/motor.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#define PWM_0_CMPA_R (*((volatile unsigned long *)0x40028058))

void CAN_Send(char* buffer){}
void CANIntHandler(void){}

int main(void)
{
	MotorInit();
	LeftMotorConfigure(10000, 9900);
	setMotorDirection(0,0);
	setMotorDirection(0,1);
	LeftMotorStart();

	RightMotorConfigure(10000,9900);
	setMotorDirection(1,0);
	setMotorDirection(1,1);
	RightMotorStart();

	while(1)
	{
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(0,8000);
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(0,6000);
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(0,4000);
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(0,2000);
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(1,8000);
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(1,6000);
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(1,4000);
		SysCtlDelay(SysCtlClockGet()/1);
		setDutyCycle(1,2000);
	}

}
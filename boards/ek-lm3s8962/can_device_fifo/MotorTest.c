#include "drivers/motor.h"
#include "drivers/tachometer.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#define PWM_0_CMPA_R (*((volatile unsigned long *)0x40028058))

void CAN_Send(char* buffer){}
void CANIntHandler(void){}

int main(void)
{
	Motor_Init();
	Tach_Init(0);
	Motor_Configure(0, 10000, 1);
	//setMotorDirection(0,0);
	//setMotorDirection(0,1);
	Motor_Start(0);

	Motor_Configure(1, 10000,1);
	//setMotorDirection(1,0);
	//setMotorDirection(1,1);
	Motor_Start(1);

	while(1)
	{
		Tach_SendData(0);
		Tach_SendData(1);
		
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(0,8000);
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(0,6000);
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(0,4000);
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(0,2000);
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(1,8000);
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(1,6000);
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(1,4000);
//		SysCtlDelay(SysCtlClockGet()/1);
//		setDutyCycle(1,2000);
	}

}

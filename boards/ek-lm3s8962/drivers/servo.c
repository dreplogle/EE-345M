#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/systick.h"
#include "driverlib/fifo.h"
#include "driverlib/adc.h"
#include "driverlib/pwm.h"
#include "servo.h"

extern unsigned long PWMduty;
void Servo_Init(void)
{
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
  GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_2);
  GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA,  GPIO_PIN_TYPE_STD_WPU);  
}
unsigned long DebugAngle = 0;
void Servo_SetAngle(unsigned long angle){
 unsigned long ulPeriod = 21739;
 DebugAngle = angle;
	 switch(angle){
	 	case SERVO_SHARP_LEFT: PWMduty = (ulPeriod * 1460)/10000; break;
		case SERVO_MEDIUM_LEFT: PWMduty = 	(ulPeriod * 1200)/10000; break;
		case SERVO_FINE_LEFT: PWMduty = (ulPeriod * 1140)/10000; break;
		case SERVO_SUPER_FINE_LEFT: PWMduty = (ulPeriod * 1120)/10000; break;
		case SERVO_STRAIGHT: PWMduty = (ulPeriod * 1100)/10000; break;
		case SERVO_SUPER_FINE_RIGHT: PWMduty = (ulPeriod * 1060)/10000; break;
		case SERVO_FINE_RIGHT: PWMduty = (ulPeriod * 1020)/10000; break;
		case SERVO_MEDIUM_RIGHT: PWMduty = (ulPeriod * 980)/10000; break;
		case SERVO_SHARP_RIGHT: PWMduty = (ulPeriod * 900)/10000; break;
		default: return;
	 }
}

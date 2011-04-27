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

extern unsigned long PWMduty;
void Servo_Init(void)
{
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
  GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_2);
  GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA,  GPIO_PIN_TYPE_STD_WPU);  
}

void Servo_SetAngle(unsigned long angle){
 unsigned long ulPeriod = 21739;
	 switch(angle){
	 	case 30: PWMduty = (ulPeriod * 1400)/10000; break;
		case 90: PWMduty = (ulPeriod * 1050)/10000; break;
		case 150: PWMduty = (ulPeriod * 800)/10000; break;
		default: return;
	 }
}

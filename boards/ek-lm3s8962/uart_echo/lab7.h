//*****************************************************************************
//
// Lab7.c - user programs, File system, stream data onto disk
// Jonathan Valvano, March 16, 2011, EE345M
//     You may implement Lab 5 without the oLED display
//*****************************************************************************

struct sensors{
	unsigned long ping;
	unsigned long tach;
	long IR0;
  long IR1;
  long IR2;
  long IR3;
};

extern struct sensors Sensor;
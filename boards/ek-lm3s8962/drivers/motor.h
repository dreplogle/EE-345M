//*****************************************************************************
//
// Motor.h
//
//*****************************************************************************

//*****************************************************************************
//
// Motor should be pulling both wheels back at half speed
//
//*****************************************************************************
void MotorBackward(void);

//*****************************************************************************
//
// Motor should be pushing both wheels at full speed ahead
//
//*****************************************************************************
void MotorForward(void);

//*****************************************************************************
//
// Left wheel goes half (maybe more, maybe less) speed, right wheel goes full speed
//
//*****************************************************************************
void MotorTurnLeft(void);

//*****************************************************************************
//
// Right wheel goes half (maybe more, maybe less) speed, left wheel goes full speed
//
//*****************************************************************************
void MotorTurnRight(void);

//*****************************************************************************
//
// Left wheel stops, right wheel at full speed
//
//*****************************************************************************
void MotorTurnHardLeft(void);

//*****************************************************************************
//
// Right wheel stops, left wheel at full speed
//
//*****************************************************************************
void MotorTurnHardRight(void);

//*****************************************************************************
//
// Right wheel back at full speed, left wheel back at fraction of full speed
//
//*****************************************************************************
void MotorTurnBackLeft(void);

//*****************************************************************************
//
// Left wheel back at full speed, right wheel back at fraction of full speed
//
//*****************************************************************************
void MotorTurnBackRight(void);



void MotorInit(void);

void LeftMotorConfigure(unsigned short period, unsigned short duty);

void RightMotorConfigure(unsigned short period, unsigned short duty);

void setMotorDirection(unsigned char motor, unsigned char direction);

void LeftMotorStart(void);

void RightMotorStart(void);

void LeftMotorStop(void);

void RightMotorStop(void);


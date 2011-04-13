
// ******** Ping_Init ************
// Initializes all timers and ports that
// are related to the Ping sensor.
// Inputs: "periodicTimer" is the timer used for the periodic interrupt
// that calls pingProducer. "subTimer" is the sub-timer used for the periodic
// interrupt that calls pingProducer.
// Outputs: none
void Ping_Init(unsigned long periodicTimer, unsigned long subTimer);



// ******** pingProducer ************
// Starts a Ping distance measurement
// by the Ping sensor
// Inputs: none
// Outputs: none
void pingProducer(void);


// ******** pingConsumer ************
// Gets pulse width data from the Ping fifo
// and converts it into distance
// Inputs: none
// Outputs: none
void pingConsumer(void);


// ******** Ping_Fifo_Put ************
// Puts one value to Ping data FIFO
// Inputs: buffer data entry
// Outputs: 0 if buffer is full, 1 if put is successful
int Ping_Fifo_Put(unsigned long data);


// ******** Ping_Fifo_Init ************
// Initializes Ping data FIFO
// Inputs: size of the FIFO
// Outputs: none
void Ping_Fifo_Init(void);


// ******** Ping_Fifo_Get ************
// Retreives one value from Ping data FIFO
// Inputs:  none
// Outputs: buffer entry
unsigned long Ping_Fifo_Get(void);


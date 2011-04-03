void Ping_Init(unsigned long periodicTimer, unsigned long subTimer);
void pingProducer(void);
void pingConsumer(void);
int Ping_Fifo_Put(unsigned long data);
void Ping_Fifo_Init(void);
unsigned long Ping_Fifo_Get(void);
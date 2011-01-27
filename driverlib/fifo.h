// Fifo.h



//
// This macro allows for the creation of a FIFO with
// a variable name.  Simple put and get functions for
// the fifo created are also defined.
//
// This macro was supplied by Jonathan Valvano, EE 380L 
// laboratory manual.
//
#define AddFifo(NAME,SIZE,TYPE, SUCCESS,FAIL) \
unsigned long volatile PutI ## NAME; \
unsigned long volatile GetI ## NAME; \
TYPE static Fifo ## NAME [SIZE]; \
void NAME ## Fifo_Init(void){ \
  PutI ## NAME= GetI ## NAME = 0; \
} \
int NAME ## Fifo_Put (TYPE data){ \
  if(( PutI ## NAME - GetI ## NAME ) & ~(SIZE-1)){ \
     return(FAIL); \
  } \
  Fifo ## NAME[ PutI ## NAME &(SIZE-1)] = data; \
  PutI ## NAME ## ++; \
  return(SUCCESS); \
} \
int NAME ## Fifo_Get (TYPE *datapt){ \
  if( PutI ## NAME == GetI ## NAME ){ \
    return(FAIL); \
  } \
  *datapt = Fifo ## NAME[ GetI ## NAME &(SIZE-1)]; \
  GetI ## NAME ## ++; \
  return(SUCCESS); \
}

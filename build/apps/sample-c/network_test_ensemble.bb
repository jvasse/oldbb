#include "block.bbh"

/********************************************** HOW TO READ BLOCK COLORS ******************************************************
 * 1 - DURING SPANNING TREE SETUP: 
 * 	Leader is blue
 * 	blocks not yet in spanning tree are red
 * 	blocks turn blue once they are added to the spanning tree
 * 2 - DURING NETWORK TEST: 
 * 	Blocks which receive a message from the leader to the leaves turn red
 * 	once the message is received by the leaves, the leaves send back a message to their parent and stay red
 * 	parents turn green when they receive a message from all of their children
 * 	once the leader has received a message from all of its children, the cycle is complete, it turns green and starts a new cycle by turning blue
 *******************************************************************************************************************************/

void handleReceivedCommand(void);
byte resetReceived = 0;

/****************************************
 ************ SPANNING TREE *************
 ***************************************/
#define FILENUM 20
#define ST_NACK		0
#define ST_ACK		1
#define ADD_YOURSELF	2

void sendAddYourself(PRef p);
void sendACK(PRef p);
void sendNACK(PRef p);
void sendSuccessMsg(PRef p);
void addYourselfHandler(void);
void addChildHandler(void);
void successMsgHandler(void);

threadvar PRef children[NUM_PORTS];
threadvar PRef parent;
threadvar byte numChildren = 0;
threadvar byte isSTLeader = 0;
threadvar byte isInTree = 0;
threadvar byte isLeaf = 0;

/****************************************
 ************* NETWORK TEST *************
 ***************************************/

#define COM			3
#define COMBACK			4
#define SUCCESS			5
#define MAX_CYCLE               20
// #define ROOT_ID 		20


void goMsgHandler(void);
void backMsgHandler(void);
void sendCycle(PRef p);
void sendBackCycle(PRef p);
void startNextCycle(void);
void cycleFailed(void);

byte childResponseCount;
byte cycleNum;
byte failedCycles;

// CHUNCK MANAGEMENT
void freeMyChunk(void);
byte sendMyChunk(PRef port, byte *data, byte size, MsgHandler mh); 
uint16_t id;

// TIME MANAGEMENT
#define CYCLE_EXPIRE_TIME 3000
#define START_TIME 2000


Timeout startTest;
Timeout cycleTimeout;

void setNextCycleTimeout(void);

void myMain(void)
{  
  byte p;

  // initialize variables
  childResponseCount = 0;
  cycleNum = 0;
  failedCycles = 0;

  // initialize timeouts
  cycleTimeout.callback = (GenericHandler)(&cycleFailed);
  startTest.callback = (GenericHandler)(&startNextCycle);

  // build Spanning Tree
  // initialize children table
  for (p = 0 ; p < NUM_PORTS ; p++) children[p] = 0;
  
  //choosing the leader of the Spanning Tree
 for(p = 0; p < NUM_PORTS ; p++) {
   if(isHostPort(p)) {
     id = getGUID();
   }
 }
  
  // ID 2 is spanning tree leader and starts the set up
  if (getGUID() == id) {
    isSTLeader = 1;
    isInTree = 1;
    setColor(BLUE);
   
    for (p = 0 ; p < NUM_PORTS ; p++) { 
      if (thisNeighborhood.n[p] != VACANT) sendAddYourself(p);
    }
    
    // Setting test start time
    startTest.calltime = getTime() + START_TIME;
    registerTimeout(&startTest);
  }
  else {
    isSTLeader = 0;
    setColor(RED);
  }

  // infinite loop to avoid bootloader mode
  while(1);
}

/**************************************************
 ************* NETWORK TEST FUNCTIONS *************
 **************************************************/


// If cycle has failed, send log and start next cycle
void 
cycleFailed(void)
{
  if ( cycleNum < (MAX_CYCLE + 1) ) {
  char s[15];
#ifdef LOG_DEBUG
  snprintf(s, 15*sizeof(char), "FAILED");
  printDebug(s);
#endif  
  setColor(RED);
  
  failedCycles++;
  startNextCycle();
  }
}

void 
startNextCycle(void) 
{
  char s[15];
  
#ifdef LOG_DEBUG
  snprintf(s, 15*sizeof(char), "C%d GO", cycleNum);
  printDebug(s);
#endif

  // Send go message to all children
  for(byte p = 0; p < NUM_PORTS; p++) 
    if (children[p] == 1) sendCycle(p);   
    
  // Set timeout for next cycle
  setNextCycleTimeout();
}

void 
backMsgHandler(void)
{ 
  char s[15];
  delayMS(200);

  childResponseCount++;
  
  if(!isSTLeader) {
    if (childResponseCount == numChildren) {
      childResponseCount = 0;
      setColor(BLUE);
      sendBackCycle(parent);
    }
    else{
    setColor(AQUA);
    }
  }
  else {
    if (childResponseCount == numChildren) {
      childResponseCount = 0;
      setColor(BLUE);    
      
#ifdef LOG_DEBUG
      snprintf(s, 10*sizeof(char), "C%d PASSED", cycleNum);
      printDebug(s);
#endif

      // Cycle succeeded, cancel last timeout
      deregisterTimeout(&cycleTimeout);

      if (cycleNum++ == MAX_CYCLE) {
#ifdef LOG_DEBUG
	snprintf(s, 15*sizeof(char), "err: %d/%d", failedCycles, MAX_CYCLE);
	printDebug(s);
	snprintf(s, 15*sizeof(char), "Done.");
	printDebug(s);
#endif
	byte p;
	setColor(GREEN);
	   for (p = 0; p < NUM_PORTS; p++) { 
      if (children[p] == 1) sendSuccessMsg(p);
    }  
	
	return;
      }
      else startNextCycle();
    }
    else
    {
      setColor(AQUA);
    }
  }
}

void goMsgHandler(void)
{ 
  delayMS(200);
  byte p;
  setColor(RED);
  if (numChildren > 0) {
    for (p = 0; p < NUM_PORTS; p++) { 
      if (children[p] == 1) sendCycle(p);
    }    
  }
  else sendBackCycle(parent);
}

void successMsgHandler(void) 
{ 
  byte p;
  setColor(GREEN);
  if (numChildren > 0) {
    for (p = 0; p < NUM_PORTS; p++) {
      if (children[p] == 1) sendSuccessMsg(p);   
    }
  }
}

// Message to send to all children towards the leaves
void sendCycle(byte child)
{
  byte data[1];
  data[0] = COM;
  sendMyChunk(child, data, 1, (MsgHandler)goMsgHandler);
}

// Message from the children to the leader to acknowledge reception of the message by the leaves
void sendBackCycle(byte parent)
{
  byte data[1];
  data[0] = COMBACK;  
  sendMyChunk(parent, data, 1, (MsgHandler)backMsgHandler);
}

// Once the targeted number of succesful cycles has been hit, spread this message to set everyone green
void sendSuccessMsg(byte child)
{
  byte data[1];
  data[0] = SUCCESS;  
  sendMyChunk(child, data, 1, (MsgHandler)successMsgHandler);
}

void setNextCycleTimeout(void) 
{
  cycleTimeout.calltime = getTime() + CYCLE_EXPIRE_TIME;
  registerTimeout(&cycleTimeout);
}

/*********************************************
 ********** SPANNING TREE FUNCTIONS **********
 ********************************************/

void addYourselfHandler(void)
{
  byte p;
  
  if (isInTree) sendNACK(faceNum(thisChunk));
  else {
      // NOW IN TREE
      setColor(BLUE);
      isInTree  = 1;
    
      // TELLS PARENT
      parent = faceNum(thisChunk);
      sendACK(parent);
   
      // GETS CHILDREN
      for (p = 0 ; p < NUM_PORTS ; p++) {
	  if (p == parent || thisNeighborhood.n[p] == VACANT) continue;
	  else sendAddYourself(p);
	}
    }
}

void addChildHandler(void)
{
  switch (thisChunk->data[0]) {
  case ST_ACK:
    children[faceNum(thisChunk)] = 1;
    numChildren++;
    break;
  case ST_NACK:
    // Just ignore this face.
    break;
  }
}

void sendAddYourself(PRef p)
{ 
  byte data[1];
  data[0] = ADD_YOURSELF;
  
  sendMyChunk(p, data, 1, (MsgHandler)addYourselfHandler);
}

void sendACK(PRef p)
{ 
  byte data[1];
  data[0] = ST_ACK;
  
  sendMyChunk(p, data, 1, (MsgHandler)addChildHandler);
}



void sendNACK(PRef p)
{ 
  byte data[1];
  data[0] = ST_NACK;
  
  sendMyChunk(p, data, 1, (MsgHandler)addChildHandler);
}

/************************************************************
 ***************CHUNCK MANAGEMENT FUNCTIONS*******************
 ************************************************************/

void freeMyChunk(void)
{
  freeChunk(thisChunk);
}

byte sendMyChunk(PRef port, byte *data, byte size, MsgHandler mh) 
{ 
  Chunk *c=getSystemTXChunk();
  if (c == NULL) return 0;
  if (sendMessageToPort(c, port, data, size, mh, (GenericHandler)&freeMyChunk) == 0) {
    freeChunk(c);
    return 0;
  }
  return 1;
}

/*void handleReceivedCommand(void)
  {
  initializeMemory();
  callHandler(SYSTEM_MAIN);
  }*/

/************************************************************
 ************************COMMAND HANDLING*********************
 ************************************************************/

void userRegistration(void)
{
  registerHandler(SYSTEM_MAIN, (GenericHandler)&myMain);	
  //registerHandler(EVENT_COMMAND_RECEIVED, (GenericHandler)&handleReceivedCommand);
}


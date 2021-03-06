#include "block.bbh"

/*-------------------- SIMPLE EXAMPLE OF HOW TO USE TIMEOUTS INSIDE YOUR PROGRAM */

// Time between color changes
#define COLOR_CHANGE_TIME 1000

Timeout tout;

void setNextTimeout(void);

// Set next color on the block every COLOR_CHANGE_TIME ms
void 
nextColor(void)
{
  setNextColor();
  setNextTimeout();
}

// Re-enable timeout
void setNextTimeout(void)
{
  // set calltime
  tout.calltime = getTime() + COLOR_CHANGE_TIME;
  // start countdown
  registerTimeout( &tout );
}

void 
myMain(void) 
{
  setColor(RED);

  // initialize timeout - set its callback function
  tout.callback = (GenericHandler)(&nextColor);

  setNextTimeout();

  while(1);
}

void 
userRegistration(void)
{
  registerHandler(SYSTEM_MAIN, (GenericHandler)&myMain);	
}

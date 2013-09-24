#include "block.bbh"

void myMain(void)
{
    AccelData acc;
    MicData mic;

    /***************** indicate set-up *****************/
    setColor(GREEN);

    /******* set-up custom accelerometer status *******/
    // put into standby mode to update registers
    setAccelRegister(0x07, 0x18);

    // every measurement triggers an interrupt
    setAccelRegister(0x06, 0x10);

    // set filter rate
    setAccelRegister(0x08, 0x00);

    // enable accelerometer
    setAccelRegister(0x07, 0x19);

    /***** end set-up custom accelerometer status *****/

    // indicate reset
    setColor(GREEN);
    /******************* end set-up *******************/
    
    while(1)
    {
        // get accelerometer data
        acc = getAccelData();

        // get microphone data
        BB_LOCK(ATOMIC_RESTORESTATE)
        mic = getMicData();
        BB_UNLOCK(NULL)

        // print all data
        printf("%i %i %i %i\r\n", acc.x, acc.y, acc.z, mic);
    }
}

void userRegistration(void)
{
    registerHandler(SYSTEM_MAIN, (GenericHandler)&myMain);
}


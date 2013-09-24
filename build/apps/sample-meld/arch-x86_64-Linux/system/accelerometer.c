# 1 "/home/dcampbel/Research/blinkyBocksHardware/build/src-bobby/system/accelerometer.bb"
#include "../sim/block.h"
// SYSTEM INCLUDES
#include "accelerometer.h"
#include "handler.h"

// HARDWARE INCLUDES
#include "../hw-api/hwAccelerometer.h"

// AccelData _acc;

AccelData getAccelData()
{
	return  (this()->_acc) ;
}

int newAccelData()
{
	return newHWAccelData();
}

void updateAccel()
{
	//byte oldstatus = _acc.status & ACC_O_MASK;

	// this changes the _acc datastructure with new data, if available
	updateHWAccel();

}

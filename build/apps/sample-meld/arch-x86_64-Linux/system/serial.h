# 1 "/home/dcampbel/Research/blinkyBocksHardware/build/src-bobby/system/serial.bbh"
#include "../sim/block.h"
#ifndef __SERIAL_H__
#define __SERIAL_H__

#include "circ_buffer.h"
#include "queues.h"

// typedef uint8_t PRef;
// enum portReferences { DOWN, NORTH, EAST, WEST, SOUTH, UP, NUM_PORTS };

// typedef struct _port_t { PRef pnum; SendChunkQueue sq; ReceiveChunkQueue rq; CircBuf rx; CircBuf tx; } Port;

void initPorts(void);

#endif

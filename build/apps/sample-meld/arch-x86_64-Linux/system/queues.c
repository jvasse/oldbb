# 1 "/home/dcampbel/Research/blinkyBocksHardware/build/src-bobby/system/queues.bb"
#include "../sim/block.h"
// queues.c
//
// implementation of the queues

#ifndef _QUEUES_C_
#define _QUEUES_C_

#include "queues.h"

void retrySend(void)
{
    SendChunkQueue* currSq = ((SQTimeout *) (this()->thisTimeout) )->sq;

    //Try to resend
    currSq->flags |= CLEAR_TO_SEND;
}

#endif

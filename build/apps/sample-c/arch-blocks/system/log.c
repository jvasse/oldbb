# 1 "/home/pthalamy/CMU/build-modif/src-bobby/system/log.bb"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "ensemble.h"
#include "data_link.h"
#include "serial.h"

#define UNDEFINED_HOST 200

#define LOG_I_AM_HOST			0x01
#define LOG_PATH_TO_HOST		0x02
#define LOG_NEED_PATH_TO_HOST	0x03
#define LOG_DATA				0x04

//#define FORCE_TRANSMISSION

 byte PCConnection = 0;
 PRef toHost = 200;

byte sendLogChunk(PRef p, byte *d, byte s);

void freeLogChunk(void)
{
#ifdef FORCE_TRANSMISSION
	 if(chunkResponseType(thisChunk) == MSG_RESP_ACK) {
		free(thisChunk);
	} else {
		// try again
		sendLogChunk(faceNum(thisChunk), thisChunk->data, DATA_SIZE);
		free(thisChunk);
	}
#else
	free(thisChunk);
#endif
}

byte isHostPort(PRef p)
{
	return ((p == toHost) && (PCConnection == 1));
}

byte sendLogChunk(PRef p, byte *d, byte s)
{
	Chunk *c=calloc(sizeof(Chunk), 1);
#ifdef FORCE_TRANSMISSION
	while (c == NULL)
	{
		c=calloc(sizeof(Chunk), 1);
	}
	while (sendMessageToPort(c, p, d, s, (MsgHandler)RES_SYS_HANDLER, (GenericHandler)&freeLogChunk) == 0);
	return 1;
#else
	if (c == NULL)
	{
		return 0;
	}
	if (sendMessageToPort(c, p, d, s, (MsgHandler)RES_SYS_HANDLER, (GenericHandler)&freeLogChunk) == 0)
	{
		free(c);
		return 0;
	}
	return 1;
#endif

}


void sendPathToHost(PRef p)
{
	byte buf[2];

	buf[0] = LOG_MSG;
	buf[1] = LOG_PATH_TO_HOST;
	sendLogChunk(p, buf, 2);

}

void spreadPathToHost(PRef excluded)
{
	byte p;

	for( p = 0; p < NUM_PORTS; p++)
	{
		if ((p == excluded) || (thisNeighborhood.n[p] == VACANT))
		{
			continue;
		}
		sendPathToHost(p);
	}
}

void forwardToHost(Chunk *c)
{
	if(toHost != UNDEFINED_HOST) {
		sendLogChunk(toHost, c->data, DATA_SIZE);
	}
}

void initLogDebug(void)
{
	byte buf[2];

	toHost = UNDEFINED_HOST;
	PCConnection = 0;

	buf[0] = LOG_MSG;
	buf[1] = LOG_NEED_PATH_TO_HOST;
	byte p;

	setColor(ORANGE); // to remember to the user that the block is waiting
	while(toHost == UNDEFINED_HOST)
	{
		for( p = 0; p < NUM_PORTS; p++)
		{
			if ((thisNeighborhood.n[p] == VACANT))
			{
				continue;
			}
			sendLogChunk(p, buf, 2);
		}
		delayMS(6);
	}
}

byte handleLogMessage(void)
{

	if( thisChunk == NULL )
	{
		return 0;
	}
	switch(thisChunk->data[1])
	{
		case LOG_I_AM_HOST:
			toHost = faceNum(thisChunk);
			PCConnection = 1;
			spreadPathToHost(faceNum(thisChunk));
		break;
		case LOG_PATH_TO_HOST:
			if (toHost == UNDEFINED_HOST) {
				toHost = faceNum(thisChunk);
				PCConnection = 0;
				spreadPathToHost(faceNum(thisChunk));
			}
		break;
		case LOG_NEED_PATH_TO_HOST:
			if (toHost != UNDEFINED_HOST) {
				sendPathToHost(faceNum(thisChunk));
			}
		break;
		case LOG_DATA:
			if(toHost != UNDEFINED_HOST) {
				forwardToHost(thisChunk);
			}
		break;
	}

	return 1;
}

byte getSize(char* str) {
	byte sizeCar = 0;
	byte sizeChunk = 1;

	if (str == NULL) {
        return 0;
    }

	sizeCar = strlen(str) + 1;

    if (sizeCar < 11) {
        return 1;
    }

    sizeCar -= 10;
    sizeChunk += sizeCar / 11;
	if ((sizeCar % 11) != 0) {
		sizeChunk++;
	}
	return sizeChunk;
}

// format: <LOG_MSG> <LOG_DATA> <block id (2 bytes) > <message # (1 byte)> < fragment # (1 byte)> < if fragment # = 1, number of fragments. Otherwise data> < log data: 17 - 7 = 10>.
byte printDebug(char* str) {
	byte size = getSize(str);
	static byte mId = 0;
	uint8_t index = 0;
	byte buf[DATA_SIZE];
	byte s = 0;
	byte fId = 0;
	byte off = 6;

	if (toHost == UNDEFINED_HOST)
	{
		return 0;
	}

	buf[0] = LOG_MSG;
	buf[1] = LOG_DATA;
	GUIDIntoChar(getGUID(), &(buf[2]));
	buf[4] = mId;

	if (size == 1)
	{
		off = 7;
		s = strlen(str)+1;
		memcpy(buf+off, str, s);
		buf[5] = 0;
		buf[6] = size;
		sendLogChunk(toHost, buf, s+off);
	}
	else
	{
		for (fId = 0; fId < size; fId++)
		{
			buf[5] = fId;
			if (fId == 0)
			{
				buf[6] = size;
				off = 7;
				s = 10;
			}
			else if (fId == (size -1))
			{
				s = strlen(str+index)+1 ;
			}
			else
			{
				s = 11;
			}
			memcpy(buf+off, str+index, s);
			index += s;
			sendLogChunk(toHost, buf, s+off);
			off = 6;
		}
	}
	mId++;
	return 1;
}

byte blockingPrintDebug(char *s)
{
	while(toHost == UNDEFINED_HOST)
	{
		delayMS(1);
	}
	return printDebug(s);
}

/*
// format: <LOG_MSG> <LOG_DATA> <block id (2 bytes) > <message # (1 byte)> < fragment # (1 byte)> < if fragment # = 1, number of fragments. Otherwise data> < log data: 17 - 7 = 10>.
byte print(char* str) {

}

byte printDebug(char *s, ...)
{
	char buffer[251];
	va_list args;
	va_start (args,s);
	vsprintf(buffer, s, args);
	va_end(args);
	buffer[250] = '\0';
	return print(buffer);
}

byte blockingPrintDebug(char *s)
{
	va_list args;
	byte ret = 0;

	while(toHost == UNDEFINED_HOST)
	{
		delayMS(1);
	}
	va_start (args,s);
	ret = printDebug(s);
	va_end(args);
	return ret;
} */


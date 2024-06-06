/*
MACINTOSH_NETWORK.H
Monday, June 20, 1994 12:22:25 PM
Wednesday, August 9, 1995 3:34:50 PM- network lookup stuff now takes a version which is 
	concatenated to the lookup type (ryan)
*/

#include <AppleTalk.h>
#include <ADSP.h>

#include "network.h"

/* ---------- constants */

#define strNETWORK_ERRORS 132
enum /* error string for user */
{
	netErrCantAddPlayer,
	netErrCouldntDistribute,
	netErrCouldntJoin,
	netErrServerCanceled,
	netErrMapDistribFailed,
	netErrWaitedTooLongForMap,
	netErrSyncFailed,
	netErrJoinFailed,
	netErrCantContinue
};

/* missing from AppleTalk.h */
#define ddpMaxData 586

/* ---------- DDPFrame and PacketBuffer (DDP) */

struct DDPFrame
{
	short data_size;
	byte data[ddpMaxData];
	
	MPPParamBlock pb;

	WDSElement wds[3];
	byte header[17];
};
typedef struct DDPFrame DDPFrame, *DDPFramePtr;

struct DDPPacketBuffer
{
	short inUse;
	
	byte protocolType;
	byte destinationNode;
	AddrBlock sourceAddress;
	short hops;
	
	short datagramSize;
	byte datagramData[ddpMaxData];
};
typedef struct DDPPacketBuffer DDPPacketBuffer, *DDPPacketBufferPtr;

/* ---------- ConnectionEnd (ADSP) */

struct ConnectionEnd
{
	short ccbRefNum, socketNum; /* reference number and socket number of this connection end */

	/* memory for ADSP */
	TPCCB dspCCB;
	Ptr dspSendQPtr;
	Ptr dspRecvQPtr;
	Ptr dspAttnBufPtr;

#ifdef env68k	
	long a5; /* store our current a5 here */
#endif

	DSPParamBlock pb; /* parameter block for this connection end */
};
typedef struct ConnectionEnd ConnectionEnd, *ConnectionEndPtr;

/* ---------- types */

typedef EntityName *EntityNamePtr;

typedef void (*lookupUpdateProcPtr)(short message, short index);
typedef boolean (*lookupFilterProcPtr)(EntityName *entity, AddrBlock *address);
typedef void (*packetHandlerProcPtr)(DDPPacketBufferPtr packet);

/* ---------- prototypes/NETWORK.C */

short NetState(void);

void NetSetServerIdentifier(short identifier);

/* for giving to NetLookupOpen() as a filter procedure */
boolean NetEntityNotInGame(EntityName *entity, AddrBlock *address);

/* ---------- prototypes/NETWORK_NAMES.C */

OSErr NetRegisterName(char *name, char *type, short version, short socketNumber);
OSErr NetUnRegisterName(void);

/* ---------- prototypes/NETWORK_LOOKUP.C */

void NetLookupUpdate(void);
void NetLookupClose(void);
OSErr NetLookupOpen(char *name, char *type, char *zone, short version, 
	lookupUpdateProcPtr updateProc, lookupFilterProcPtr filterProc);
void NetLookupRemove(short index);
void NetLookupInformation(short index, AddrBlock *address, EntityName *entity);

OSErr NetGetZonePopupMenu(MenuHandle menu, short *local_zone);
OSErr NetGetZoneList(Ptr zone_names, short maximum_zone_names, short *zone_count, short *local_zone);
OSErr NetGetLocalZoneName(Str32 local_zone_name);

/* ---------- prototypes/NETWORK_DDP.C */

OSErr NetDDPOpen(void);
OSErr NetDDPClose(void);

OSErr NetDDPOpenSocket(short *socketNumber, packetHandlerProcPtr packetHandler);
OSErr NetDDPCloseSocket(short socketNumber);

DDPFramePtr NetDDPNewFrame(void);
void NetDDPDisposeFrame(DDPFramePtr frame);

OSErr NetDDPSendFrame(DDPFramePtr frame, AddrBlock *address, short protocolType, short socket);

/* ---------- prototypes/NETWORK_ADSP.C */

OSErr NetADSPOpen(void);
OSErr NetADSPClose(void);

OSErr NetADSPEstablishConnectionEnd(ConnectionEndPtr *connection);
OSErr NetADSPDisposeConnectionEnd(ConnectionEndPtr connectionEnd);

OSErr NetADSPOpenConnection(ConnectionEndPtr connectionEnd, AddrBlock *address);
OSErr NetADSPCloseConnection(ConnectionEndPtr connectionEnd, boolean abort);
OSErr NetADSPWaitForConnection(ConnectionEndPtr connectionEnd);
Boolean NetADSPCheckConnectionStatus(ConnectionEndPtr connectionEnd, AddrBlock *address);

OSErr NetADSPWrite(ConnectionEndPtr connectionEnd, void *buffer, word *count);
OSErr NetADSPRead(ConnectionEndPtr connectionEnd, void *buffer, word *count);

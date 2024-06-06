/*
NETWORK_ADSP.C
Sunday, June 26, 1994 5:33:43 PM
Monday, July 18, 1994 11:47:15 AM
	NetADSPWrite and NetADSPRead now take a "word" for length because ADSP can only write
	64K chunks
*/

#include "macintosh_cseries.h"
#include "macintosh_network.h"

#ifdef mpwc
#pragma segment network
#endif

/* ---------- constants */

#define kSendBlocking 0
#define kBadSeqMax 0

#define DSP_QUEUE_SIZE 1024

/* ---------- globals */

static short dspRefNum; /* reference number returned by OpenDriver('.DSP', ... ) */

static DSPPBPtr myDSPPBPtr; /* general-purpose non-asynchronous ADSP parameter block */

/* ---------- code */

/*
-----------
NetADSPOpen
-----------

	<--- error

allocates myDSPPBPtr, opens the '.DSP' driver and saves it�s reference number

------------
NetADSPClose
------------

	<--- error
*/

OSErr NetADSPOpen(
	void)
{
	OSErr error;
	
#ifdef OBSOLETE
	if (!DeferredReadUPP) DeferredReadUPP= NewADSPCompletionProc(NetDeferredReadCompletionRoutine);
	assert(DeferredReadUPP);
#endif

	error= OpenDriver("\p.DSP", &dspRefNum);
	if (error==noErr)
	{
		myDSPPBPtr= (DSPPBPtr) NewPtrClear(sizeof(DSPParamBlock));
		
		error= MemError();
		if (error==noErr)
		{
		}
	}
	
	return error;
}

OSErr NetADSPClose(
	void)
{
	OSErr error;

	DisposePtr((Ptr)myDSPPBPtr);
	
	error= noErr;
	
	return error;
}

/*
-----------------------------
NetADSPEstablishConnectionEnd
-----------------------------

	---> pointer to a pointer to a ConnectionEnd (will be filled in if error==noErr)
	
	<--- error

establishes an ADSP connection end

---------------------------
NetADSPDisposeConnectionEnd
---------------------------
	
	---> connectionEndPtr

	<--- error

disposes of an ADSP connection end
*/

OSErr NetADSPEstablishConnectionEnd(
	ConnectionEndPtr *connection)
{
	ConnectionEndPtr connectionEnd= (ConnectionEndPtr) NewPtrClear(sizeof(ConnectionEnd));
	OSErr error;
	
	error= MemError();
	if (error==noErr)
	{
		connectionEnd->dspCCB= (TPCCB) NewPtrClear(sizeof(TRCCB));
		connectionEnd->dspSendQPtr= NewPtrClear(DSP_QUEUE_SIZE);
		connectionEnd->dspRecvQPtr= NewPtrClear(DSP_QUEUE_SIZE);
		connectionEnd->dspAttnBufPtr= NewPtrClear(attnBufSize);

		error= MemError();
		if (error==noErr)
		{
			myDSPPBPtr->csCode= dspInit; /* establish a connection end */
			myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
			
			myDSPPBPtr->u.initParams.localSocket= 0; /* dynamically allocate a socket */
			myDSPPBPtr->u.initParams.userRoutine= (ADSPConnectionEventUPP) NULL; /* no unsolicited connection events */
			myDSPPBPtr->u.initParams.ccbPtr= connectionEnd->dspCCB;
			myDSPPBPtr->u.initParams.sendQSize= DSP_QUEUE_SIZE;
			myDSPPBPtr->u.initParams.sendQueue= connectionEnd->dspSendQPtr;
			myDSPPBPtr->u.initParams.recvQSize= DSP_QUEUE_SIZE;
			myDSPPBPtr->u.initParams.recvQueue= connectionEnd->dspRecvQPtr;
			myDSPPBPtr->u.initParams.attnPtr= connectionEnd->dspAttnBufPtr;
			
			error= PBControl((ParmBlkPtr)myDSPPBPtr, FALSE);
			if (error==noErr)
			{
				connectionEnd->ccbRefNum= myDSPPBPtr->ccbRefNum; /* save CCB reference number */
				connectionEnd->socketNum= myDSPPBPtr->u.initParams.localSocket; /* save socket */
#ifdef env68k
				connectionEnd->a5= get_a5(); /* save a5 for later asynchronous calls to retrieve */
#endif
				
				myDSPPBPtr->csCode= dspOptions;
				myDSPPBPtr->ioCRefNum= dspRefNum;
				myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum;
				
				myDSPPBPtr->u.optionParams.sendBlocking= kSendBlocking;
				myDSPPBPtr->u.optionParams.badSeqMax= kBadSeqMax;
				myDSPPBPtr->u.optionParams.useCheckSum= FALSE;
				
				error= PBControl((ParmBlkPtr)myDSPPBPtr, FALSE);
				if (error==noErr)
				{
					/* connection end established and initialized with our options */
					*connection= connectionEnd;
				}
			}
		}
	}
	
	return error;
}

OSErr NetADSPDisposeConnectionEnd(
	ConnectionEndPtr connectionEnd)
{
	OSErr error;

	myDSPPBPtr->csCode= dspRemove; /* remove a connection end */
	myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
	myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum; /* from PBControl csCode==dspInit */
	myDSPPBPtr->u.closeParams.abort= TRUE; /* tear down the connection immediately */
	
	error= PBControl((ParmBlkPtr)myDSPPBPtr, FALSE);
	if (error==noErr)
	{
		/* free all memory allocated by the connection end */
		DisposePtr((Ptr)connectionEnd->dspCCB);
		DisposePtr(connectionEnd->dspSendQPtr);
		DisposePtr(connectionEnd->dspRecvQPtr);
		DisposePtr(connectionEnd->dspAttnBufPtr);
		DisposePtr((Ptr)connectionEnd); // oops. can't forget this.
	}

	return error;
}

/*
---------------------
NetADSPOpenConnection
---------------------

	---> ConnectionEndPtr

	<--- error

----------------------
NetADSPCloseConnection
----------------------

	---> ConnectionEndPtr
	
	<--- error

------------------------
NetADSPWaitForConnection
------------------------

	---> ConnectionEndPtr
	---> address to connect with (AddrBlock *)
	
	<--- error

----------------------------
NetADSPCheckConnectionStatus
----------------------------

	---> ConnectionEndPtr
	
	<--- connection established (boolean)
	<--- address block of remote machine (can be NULL)
*/

OSErr NetADSPOpenConnection(
	ConnectionEndPtr connectionEnd,
	AddrBlock *address)
{
	OSErr error;
	
	myDSPPBPtr->csCode= dspOpen; /* open a connection */
	myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
	myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum; /* from PBControl csCode==dspInit */
	
	myDSPPBPtr->u.openParams.remoteAddress= *address;
	myDSPPBPtr->u.openParams.filterAddress= *address; /* filter out anybody but our target address */
	myDSPPBPtr->u.openParams.ocMode= ocRequest; /* open connection mode */
	myDSPPBPtr->u.openParams.ocInterval= 0; /* default retry interval */
	myDSPPBPtr->u.openParams.ocMaximum= 0; /* default retry maximum */
	
	error= PBControl((ParmBlkPtr)myDSPPBPtr, FALSE);
	if (error==noErr)
	{
		/* successfully opened connection */
	}
	
	return error;
}

OSErr NetADSPCloseConnection(
	ConnectionEndPtr connectionEnd,
	boolean abort)
{
	OSErr error;

	myDSPPBPtr->csCode= dspClose; /* remove a connection end */
	myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
	myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum; /* from PBControl csCode==dspInit */
	myDSPPBPtr->u.closeParams.abort= abort; /* tear down the connection immediately if TRUE */
	
	error= PBControl((ParmBlkPtr)myDSPPBPtr, FALSE);
	
	return error;
}

OSErr NetADSPWaitForConnection(
	ConnectionEndPtr connectionEnd)
{
	DSPPBPtr myDSPPBPtr= &connectionEnd->pb; /* use private DSPPBPtr (this will be asynchronous) */
	AddrBlock filter_address;
	OSErr error;
	
	myDSPPBPtr->csCode= dspOpen; /* open a connection */
	myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
	myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum; /* from PBControl csCode==dspInit */
	myDSPPBPtr->ioCompletion= (ADSPCompletionUPP) NULL; /* no completion routine */
	
	filter_address.aNet= filter_address.aNode= filter_address.aSocket= 0;
	myDSPPBPtr->u.openParams.filterAddress= filter_address; /* accept connections from anybody (?) */
	myDSPPBPtr->u.openParams.ocMode= ocPassive; /* open connection mode */
	myDSPPBPtr->u.openParams.ocInterval= 0; /* default retry interval */
	myDSPPBPtr->u.openParams.ocMaximum= 0; /* default retry maximum */
	
	error= PBControl((ParmBlkPtr)myDSPPBPtr, TRUE);
	if (error==noErr)
	{
		/* we�re asynchronously waiting for a connection now; nobody had better use this
			parameter block or we�re screwed */
	}
	
	return error;
}

Boolean NetADSPCheckConnectionStatus(
	ConnectionEndPtr connectionEnd,
	AddrBlock *address)
{
	DSPPBPtr myDSPPBPtr= &connectionEnd->pb; /* use private DSPPBPtr (ocPassive call was asynchronous) */
	Boolean connectionEstablished= FALSE;
	OSErr error;
	
	/* check to make sure we�re waiting for a connection like we expect */
	
	error= myDSPPBPtr->ioResult;
	if (error!=asyncUncompleted)
	{
		if (error==noErr)
		{
			if (address) *address= myDSPPBPtr->u.openParams.remoteAddress; /* get remote address */
			connectionEstablished= TRUE; /* got one! */
		}
		else
		{
			dprintf("dspOpen(ocPassive, ...) returned %d asynchronously", error);
			
			error= NetADSPWaitForConnection(connectionEnd); /* connection failed, try again */
			if (error!=noErr) dprintf("subsequent NetADSPWaitForConnection() returned %d", error);
		}
	}
	
	return connectionEstablished;
}

/*
------------
NetADSPWrite
------------

	---> connectionEnd pointer
	---> buffer to send
	---> number of bytes to send
	
	<--- error

executed synchronously (not safe at interrupt time)
*/

OSErr NetADSPWrite(
	ConnectionEndPtr connectionEnd,
	void *buffer,
	word *count)
{
	DSPPBPtr myDSPPBPtr= &connectionEnd->pb; /* use private DSPPBPtr */
	OSErr error;

	error= myDSPPBPtr->ioResult;
	if (error==asyncUncompleted)
	{
		dprintf("found previously uncompleted PBControl call, exiting NetADSPWrite() without action");
	}
	else
	{
		if (error==noErr)
		{
			myDSPPBPtr->csCode= dspWrite; /* write data to connection */
			myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
			myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum; /* from PBControl csCode==dspInit */
			myDSPPBPtr->ioCompletion= (ADSPCompletionUPP) NULL; /* no completion routine */
			
			myDSPPBPtr->u.ioParams.reqCount= *count;
			myDSPPBPtr->u.ioParams.dataPtr= buffer;
			myDSPPBPtr->u.ioParams.eom= TRUE; /* logical end-of-message */
			myDSPPBPtr->u.ioParams.flush= TRUE; /* flush immediately */
		
			error= PBControl((ParmBlkPtr)myDSPPBPtr, FALSE);
		}
		else
		{
			dprintf("found %d in ioResult during NetADSPWrite()", error);
 		}
	}

	return error;
}

/*
-----------
NetADSPRead
-----------

	---> connectionEnd pointer
	---> buffer to receive into
	<--> number of bytes to receive (returns number of bytes actually read)
	
	<--- error

this is fundamentally broken because it doesn�t handle timeouts right now; so we can hang
waiting for data.  we should make a dspOptions call to check and see how much data is
available.
*/

OSErr NetADSPRead(
	ConnectionEndPtr connectionEnd,
	void *buffer,
	word *count)
{
	DSPPBPtr myDSPPBPtr= &connectionEnd->pb; /* use private DSPPBPtr (this will be asynchronous) */
	OSErr error;

	if (myDSPPBPtr->ioResult==asyncUncompleted)
	{
		dprintf("waiting for previous uncompleted asynchronous PBControl call in NetADSPRead()");
		while (myDSPPBPtr->ioResult==asyncUncompleted);
	}
	
	myDSPPBPtr->csCode= dspRead; /* read data from connection */
	myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
	myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum; /* from PBControl csCode==dspInit */
	myDSPPBPtr->ioCompletion= (ADSPCompletionUPP) NULL; /* no completion routine */
	
	myDSPPBPtr->u.ioParams.reqCount= *count;
	myDSPPBPtr->u.ioParams.dataPtr= buffer;
	
	error= PBControl((ParmBlkPtr)myDSPPBPtr, FALSE);
	if (error==noErr)
	{
		/* return the actual amount of data read */
		*count= myDSPPBPtr->u.ioParams.actCount;
	}
	
	return error;
}

#ifdef OBSOLETE
/*
---------------
NetDeferredRead
---------------

	---> connection end to read bytes from
	
	<--- error

sets up the given connection end to wait for LONG_MAX bytes or an EOM; will call the
NetDeferredReadCompletionRoutine() when done (to catch and forward)
*/

OSErr NetDeferredRead(
	ConnectionEndPtr connectionEnd,
	void *buffer,
	long buffer_size)
{
	DSPPBPtr myDSPPBPtr= &connectionEnd->pb; /* use private DSPPBPtr (this will be asynchronous) */
	OSErr error;

	if (myDSPPBPtr->ioResult==asyncUncompleted)
	{
		dprintf("waiting for previous uncompleted asynchronous PBControl call in NetDeferredRead()");
		while (myDSPPBPtr->ioResult==asyncUncompleted);
	}
	
	myDSPPBPtr->csCode= dspRead; /* read data from connection */
	myDSPPBPtr->ioCRefNum= dspRefNum; /* from OpenDriver('.DSP', ...) */
	myDSPPBPtr->ccbRefNum= connectionEnd->ccbRefNum; /* from PBControl csCode==dspInit */
	myDSPPBPtr->ioCompletion= DeferredReadUPP;
	
	myDSPPBPtr->u.ioParams.reqCount= buffer_size;
	myDSPPBPtr->u.ioParams.dataPtr= buffer;
	
	error= PBControl((ParmBlkPtr)myDSPPBPtr, TRUE);
	if (error==noErr)
	{
		/* the request is queued and we�ll be notified, with luck, when our data arrives */
	}
	
	return error;
}

/*
--------------------------------
NetDeferredReadCompletionRoutine
--------------------------------

there�s lots of parameter voodoo here.  this routine catches, processes, and forwards the ring
packet.
*/

#ifdef env68k
static pascal void NetDeferredReadCompletionRoutine(
	void)
{
	DSPPBPtr thePBPtr= (DSPPBPtr) get_a0(); /* get the pointer to this parameter block from a0 */
	long old_a5= set_a5(*((long*)thePBPtr-1)); /* our a5 is right under the parameter block */
#else
static void NetDeferredReadCompletionRoutine(
	DSPPBPtr thePBPtr)
{
#endif
	ConnectionEndPtr connectionEnd= (byte*)thePBPtr - (sizeof(ConnectionEnd)-sizeof(DSPParamBlock));

	/* did this read succeed? */
	if (thePBPtr->ioResult==noErr)
	{
		/* process the incoming buffer into an outgoing buffer (count is ignored) */
		long count= NetProcessIncomingBuffer(thePBPtr->u.ioParams.dataPtr, outgoing_buffer,
			thePBPtr->u.ioParams.actCount);

		/* if we�re not the server (player index zero), forward this packet immediately.  if we
			are the server then we�ll only forward this packet when our time manager task fires */
 		if (localPlayerIndex&&count) NetForward();  /* ���� */
	
		/* we got a ring packet */
		topology->ring_packet_count+= 1;
	}
	else
	{
		dprintf("NetDeferredRead() asynchronously returned %d and killed the ring.", thePBPtr->ioResult);
	}
	
#ifdef env68k
	set_a5(old_a5); /* restore whatever a5 was on enter */
#endif

	return;
}
#endif

#ifdef OBSOLETE
/*
--------------------
NetADSPAsyncComplete
--------------------

	---> ConnectionEndPtr
	
	<--- error

returns the ioResult field of the connectionEnd�s parameter block.
*/

OSErr NetADSPAsyncComplete(
	ConnectionEndPtr connectionEnd)
{
	return connectionEnd->pb.ioResult;
}
#endif

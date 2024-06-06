/*
NETWORK_NAMES.C
Sunday, June 26, 1994 5:45:49 PM
Friday, July 15, 1994 11:03:22 AM
	allocated name table entry in the system heap, to prevent problems if the name isn't 
	unregistered before the application exits. (ajr, suggested by jgj.)
*/

#include "macintosh_cseries.h"
#include "macintosh_network.h"

#ifdef mpwc
#pragma segment network
#endif

// #define MODEM_TEST

/* ---------- constants */

#define strUSER_NAME -16096

/* ---------- types */

typedef NamesTableEntry *NamesTableEntryPtr;

/* ---------- globals */

static NamesTableEntryPtr myNTEName= (NamesTableEntryPtr) NULL;

/* ---------- code */

/*
---------------
NetRegisterName
---------------

allocates and registers and entity name for the given socket; call NetUnRegisterName to unregister
the name.  only one name can be registered at a time through NetRegisterName().

	---> name (pascal string, can be NULL and will be replaced with user name)
	---> type (pascal string)
	---> socket number
	
	<--- error
*/

OSErr NetRegisterName(
	char *name,
	char *type,
	short version,
	short socketNumber)
{
	Handle user_name= (Handle)GetString(strUSER_NAME);
	MPPPBPtr myMPPPBPtr= (MPPPBPtr) NewPtrClear(sizeof(MPPParamBlock));
	Str255 adjusted_name;
	Str255 adjusted_type;
	OSErr error;

#ifdef MODEM_TEST
	error= ModemRegisterName(name, type, version, socketNumber);
#else

	assert(!myNTEName);
	// we stick it in the system heap so that if the application crashes/quits while the name
	// is registered, this pointer won't be trashed by another application (unless, of course,
	// it trashes the system heap).
	myNTEName= (NamesTableEntryPtr) NewPtrSysClear(sizeof(NamesTableEntry));
	assert(myNTEName);

	/* get user name if no object name was supplied */
	pstrcpy((char *)adjusted_name, name ? name : (user_name ? *user_name : "\p"));

	/* Calculate the adjusted type */
	{
		Str255 version_text;
		
		psprintf((char *)version_text, "%d", version);
		pstrcpy((char *)adjusted_type, (char *)type);
		pstrcat(adjusted_type, version_text);
	}

	error= MemError();
	if (error==noErr)
	{
		NBPSetNTE((Ptr)myNTEName, adjusted_name, adjusted_type, "\p*", socketNumber); /* build names table entry */

		myMPPPBPtr->NBP.nbpPtrs.ntQElPtr= (Ptr) myNTEName;
		myMPPPBPtr->NBP.parm.verifyFlag= TRUE; /* verify this name doesn�t already exist */
		myMPPPBPtr->NBP.interval= 2; /* retry every 2*8 == 16 ticks */
		myMPPPBPtr->NBP.count= 4; /* retry 4 times ( == 64 ticks) */
	
		error= PRegisterName(myMPPPBPtr, FALSE);
		
		DisposePtr((Ptr)myMPPPBPtr);
	}
#endif
	
	return error;
}

/*

-----------------
NetUnRegisterName
-----------------

	(no parameters)

deallocates and unregisters the entity name previously allocated with NetRegisterName().
*/

OSErr NetUnRegisterName(
	void)
{
	OSErr error= noErr;

#ifdef MODEM_TEST
	error= ModemUnRegisterName();
#else

	if (myNTEName)
	{
		MPPPBPtr myMPPPBPtr= (MPPPBPtr) NewPtrClear(sizeof(MPPParamBlock));

		error= MemError();
		if (error==noErr)
		{
			myMPPPBPtr->NBP.nbpPtrs.entityPtr= (Ptr) &myNTEName->nt.entityData; /* can�t just give back names table entry */
			
			error= PRemoveName(myMPPPBPtr, FALSE);
		
			DisposePtr((Ptr)myMPPPBPtr);
	
			DisposePtr((Ptr)myNTEName);
			myNTEName= (NamesTableEntryPtr) NULL;
		}
	}
#endif

	return error;
}

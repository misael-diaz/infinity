/*
	find_files.c
	Wednesday, January 11, 1995 6:44:55 PM
*/

#include "macintosh_cseries.h"
#include <string.h> /* For memset */

#include "find_files.h"

#ifdef mpwc
	#pragma segment file_io
#endif

/* --------- local prototypes */
static int alphabetical_names(const void *a, const void *b);
static OSErr enumerate_files(struct find_file_pb *param_block, long directory_id);
	
/* ---------------- Parameter Block Version */
OSErr find_files(
	struct find_file_pb *param_block)
{
	OSErr err;

	/* If we need to create the destination buffer */
	if(param_block->flags & _ff_create_buffer) 
	{
		assert(param_block->search_type==_fill_buffer);
		param_block->buffer= (FSSpec *) NewPtr(sizeof(FSSpec)*param_block->max);
	}

	/* Assert that we got it. */
	assert(param_block->search_type==_callback_only || param_block->buffer);
	assert(param_block->version==0);

	/* Set the variables */
	param_block->count= 0;

	err= enumerate_files(param_block, param_block->directory_id);

	/* Alphabetical */
	if(param_block->flags & _ff_alphabetical)
	{
		assert(param_block->search_type==_fill_buffer);
		qsort(param_block->buffer, param_block->count, 
			sizeof(FSSpec), alphabetical_names);
	}

	/* If we created the buffer, make it the exact right size */
	if(param_block->flags & _ff_create_buffer)
	{
		assert(param_block->search_type==_fill_buffer);
		SetPtrSize((Ptr) param_block->buffer, sizeof(FSSpec)*(param_block->count));
	}
	
	return err;
}

Boolean equal_fsspecs(
	FSSpec *a, 
	FSSpec *b)
{
	Boolean equal= false;
	
	if(a->vRefNum==b->vRefNum && a->parID==b->parID && 
		EqualString(a->name, b->name, false, false))
	{
		equal= true;
	}
	
	return equal;
}

/* --------- Local Code --------- */
static OSErr enumerate_files(
	struct find_file_pb *param_block, 
	long directory_id) /* Because it is recursive.. */
{
	static CInfoPBRec pb; /* static to prevent stack overflow.. */
	static FSSpec temp_file;
	static OSErr err;
	short index;

	memset(&pb, 0, sizeof(CInfoPBRec));
	
	temp_file.vRefNum= param_block->vRefNum;
	pb.hFileInfo.ioVRefNum= temp_file.vRefNum;
	pb.hFileInfo.ioNamePtr= temp_file.name;
			
	for(err= noErr, index=1; param_block->count<param_block->max && err==noErr; index++) 
	{
		pb.hFileInfo.ioDirID= directory_id;
		pb.hFileInfo.ioFDirIndex= index;

		err= PBGetCatInfo( &pb, false);
		if(!err)
		{
			if ((pb.hFileInfo.ioFlAttrib & 16) && (param_block->flags & _ff_recurse))
			{
				/* Recurse, if you really want to... */
				err= enumerate_files(param_block, pb.dirInfo.ioDrDirID);
			} else {
				/* Add.. */
				if(param_block->type_to_find==WILDCARD_TYPE || 
					pb.hFileInfo.ioFlFndrInfo.fdType==param_block->type_to_find)
				{
					temp_file.vRefNum= pb.hFileInfo.ioVRefNum;
					temp_file.parID= directory_id;
					
					/* Only add if there isn't a callback or it returns TRUE */
					switch(param_block->search_type)
					{
						case _fill_buffer:
							if(!param_block->callback || param_block->callback(&temp_file, param_block->user_data))
							{
								/* Copy it in.. */
								BlockMove(&temp_file, &param_block->buffer[param_block->count++], 
									sizeof(FSSpec));
							}
							break;
							
						case _callback_only:
							assert(param_block->callback);
							if(param_block->flags & _ff_callback_with_catinfo)
							{
								param_block->callback(&temp_file, &pb);
							} else {
								param_block->callback(&temp_file, param_block->user_data);
							}
							break;
							
						default:
							halt();
							break;
					}
				}
			}
		} else {
			/* We ran out of files.. */
		}
	}

	/* If we got a fnfErr, it was because we indexed too far. */	
	return (err==fnfErr) ? (noErr) : err;
}

static int alphabetical_names(
	const void *a,
	const void *b)
{
	return (IUCompString(((FSSpec *)a)->name, ((FSSpec *)b)->name)); 
}
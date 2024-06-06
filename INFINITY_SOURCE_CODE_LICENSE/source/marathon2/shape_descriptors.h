#ifndef __SHAPE_DESCRIPTORS
#define __SHAPE_DESCRIPTORS

/*
SHAPE_DESCRIPTORS.H
Saturday, July 9, 1994 3:24:56 PM

Saturday, July 9, 1994 6:47:04 PM
	this header file is not in the makefile, so changing it won�t result in the nearly-full
	rebuild that it should.  if you change this, which you shouldn�t, you must touch the makefile.
*/

/* ---------- types */

typedef short shape_descriptor; /* [clut.3] [collection.5] [shape.8] */

#define DESCRIPTOR_SHAPE_BITS 8
#define DESCRIPTOR_COLLECTION_BITS 5
#define DESCRIPTOR_CLUT_BITS 3

#define MAXIMUM_COLLECTIONS (1<<DESCRIPTOR_COLLECTION_BITS)
#define MAXIMUM_SHAPES_PER_COLLECTION (1<<DESCRIPTOR_SHAPE_BITS)
#define MAXIMUM_CLUTS_PER_COLLECTION (1<<DESCRIPTOR_CLUT_BITS)

/* ---------- collections */

enum /* collection numbers */
{
	_collection_interface, // 0
	_collection_weapons_in_hand, // 1
	_collection_juggernaut, // 2
	_collection_tick, // 3
	_collection_rocket, // 4
	_collection_hunter, // 5
	_collection_player, // 6
	_collection_items, // 7
	_collection_trooper, // 8
	_collection_fighter, // 9
	_collection_defender, // 10
	_collection_yeti, // 11
	_collection_civilian, // 12
	_collection_vacuum_civilian, // 13
	_collection_enforcer, // 14
	_collection_hummer, // 15
	_collection_compiler, // 16
	_collection_walls1, // 17
	_collection_walls2, // 18
	_collection_walls3, // 19
	_collection_walls4, // 20 
	_collection_walls5, // 21 
	_collection_scenery1, // 22
	_collection_scenery2, // 23
	_collection_scenery3, // 24
	_collection_scenery4, // 25 pathways
	_collection_scenery5, // 26 alien
	_collection_landscape1, // 27 day
	_collection_landscape2, // 28 night
	_collection_landscape3, // 29 moon
	_collection_landscape4, // 30 
	_collection_cyborg, // 31
	
	NUMBER_OF_COLLECTIONS
};

/* ---------- macros */

#define GET_DESCRIPTOR_SHAPE(d) ((d)&(word)(MAXIMUM_SHAPES_PER_COLLECTION-1))
#define GET_DESCRIPTOR_COLLECTION(d) (((d)>>DESCRIPTOR_SHAPE_BITS)&(word)((1<<(DESCRIPTOR_COLLECTION_BITS+DESCRIPTOR_CLUT_BITS))-1))
#define BUILD_DESCRIPTOR(collection,shape) (((collection)<<DESCRIPTOR_SHAPE_BITS)|(shape))

#define BUILD_COLLECTION(collection,clut) ((collection)|(word)((clut)<<DESCRIPTOR_COLLECTION_BITS))
#define GET_COLLECTION_CLUT(collection) (((collection)>>DESCRIPTOR_COLLECTION_BITS)&(word)(MAXIMUM_CLUTS_PER_COLLECTION-1))
#define GET_COLLECTION(collection) ((collection)&(MAXIMUM_COLLECTIONS-1))

#endif

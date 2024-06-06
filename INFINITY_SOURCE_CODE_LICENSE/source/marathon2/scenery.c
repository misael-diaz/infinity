/*
SCENERY.C
Thursday, December 1, 1994 11:56:43 AM  (Jason)

Friday, June 16, 1995 11:48:23 AM  (Jason)
	animated scenery; audible scenery.
Tuesday, October 10, 1995 10:30:58 AM  (Jason)
	destroyable scenery; new_scenery doesn�t bail on out-of-range scenery.
*/

#include "cseries.h"
#include "map.h"
#include "render.h"
#include "interface.h"
#include "flood_map.h"
#include "effects.h"
#include "monsters.h"
#include "projectiles.h"
#include "player.h"
#include "platforms.h"
#include "scenery.h"

#include <string.h>

#ifdef mpwc
#pragma segment objects
#endif

/* ---------- constants */

enum
{
	MAXIMUM_ANIMATED_SCENERY_OBJECTS= 20
};

/* ---------- globals */

#include "scenery_definitions.h"

static short animated_scenery_object_count;
static short *animated_scenery_object_indexes;

/* ---------- private prototypes */

#ifdef DEBUG
struct scenery_definition *get_scenery_definition(short scenery_type);
#else
#define get_scenery_definition(i) (scenery_definitions+(i))
#endif

/* ---------- code */

void initialize_scenery(
	void)
{
	animated_scenery_object_indexes= (short *)malloc(sizeof(short)*MAXIMUM_ANIMATED_SCENERY_OBJECTS);
	assert(animated_scenery_object_indexes);
	
	return;
}

/* returns object index if successful, NONE otherwise */
short new_scenery(
	struct object_location *location,
	short scenery_type)
{
	short object_index= NONE;
	
	if (scenery_type<NUMBER_OF_SCENERY_DEFINITIONS)
	{
		struct scenery_definition *definition= get_scenery_definition(scenery_type);
		
		object_index= new_map_object(location, definition->shape);
		if (object_index!=NONE)
		{
			struct object_data *object= get_object_data(object_index);
			
			SET_OBJECT_OWNER(object, _object_is_scenery);
			SET_OBJECT_SOLIDITY(object, (definition->flags&_scenery_is_solid) ? TRUE : FALSE);
			object->permutation= scenery_type;
		}
	}
	
	return object_index;
}

void animate_scenery(
	void)
{
	short i;
	
	for (i= 0; i<animated_scenery_object_count; ++i)
	{
		animate_object(animated_scenery_object_indexes[i]);
	}
	
	return;
}

void randomize_scenery_shapes(
	void)
{
	struct object_data *object;
	short object_index;

	animated_scenery_object_count= 0;
	
	for (object_index= 0, object= objects; object_index<MAXIMUM_OBJECTS_PER_MAP; ++object_index, ++object)
	{
		if (GET_OBJECT_OWNER(object)==_object_is_scenery)
		{
			struct scenery_definition *definition= get_scenery_definition(object->permutation);
			
			if (!randomize_object_sequence(object_index, definition->shape))
			{
				if (animated_scenery_object_count<MAXIMUM_ANIMATED_SCENERY_OBJECTS)
				{
					animated_scenery_object_indexes[animated_scenery_object_count++]= object_index;
				}
			}
		}
	}
	
	return;
}

void get_scenery_dimensions(
	short scenery_type,
	world_distance *radius,
	world_distance *height)
{
	struct scenery_definition *definition= get_scenery_definition(scenery_type);

	*radius= definition->radius;
	*height= definition->height;
	
	return;
}

void damage_scenery(
	short object_index)
{
	struct object_data *object= get_object_data(object_index);
	struct scenery_definition *definition= get_scenery_definition(object->permutation);
	
	if (definition->flags&_scenery_can_be_destroyed)
	{
		object->shape= definition->destroyed_shape;
		new_effect(&object->location, object->polygon, definition->destroyed_effect, object->facing);
		SET_OBJECT_OWNER(object, _object_is_normal);
	}
	
	return;
}

/* ---------- private code */

#ifdef DEBUG
struct scenery_definition *get_scenery_definition(
	short scenery_type)
{
	assert(scenery_type>=0 && scenery_type<NUMBER_OF_SCENERY_DEFINITIONS);
	
	return scenery_definitions + scenery_type;
}
#endif

/*
MEDIA.C
Sunday, March 26, 1995 1:13:11 AM  (Jason')
*/

#include "cseries.h"

#include "map.h"
#include "media.h"
#include "effects.h"
#include "fades.h"
#include "lightsource.h"
#include "game_sound.h"

#ifdef mpwc
#pragma segment marathon
#endif

/* ---------- macros */

#define CALCULATE_MEDIA_HEIGHT(m) ((m)->low + FIXED_INTEGERAL_PART(((m)->high-(m)->low)*get_light_intensity((m)->light_index)))

/* ---------- globals */

struct media_data *medias;

/* ---------- private prototypes */

static void update_one_media(short media_index, boolean force_update);

#ifdef DEBUG
static struct media_definition *get_media_definition(short type);
#else
#define get_media_definition(t) (media_definitions+(t))
#endif

/* ---------- globals */

struct media_data *medias;

#include "media_definitions.h"

/* ---------- code */

// light_index must be loaded
short new_media(
	struct media_data *initializer)
{
	struct media_data *media;
	short media_index;
	
	for (media_index= 0, media= medias; media_index<MAXIMUM_MEDIAS_PER_MAP; ++media_index, ++media)
	{
		if (SLOT_IS_FREE(media))
		{
			*media= *initializer;
			
			MARK_SLOT_AS_USED(media);
			
			media->origin.x= media->origin.y= 0;
			update_one_media(media_index, TRUE);
			
			break;
		}
	}
	if (media_index==MAXIMUM_MEDIAS_PER_MAP) media_index= NONE;
	
	return media_index;
}

boolean media_in_environment(
	short media_type,
	short environment_code)
{
	return collection_in_environment(get_media_definition(media_type)->collection, environment_code);
}

void update_medias(
	void)
{
	short media_index;
	struct media_data *media;
	
	for (media_index= 0, media= medias; media_index<MAXIMUM_MEDIAS_PER_MAP; ++media_index, ++media)
	{
		if (SLOT_IS_USED(media))
		{
			update_one_media(media_index, FALSE);
			
			media->origin.x= WORLD_FRACTIONAL_PART(media->origin.x + ((cosine_table[media->current_direction]*media->current_magnitude)>>TRIG_SHIFT));
			media->origin.y= WORLD_FRACTIONAL_PART(media->origin.y + ((sine_table[media->current_direction]*media->current_magnitude)>>TRIG_SHIFT));
		}
	}

	return;
}

void get_media_detonation_effect(
	short media_index,
	short type,
	short *detonation_effect)
{
	struct media_data *media= get_media_data(media_index);
	struct media_definition *definition= get_media_definition(media->type);

	if (type!=NONE)
	{
		assert(type>=0 && type<NUMBER_OF_MEDIA_DETONATION_TYPES);
		
		if (definition->detonation_effects[type]!=NONE) *detonation_effect= definition->detonation_effects[type];
	}

	return;
}

short get_media_sound(
	short media_index,
	short type)
{
	struct media_data *media= get_media_data(media_index);
	struct media_definition *definition= get_media_definition(media->type);

	assert(type>=0 && type<NUMBER_OF_MEDIA_SOUNDS);		

	return definition->sounds[type];
}

struct damage_definition *get_media_damage(
	short media_index,
	fixed scale)
{
	struct media_data *media= get_media_data(media_index);
	struct media_definition *definition= get_media_definition(media->type);
	struct damage_definition *damage= &definition->damage;

	damage->scale= scale;
		
	return (damage->type==NONE || (dynamic_world->tick_count&definition->damage_frequency)) ?
		(struct damage_definition *) NULL : damage;
}

short get_media_submerged_fade_effect(
	short media_index)
{
	struct media_data *media= get_media_data(media_index);
	struct media_definition *definition= get_media_definition(media->type);
	
	return definition->submerged_fade_effect;
}

#ifdef DEBUG
struct media_data *get_media_data(
	short media_index)
{
	struct media_data *media;
	
	vassert(media_index>=0&&media_index<MAXIMUM_MEDIAS_PER_MAP, csprintf(temporary, "media index #%d is out of range", media_index));
	
	media= medias+media_index;
	vassert(SLOT_IS_USED(media), csprintf(temporary, "media index #%d is unused", media_index));
	
	return media;
}
#endif

/* ---------- private code */

#ifdef DEBUG
static struct media_definition *get_media_definition(
	short type)
{
	assert(type>=0&&type<NUMBER_OF_MEDIA_TYPES);
	return media_definitions+type;
}
#endif

static void update_one_media(
	short media_index,
	boolean force_update)
{
	struct media_data *media= get_media_data(media_index);
	struct media_definition *definition= get_media_definition(media->type);

	/* update height */
	media->height= (media->low + FIXED_INTEGERAL_PART((media->high-media->low)*get_light_intensity(media->light_index)));

	/* update texture */	
	media->texture= BUILD_DESCRIPTOR(definition->collection, definition->shape);
	media->transfer_mode= definition->transfer_mode;

#if 0
if (force_update || !(dynamic_world->tick_count&definition->shape_frequency))
	{
		shape_descriptor texture;
		
		do
		{
			texture= BUILD_DESCRIPTOR(definition->collection, definition->shape + random()%definition->shape_count);
		}
		while (definition->shape_count>1 && texture==media->texture);
		
		media->texture= BUILD_DESCRIPTOR(definition->collection, definition->shape);
		media->transfer_mode= definition->transfer_mode;
	}
#endif
	
	return;
}

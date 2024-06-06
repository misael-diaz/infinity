/*
MOTION_SENSOR.C
Saturday, June 11, 1994 1:37:32 AM

Friday, September 16, 1994 2:04:47 PM  (Jason')
	removed all get_shape_information() calls.
Monday, December 5, 1994 9:19:55 PM  (Jason)
	flickers in magnetic environments
*/

#include "cseries.h"
#include "map.h"
#include "monsters.h"
#include "render.h"
#include "interface.h"
#include "motion_sensor.h"
#include "player.h"
#include "network_games.h"

#include <math.h>
#include <string.h>

#ifdef mpwc
#pragma segment texture
#endif

/*
??monsters which translate only on frame changes periodically drop off the motion sensor making it appear jumpy

//when entities first appear on the sensor they are initially visible
//entities are drawn one at a time, so the dark spot from one entity can cover the light spot from another
//sometimes the motion sensor shapes are not masked correctly (fixed from SHAPES.C)
//must save old points in real world coordinates because entities get redrawn when the player turns
*/

#define MAXIMUM_MOTION_SENSOR_ENTITIES 12

#define NUMBER_OF_PREVIOUS_LOCATIONS 6

#define MOTION_SENSOR_UPDATE_FREQUENCY 5
#define MOTION_SENSOR_RESCAN_FREQUENCY 15

#define MOTION_SENSOR_RANGE (8*WORLD_ONE)

#define OBJECT_IS_VISIBLE_TO_MOTION_SENSOR(o) TRUE

#define MOTION_SENSOR_SCALE 7

#define FLICKER_FREQUENCY 0xf

/* ---------- structures */

/* an entity can�t just be jerked from the array, because his signal should fade out, so we
	mark him as �being removed� and wait until his last signal fades away to actually remove
	him */
#define SLOT_IS_BEING_REMOVED_BIT 0x4000
#define SLOT_IS_BEING_REMOVED(e) ((e)->flags&(word)SLOT_IS_BEING_REMOVED_BIT)
#define MARK_SLOT_AS_BEING_REMOVED(e) ((e)->flags|=(word)SLOT_IS_BEING_REMOVED_BIT)

struct entity_data
{
	word flags; /* [slot_used.1] [slot_being_removed.1] [unused.14] */
	
	short monster_index;
	shape_descriptor shape;
	
	short remove_delay; /* only valid if this entity is being removed [0,NUMBER_OF_PREVIOUS_LOCATIONS) */
	
	point2d previous_points[NUMBER_OF_PREVIOUS_LOCATIONS];
	boolean visible_flags[NUMBER_OF_PREVIOUS_LOCATIONS];
	
	world_point3d last_location;
	angle last_facing;
};

struct region_data
{
	short x0, x1;
};

/* ---------- globals */

static short motion_sensor_player_index;

static short motion_sensor_side_length;

static struct region_data *sensor_region;
static struct entity_data *entities;
static short network_compass_state;

static shape_descriptor mount_shape, virgin_mount_shapes, compass_shapes;
static shape_descriptor alien_shapes, friendly_shapes, enemy_shapes;

static boolean motion_sensor_changed;
static long ticks_since_last_update, ticks_since_last_rescan;

/* ---------- private code */

static void precalculate_sensor_region(short side_length);

static short find_or_add_motion_sensor_entity(short monster_index);

static void erase_all_entity_blips(void);
static void draw_network_compass(void);
static void draw_all_entity_blips(void);

static void erase_entity_blip(point2d *location, shape_descriptor shape);
static void draw_entity_blip(point2d *location, shape_descriptor shape);
static void draw_or_erase_unclipped_shape(short x, short y, shape_descriptor shape, boolean draw);

static shape_descriptor get_motion_sensor_entity_shape(short monster_index);

static void clipped_transparent_sprite_copy(struct bitmap_definition *source, struct bitmap_definition *destination,
	struct region_data *region, short x0, short y0);
static void bitmap_window_copy(struct bitmap_definition *source, struct bitmap_definition *destination,
	short x0, short y0, short x1, short y1);
static void unclipped_solid_sprite_copy(struct bitmap_definition *source,
	struct bitmap_definition *destination, short x0, short y0);

/* ---------- code */

void initialize_motion_sensor(
	shape_descriptor mount,
	shape_descriptor virgin_mounts,
	shape_descriptor aliens,
	shape_descriptor friends,
	shape_descriptor enemies,
	shape_descriptor compasses,
	short side_length)
{
	mount_shape= mount;
	virgin_mount_shapes= virgin_mounts;
	enemy_shapes= enemies;
	friendly_shapes= friends;
	alien_shapes= aliens;
	compass_shapes= compasses;
	
	entities= (struct entity_data *) malloc(MAXIMUM_MOTION_SENSOR_ENTITIES*sizeof(struct entity_data));
	assert(entities);
	
	sensor_region= (struct region_data *) malloc(side_length*sizeof(struct region_data));
	assert(sensor_region);
	
	/* precalculate the sensor region */
	precalculate_sensor_region(side_length);	

	/* reset_motion_sensor() should be called before the motion sensor is used, but after it�s
		shapes are loaded (because it will do bitmap copying) */
		
	return;
}

void reset_motion_sensor(
	short player_index)
{
	struct bitmap_definition *mount, *virgin_mount;
	short i;

	motion_sensor_player_index= player_index;
	ticks_since_last_update= ticks_since_last_rescan= 0;

	get_shape_bitmap_and_shading_table(mount_shape, &mount, (void **) NULL, NONE);
	get_shape_bitmap_and_shading_table(virgin_mount_shapes, &virgin_mount, (void **) NULL, NONE);
	
	assert(mount->width==virgin_mount->width);
	assert(mount->height==virgin_mount->height);
	bitmap_window_copy(virgin_mount, mount, 0, 0, mount->width, mount->height);

	for (i= 0; i<MAXIMUM_MOTION_SENSOR_ENTITIES; ++i) MARK_SLOT_AS_FREE(entities+i);
	
	network_compass_state= _network_compass_all_off;

	return;
}

/* every ten ticks, regardless of frame rate, this will update the positions of the objects we
	are tracking (render_motion_sensor() will only be called at frame time) */
/* ticks_elapsed==NONE means force rescan now.. */
void motion_sensor_scan(
	short ticks_elapsed)
{
	struct object_data *owner_object= get_object_data(get_player_data(motion_sensor_player_index)->object_index);

	/* if we need to scan for new objects, flood around the owner monster looking for other,
		visible monsters within our range */
	if ((ticks_since_last_rescan-= ticks_elapsed)<0 || ticks_elapsed==NONE)
	{
		struct monster_data *monster;
		short monster_index;
		
		for (monster_index=0,monster=monsters;monster_index<MAXIMUM_MONSTERS_PER_MAP;++monster,++monster_index)
		{
			if (SLOT_IS_USED(monster)&&(MONSTER_IS_PLAYER(monster)||MONSTER_IS_ACTIVE(monster)))
			{
				struct object_data *object= get_object_data(monster->object_index);
				world_distance distance= guess_distance2d((world_point2d *) &object->location, (world_point2d *) &owner_object->location);
				
				if (distance<MOTION_SENSOR_RANGE && OBJECT_IS_VISIBLE_TO_MOTION_SENSOR(object))
				{
//					dprintf("found valid monster #%d", monster_index);
					find_or_add_motion_sensor_entity(object->permutation);
				}
			}
		}
		
		ticks_since_last_rescan= MOTION_SENSOR_RESCAN_FREQUENCY;
	}

	/* if we need to update the motion sensor, draw all active entities */
	if ((ticks_since_last_update-= ticks_elapsed)<0 || ticks_elapsed==NONE)
	{
		erase_all_entity_blips();
		if (dynamic_world->player_count>1) draw_network_compass();
		draw_all_entity_blips();
		
		ticks_since_last_update= MOTION_SENSOR_UPDATE_FREQUENCY;
		motion_sensor_changed= TRUE;
	}
	
	return;
}

/* the interface code will call this function and only draw the motion sensor if we return TRUE */
boolean motion_sensor_has_changed(
	void)
{
	boolean changed= motion_sensor_changed;
	
	if (changed) motion_sensor_changed= FALSE;
	
	return changed;
}

/* toggle through the ranges */
void adjust_motion_sensor_range(
	void)
{
	return;
}

/* ---------- private code */

static void draw_network_compass(
	void)
{
	short new_state= get_network_compass_state(motion_sensor_player_index);
	short difference= (new_state^network_compass_state)|new_state;
	
	if (difference&_network_compass_nw) draw_or_erase_unclipped_shape(36, 36, compass_shapes, new_state&_network_compass_nw);
	if (difference&_network_compass_ne) draw_or_erase_unclipped_shape(61, 36, compass_shapes+1, new_state&_network_compass_ne);
	if (difference&_network_compass_se) draw_or_erase_unclipped_shape(61, 61, compass_shapes+3, new_state&_network_compass_se);
	if (difference&_network_compass_sw) draw_or_erase_unclipped_shape(36, 61, compass_shapes+2, new_state&_network_compass_sw);
	
	network_compass_state= new_state;
	
	return;
}

static void erase_all_entity_blips(
	void)
{
	struct object_data *owner_object= get_object_data(get_player_data(motion_sensor_player_index)->object_index);
	struct entity_data *entity;
	short entity_index;

	/* first erase all locations where the entity changed locations and then did not change
		locations, and erase it�s last location */
	for (entity_index=0,entity=entities;entity_index<MAXIMUM_MOTION_SENSOR_ENTITIES;++entity_index,++entity)
	{
		if (SLOT_IS_USED(entity))
		{
//			dprintf("entity #%d (%p) valid", entity_index, entity);
			/* see if our monster slot is free; if it is mark this entity as being removed; of
				course this isn�t wholly accurate and we might start tracking a new monster
				which has been placed in our old monster�s slot, but we eat that chance
				without remorse */
			if (SLOT_IS_USED(monsters+entity->monster_index))
			{
				struct object_data *object= get_object_data(get_monster_data(entity->monster_index)->object_index);
				world_distance distance= guess_distance2d((world_point2d *) &object->location, (world_point2d *) &owner_object->location);
				
				/* verify that we�re still in range (and mark us as being removed if we�re not */
				if (distance>MOTION_SENSOR_RANGE || !OBJECT_IS_VISIBLE_TO_MOTION_SENSOR(object))
				{
//					dprintf("removed2");
					MARK_SLOT_AS_BEING_REMOVED(entity);
				}
			}
			else
			{
//				dprintf("removed1");
				MARK_SLOT_AS_BEING_REMOVED(entity);
			}

			/* erase the blip specified by NUMBER_OF_PREVIOUS_LOCATIONS-1 */
			if (entity->visible_flags[NUMBER_OF_PREVIOUS_LOCATIONS-1]) erase_entity_blip(&entity->previous_points[NUMBER_OF_PREVIOUS_LOCATIONS-1], entity->shape);

			/* adjust the arrays to make room for new entries */			
			memmove(entity->visible_flags+1, entity->visible_flags, (NUMBER_OF_PREVIOUS_LOCATIONS-1)*sizeof(boolean));
			memmove(entity->previous_points+1, entity->previous_points, (NUMBER_OF_PREVIOUS_LOCATIONS-1)*sizeof(point2d));
			entity->visible_flags[0]= FALSE;
				
			/* if we�re not being removed, make room for a new location and calculate it */
			if (!SLOT_IS_BEING_REMOVED(entity))
			{
				struct monster_data *monster= get_monster_data(entity->monster_index);
				struct object_data *object= get_object_data(monster->object_index);
				
				/* remember if this entity is visible or not */
				if (object->transfer_mode!=_xfer_invisibility && object->transfer_mode!=_xfer_subtle_invisibility &&
					(!(static_world->environment_flags&_environment_magnetic) || !((dynamic_world->tick_count+4*monster->object_index)&FLICKER_FREQUENCY)))
				{
					if (object->location.x!=entity->last_location.x || object->location.y!=entity->last_location.y ||
						object->location.z!=entity->last_location.z || object->facing!=entity->last_facing)
					{
						entity->visible_flags[0]= TRUE;
	
						entity->last_location= object->location;
						entity->last_facing= object->facing;
					}
				}
				
				/* calculate the 2d position on the motion sensor */
				entity->previous_points[0]= *(point2d *)&object->location;
				transform_point2d((world_point2d *)&entity->previous_points[0], (world_point2d *)&owner_object->location, NORMALIZE_ANGLE(owner_object->facing+QUARTER_CIRCLE));
				entity->previous_points[0].x>>= MOTION_SENSOR_SCALE;
				entity->previous_points[0].y>>= MOTION_SENSOR_SCALE;
			}
			else
			{
				/* erase the blip specified by entity->remove_delay */
				if (entity->visible_flags[entity->remove_delay]) erase_entity_blip(&entity->previous_points[entity->remove_delay], entity->shape);
				
				/* if this is the last point of an entity which was being removed; mark it as unused */
				if ((entity->remove_delay+= 1)>=NUMBER_OF_PREVIOUS_LOCATIONS)
				{
					MARK_SLOT_AS_FREE(entity);
				}
			}
		}
	}
	
	return;
}

static void draw_all_entity_blips(
	void)
{
	struct entity_data *entity;
	short entity_index, intensity;

	for (intensity=NUMBER_OF_PREVIOUS_LOCATIONS-1;intensity>=0;--intensity)
	{
		for (entity_index=0,entity=entities;entity_index<MAXIMUM_MOTION_SENSOR_ENTITIES;++entity_index,++entity)
		{
			if (SLOT_IS_USED(entity))
			{
				if (entity->visible_flags[intensity])
				{
					draw_entity_blip(&entity->previous_points[intensity], entity->shape + intensity);
				}
			}
		}
	}
	
	return;
}

static void draw_or_erase_unclipped_shape(
	short x,
	short y,
	shape_descriptor shape,
	boolean draw)
{
	struct bitmap_definition *mount, *virgin_mount, *blip;

	get_shape_bitmap_and_shading_table(mount_shape, &mount, (void **) NULL, NONE);
	get_shape_bitmap_and_shading_table(virgin_mount_shapes, &virgin_mount, (void **) NULL, NONE);
	get_shape_bitmap_and_shading_table(shape, &blip, (void **) NULL, NONE);
	
	draw ?
		unclipped_solid_sprite_copy(blip, mount, x, y) :
		bitmap_window_copy(virgin_mount, mount, x, y, x+blip->width, y+blip->height);
	
	return;
}

static void erase_entity_blip(
	point2d *location,
	shape_descriptor shape)
{
	struct bitmap_definition *mount, *virgin_mount, *blip;
	short x, y;
	
	get_shape_bitmap_and_shading_table(mount_shape, &mount, (void **) NULL, NONE);
	get_shape_bitmap_and_shading_table(virgin_mount_shapes, &virgin_mount, (void **) NULL, NONE);
	get_shape_bitmap_and_shading_table(shape, &blip, (void **) NULL, NONE);

	x= location->x + (motion_sensor_side_length>>1) - (blip->width>>1);
	y= location->y + (motion_sensor_side_length>>1) - (blip->height>>1);

	bitmap_window_copy(virgin_mount, mount, x, y, x+blip->width, y+blip->height);

	return;
}

static void draw_entity_blip(
	point2d *location,
	shape_descriptor shape)
{
	struct bitmap_definition *mount, *blip;
	
	get_shape_bitmap_and_shading_table(mount_shape, &mount, (void **) NULL, NONE);
	get_shape_bitmap_and_shading_table(shape, &blip, (void **) NULL, NONE);

	clipped_transparent_sprite_copy(blip, mount, sensor_region,
		location->x + (motion_sensor_side_length>>1) - (blip->width>>1),
		location->y + (motion_sensor_side_length>>1) - (blip->height>>1));
	
	return;
}

/* if we find an entity that is being removed, we continue with the removal process and ignore
	the new signal; the new entity will probably not be added to the sensor again for a full
	second or so (the range should be set so that this is reasonably hard to do) */
static short find_or_add_motion_sensor_entity(
	short monster_index)
{
	struct entity_data *entity;
	short entity_index, best_unused_index;
	
	best_unused_index= NONE;
	for (entity_index=0,entity=entities;entity_index<MAXIMUM_MOTION_SENSOR_ENTITIES;++entity_index,++entity)
	{
		if (SLOT_IS_USED(entity))
		{
			if (entity->monster_index==monster_index) break;
		}
		else
		{
			if (best_unused_index==NONE) best_unused_index= entity_index;
		}
	}

	if (entity_index==MAXIMUM_MOTION_SENSOR_ENTITIES)
	{
		/* not found; add new entity if we can */
		
		if (best_unused_index!=NONE)
		{
			struct monster_data *monster= get_monster_data(monster_index);
			struct object_data *object= get_object_data(monster->object_index);
			short i;

			entity= entities+best_unused_index;
			
			entity->flags= 0;
			entity->monster_index= monster_index;
			entity->shape= get_motion_sensor_entity_shape(monster_index);
			for (i=0;i<NUMBER_OF_PREVIOUS_LOCATIONS;++i) entity->visible_flags[i]= FALSE;
			entity->last_location= object->location;
			entity->last_facing= object->facing;
			entity->remove_delay= 0;
			MARK_SLOT_AS_USED(entity);
			
//			dprintf("new index, pointer: %d, %p", best_unused_index, entity);
		}
		
		entity_index= best_unused_index;
	}
	
	return entity_index;
}

static void precalculate_sensor_region(
	short side_length)
{
	double half_side_length= side_length/2.0;
	double r= half_side_length + 1.0;
	short i;

	/* save length for assert() during rendering */
	motion_sensor_side_length= side_length;
	
	/* precompute [x0,x1] clipping values for each y value in the circular sensor */
	for (i=0;i<side_length;++i)
	{
		double y= i - half_side_length;
		double x= sqrt(r*r-y*y);
		
		if (x>=r) x= r-1.0;
		sensor_region[i].x0= half_side_length-x;
		sensor_region[i].x1= x+half_side_length;
	}

	return;
}

/* (x0,y0) and (x1,y1) specify a window to be copied from the source to (x2,y2) in the destination.
	pixel index zero is transparent (handles clipping) */
static void bitmap_window_copy(
	struct bitmap_definition *source,
	struct bitmap_definition *destination,
	short x0,
	short y0,
	short x1,
	short y1)
{
	short count;
	short y;
	
	assert(x0<=x1&&y0<=y1);

	if (x0<0) x0= 0;
	if (y0<0) y0= 0;
	if (x1>source->width) x1= source->width;
	if (y1>source->height) y1= source->height;
	
	assert(source->width==destination->width);
	assert(source->height==destination->height);
	assert(destination->width==motion_sensor_side_length);
	assert(destination->height==motion_sensor_side_length);
	
	for (y=y0;y<y1;++y)
	{
		register pixel8 *read= source->row_addresses[y]+x0;
		register pixel8 *write= destination->row_addresses[y]+x0;
		
		for (count=x1-x0;count>0;--count) *write++= *read++;
	}
	
	return;
}

static void clipped_transparent_sprite_copy(
	struct bitmap_definition *source,
	struct bitmap_definition *destination,
	struct region_data *region,
	short x0,
	short y0)
{
	short height, y;
	
	assert(destination->width==motion_sensor_side_length);
	assert(destination->height==motion_sensor_side_length);

	y= 0;
	height= source->height;
	if (y0+height>destination->height) height= destination->height-y0;
	if (y0<0)
	{
		y= -y0;
		height+= y0;
	}

	while ((height-= 1)>=0)	
	{
		register pixel8 pixel, *read, *write;
		register short width= source->width;
		short clip_left= region[y0+y].x0, clip_right= region[y0+y].x1;
		short offset= 0;
		
		if (x0<clip_left) offset= clip_left-x0, width-= offset;
		if (x0+offset+width>clip_right) width= clip_right-x0-offset;

		assert(y>=0&&y<source->height);
		assert(y0+y>=0&&y0+y<destination->height);
		
		read= source->row_addresses[y]+offset;
		write= destination->row_addresses[y0+y]+x0+offset;
		
		while ((width-= 1)>=0)
		{
			if (pixel= *read++) *write++= pixel; else write+= 1;
		}

		y+= 1;
	}
	
	return;
}

static void unclipped_solid_sprite_copy(
	struct bitmap_definition *source,
	struct bitmap_definition *destination,
	short x0,
	short y0)
{
	short height, y;

	y= 0;
	height= source->height;

	while ((height-= 1)>=0)	
	{
		register pixel8 *read, *write;
		register short width= source->width;

		assert(y>=0&&y<source->height);
		assert(y0+y>=0&&y0+y<destination->height);
		
		read= source->row_addresses[y];
		write= destination->row_addresses[y0+y]+x0;
		
		while ((width-= 1)>=0) *write++= *read++;

		y+= 1;
	}
	
	return;
}

static shape_descriptor get_motion_sensor_entity_shape(
	short monster_index)
{
	struct monster_data *monster= get_monster_data(monster_index);
	shape_descriptor shape;
	
	if (MONSTER_IS_PLAYER(monster))
	{
		struct player_data *player= get_player_data(monster_index_to_player_index(monster_index));
		struct player_data *owner= get_player_data(motion_sensor_player_index);
		
		shape= ((player->team==owner->team && !(GET_GAME_OPTIONS()&_force_unique_teams)) || GET_GAME_TYPE()==_game_of_cooperative_play) ?
			friendly_shapes : enemy_shapes;
	}
	else
	{
		switch (monster->type)
		{
			case _civilian_crew:
			case _civilian_science:
			case _civilian_security:
			case _civilian_assimilated:
			case _vacuum_civilian_crew:
			case _vacuum_civilian_science:
			case _vacuum_civilian_security:
			case _vacuum_civilian_assimilated:
				shape= friendly_shapes;
				break;
			
			default:
				shape= alien_shapes;
				break;
		}
	}
	
	return shape;
}

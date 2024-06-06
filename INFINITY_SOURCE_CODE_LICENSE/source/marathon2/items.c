/*
ITEMS.C
Monday, January 3, 1994 10:06:08 PM

Monday, September 5, 1994 2:17:43 PM
	razed.
Friday, October 21, 1994 3:44:11 PM
	changed inventory updating mechanism, added maximum counts of items.
Wednesday, November 2, 1994 3:49:57 PM (Jason)
	object_was_just_destroyed is now called immediately on powerups.
Tuesday, January 31, 1995 1:24:10 PM  (Jason')
	can only hold unlimited ammo on total carnage (not everything)
Wednesday, October 11, 1995 3:10:34 PM  (Jason)
	network-only items
*/

#include "cseries.h"

#include "map.h"
#include "interface.h"
#include "monsters.h"
#include "player.h"
#include "game_sound.h"
#include "platforms.h"
#include "fades.h"
#include "items.h"
#include "flood_map.h"
#include "effects.h"
#include "game_window.h"
#include "weapons.h" /* needed for process_new_item_for_reloading */
#include "network_games.h"

#ifdef mpwc
#pragma segment marathon
#endif

/* ---------- structures */

#define strITEM_NAME_LIST 150
#define strHEADER_NAME_LIST 151

#define MAXIMUM_ARM_REACH (3*WORLD_ONE_FOURTH)

/* ---------- private prototypes */

/* ---------- globals */

#include "item_definitions.h"

/* ---------- private prototypes */

#ifdef DEBUG
struct item_definition *get_item_definition(short type);
#else
#define get_item_definition(i) (item_definitions+(i))
#endif

static boolean get_item(short player_index, short object_index);

static boolean test_item_retrieval(short polygon_index1, world_point3d *location1, world_point3d *location2);

static long item_trigger_cost_function(short source_polygon_index, short line_index,
	short destination_polygon_index, void *unused);

/* ---------- code */

short new_item(
	struct object_location *location,
	short type)
{
	short object_index;
	struct item_definition *definition= get_item_definition(type);
	boolean add_item= TRUE;

	assert(sizeof(item_definitions)/sizeof(struct item_definition)==NUMBER_OF_DEFINED_ITEMS);

	/* Do NOT add items that are network-only in a single player game, and vice-versa */
	if (dynamic_world->player_count>1)
	{
		if (definition->invalid_environments & _environment_network) add_item= FALSE;
		if (get_item_kind(type)==_ball && !current_game_has_balls()) add_item= FALSE;
	} 
	else
	{
		if (definition->invalid_environments & _environment_single_player) add_item= FALSE;
	}

	if (add_item)
	{
		/* add the object to the map */
		object_index= new_map_object(location, definition->base_shape);
		if (object_index!=NONE)
		{
			struct object_data *object= get_object_data(object_index);
			
			SET_OBJECT_OWNER(object, _object_is_item);
			object->permutation= type;
		
			if ((location->flags&_map_object_is_network_only) && dynamic_world->player_count<=1)
			{
//				dprintf("killed #%d;g;", type);
				SET_OBJECT_INVISIBILITY(object, TRUE);
				object->permutation= NONE;
			}
#if 1			
			else if ((get_item_kind(type)==_ball) && !static_world->ball_in_play)
			{
				static_world->ball_in_play= TRUE;
				play_local_sound(_snd_got_ball);
			}
#endif

			/* let PLACEMENT.C keep track of how many there are */
			object_was_just_added(_object_is_item, type);
 		}
	}
	else
	{
		object_index= NONE;
	}
	
	return object_index;
}

void trigger_nearby_items(
	short polygon_index)
{
	polygon_index= flood_map(polygon_index, LONG_MAX, item_trigger_cost_function, _breadth_first, (void *) NULL);
	while (polygon_index!=NONE)
	{
		struct polygon_data *polygon= get_polygon_data(polygon_index);
		struct object_data *object;
		short object_index;

		for (object_index= get_polygon_data(polygon_index)->first_object; object_index!=NONE; object_index= object->next_object)
		{
			object= get_object_data(object_index);
			switch (GET_OBJECT_OWNER(object))
			{
				case _object_is_item:
					if (OBJECT_IS_INVISIBLE(object) && object->permutation!=NONE)
					{
						teleport_object_in(object_index);
					}
					break;
			}
		}
		
		polygon_index= flood_map(NONE, LONG_MAX, item_trigger_cost_function, _breadth_first, (void *) NULL);
	}
	
	return;
}

/* returns the color of the ball or NONE if they don't have one */
short find_player_ball_color(
	short player_index)
{
	struct player_data *player= get_player_data(player_index);
	short ball_color= NONE;
	short index;

	for(index= BALL_ITEM_BASE; ball_color==NONE && index<BALL_ITEM_BASE+MAXIMUM_NUMBER_OF_PLAYERS; ++index)
	{
		if(player->items[index]>0) 
		{
			ball_color= index-BALL_ITEM_BASE;
		}
	}

	return ball_color;	
}

void get_item_name(
	char *buffer,
	short item_id,
	boolean plural)
{
	struct item_definition *definition= get_item_definition(item_id);

	getcstr(buffer, strITEM_NAME_LIST, plural ? definition->plural_name_id :
		definition->singular_name_id);
	
	return;
}

void get_header_name(
	char *buffer,
	short type)
{
	getcstr(buffer, strHEADER_NAME_LIST, type);
	
	return;
}

void calculate_player_item_array(
	short player_index,
	short type,
	short *items,
	short *counts,
	short *array_count)
{
	struct player_data *player= get_player_data(player_index);
	short loop;
	short count= 0;
	
	for(loop=0; loop<NUMBER_OF_DEFINED_ITEMS; ++loop)
	{
		if (loop==_i_knife) continue;
	 	if(player->items[loop] != NONE)
		{
			if(get_item_kind(loop)==type)
			{
				items[count]= loop;
				counts[count]= player->items[loop];
				count++;
			}
		}
	}
	
	*array_count= count;
	
	return;
}

short count_inventory_lines(
	short player_index)
{
	struct player_data *player= get_player_data(player_index);
	boolean types[NUMBER_OF_ITEM_TYPES];
	short count= 0;
	short loop;
	
	/* Clean out the header array, so we can count properly */
	for(loop=0; loop<NUMBER_OF_ITEM_TYPES; ++loop)
	{
		types[loop]= FALSE;
	}
	
	for(loop=0; loop<NUMBER_OF_DEFINED_ITEMS; ++loop)
	{
		if (loop==_i_knife) continue;
		if (player->items[loop] != NONE)
		{
			count++;
			types[get_item_kind(loop)]= TRUE;
		}
	}
	
	/* Now add in the header lines.. */
	for(loop= 0; loop<NUMBER_OF_ITEM_TYPES; ++loop)
	{
		if(types[loop]) count++;
	}
	
	return count;
}

void swipe_nearby_items(
	short player_index)
{
	struct object_data *object;
	struct object_data *player_object;
	struct player_data *player= get_player_data(player_index);
	short next_object;
	struct polygon_data *polygon;
	short *neighbor_indexes;
	short i;

	player_object= get_object_data(get_monster_data(player->monster_index)->object_index);

	polygon= get_polygon_data(player_object->polygon);
	neighbor_indexes= get_map_indexes(polygon->first_neighbor_index, polygon->neighbor_count);

	for (i=0;i<polygon->neighbor_count;++i)
	{
		struct polygon_data *neighboring_polygon= get_polygon_data(*neighbor_indexes++);
		
		if (!POLYGON_IS_DETACHED(neighboring_polygon))
		{
			next_object= neighboring_polygon->first_object;

			while(next_object != NONE)
			{
				object= get_object_data(next_object);
				if (GET_OBJECT_OWNER(object)==_object_is_item && !OBJECT_IS_INVISIBLE(object)) 
				{
					if (guess_distance2d((world_point2d *) &player->location, (world_point2d *) &object->location)<=MAXIMUM_ARM_REACH)
					{
						world_distance radius, height;
						
						get_monster_dimensions(player->monster_index, &radius, &height);
		
						if (object->location.z >= player->location.z - MAXIMUM_ARM_REACH && object->location.z <= player->location.z + height &&
							test_item_retrieval(player_object->polygon, &player_object->location, &object->location))
						{
							if(get_item(player_index, next_object))
							{
								/* Start the search again.. */
								next_object= neighboring_polygon->first_object;
								continue;
							}
						}
					}
				}
				
				next_object= object->next_object;
			}
		}
	}
	
	return;
}

void mark_item_collections(
	boolean loading)
{
	mark_collection(_collection_items, loading);
	
	return;
}

boolean unretrieved_items_on_map(
	void)
{
	boolean found_item= FALSE;
	struct object_data *object;
	short object_index;
	
	for (object_index= 0, object= objects; object_index<MAXIMUM_OBJECTS_PER_MAP; ++object_index, ++object)
	{
		if (SLOT_IS_USED(object) && GET_OBJECT_OWNER(object)==_object_is_item)
		{
			if (get_item_kind(object->permutation)==_item)
			{
				found_item= TRUE;
				break;
			}
		}
	}
	
	return found_item;
}

boolean item_valid_in_current_environment(
	short item_type)
{
	boolean valid= TRUE;
	struct item_definition *definition= get_item_definition(item_type);

	if (definition->invalid_environments & static_world->environment_flags)
	{
		valid= FALSE;
	}
	
	return valid;
}

short get_item_kind(
	short item_id)
{
	struct item_definition *definition= get_item_definition(item_id);
	
	return definition->item_kind;
}

short get_item_shape(
	short item_id)
{
	struct item_definition *definition= get_item_definition(item_id);

	return definition->base_shape;
}

boolean try_and_add_player_item(
	short player_index,
	short type) 
{
	struct item_definition *definition= get_item_definition(type);
	struct player_data *player= get_player_data(player_index);
	short grabbed_sound_index= NONE;
	boolean success= FALSE;

	switch (definition->item_kind)
	{
		case _powerup: /* powerups don�t get added to your inventory */
			if (legal_player_powerup(player_index, type))
			{
				process_player_powerup(player_index, type);
				object_was_just_destroyed(_object_is_item, type);
				grabbed_sound_index= _snd_got_powerup;
				success= TRUE;
			}
			break;
		
		case _ball:
			/* Note that you can only carry ONE ball (ever) */	
			if(find_player_ball_color(player_index)==NONE)
			{
				player->items[type]= 1;
				
				/* Load the ball weapon.. */
				process_new_item_for_reloading(player_index, _i_red_ball);
				
				/* Tell the interface to redraw next time it has to */
				mark_player_inventory_as_dirty(player_index, type);
				success= TRUE;
			}
			grabbed_sound_index= NONE;
			break;
		
		case _weapon:
		case _ammunition:
		case _item:
			/* Increment the count */	
			assert(type>=0 && type<NUMBER_OF_ITEMS);
			if(player->items[type]==NONE)
			{
				/* just got the first one.. */
				player->items[type]= 1;
				success= TRUE;
			} 
			else if(player->items[type]+1<=definition->maximum_count_per_player ||
				(dynamic_world->game_information.difficulty_level==_total_carnage_level && definition->item_kind==_ammunition))
			{
				/* Increment your count.. */
				player->items[type]++;
				success= TRUE;
			} else {
				/* You have exceeded the count of these items */
			}

			grabbed_sound_index= _snd_got_item;

			if(success)
			{
				/* Reload or whatever.. */
				process_new_item_for_reloading(player_index, type);
					
				/* Tell the interface to redraw next time it has to */
				mark_player_inventory_as_dirty(player_index, type);
			}
			break;
		
		default:
			halt();
	}
	
	/* Play the pickup sound */
	if (success && player_index==current_player_index)
	{
		play_local_sound(grabbed_sound_index);
	
		/* Flash screen */
		start_fade(_fade_bonus);
	}

	return success;
}

/* ---------- private code */

#ifdef DEBUG
struct item_definition *get_item_definition(
	short type)
{
	vassert(type>=0 && type<NUMBER_OF_DEFINED_ITEMS, csprintf(temporary, "#%d is not a valid item type.", type));
	
	return item_definitions+type;
}
#endif

static long item_trigger_cost_function(
	short source_polygon_index,
	short line_index,
	short destination_polygon_index,
	void *unused)
{
	struct polygon_data *destination_polygon= get_polygon_data(destination_polygon_index);
//	struct polygon_data *source_polygon= get_polygon_data(source_polygon_index);
//	struct line_data *line= get_line_data(line_index);
	long cost= 1;
	
	#pragma unused (unused,source_polygon_index,line_index)

	if (destination_polygon->type==_polygon_is_zone_border) cost= -1;
	
	return cost;
}

static boolean get_item(
	short player_index,
	short object_index) 
{
	struct player_data *player= get_player_data(player_index);
	struct object_data *object= get_object_data(object_index);	
	boolean success;

	assert(GET_OBJECT_OWNER(object)==_object_is_item);
	
	if (success= try_and_add_player_item(player_index, object->permutation))
	{
		/* remove it */
		remove_map_object(object_index);
	}
	
	return success;
}

static boolean test_item_retrieval(
	short polygon_index1,
	world_point3d *location1,
	world_point3d *location2)
{
	boolean valid_retrieval= TRUE;
	short polygon_index= polygon_index1;

	do
	{
		short line_index= find_line_crossed_leaving_polygon(polygon_index, (world_point2d *) location1,
			(world_point2d *) location2);
		
		if (line_index!=NONE)
		{
			polygon_index= find_adjacent_polygon(polygon_index, line_index);
			if (LINE_IS_SOLID(get_line_data(line_index))) valid_retrieval= FALSE;
			if (polygon_index!=NONE)
			{
				struct polygon_data *polygon= get_polygon_data(polygon_index);
				
				if (polygon->type==_polygon_is_platform)
				{
					struct platform_data *platform= get_platform_data(polygon->permutation);
					
					if (PLATFORM_IS_MOVING(platform)) valid_retrieval= FALSE;
				}
			}
		}
		else
		{
			polygon_index= NONE;
		}
	}
	while (polygon_index!=NONE && valid_retrieval);
	
	return valid_retrieval;
}

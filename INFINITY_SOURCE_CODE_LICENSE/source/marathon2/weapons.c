/*

	weapons.c
	Saturday, May 13, 1995 4:41:04 PM- rdm created.
		Recreating to fix all the annoying problems.
*/

#include "cseries.h"
#include "map.h"
#include "projectiles.h"
#include "player.h"
#include "weapons.h"
#include "game_sound.h"
#include "interface.h"
#include "items.h"
#include "monsters.h"
#include "game_window.h"

#include <string.h>

#include "weapon_definitions.h"

#ifdef mpwc
	#pragma segment weapons
#endif

// To Do:
// lowering second weapon on ammo empty flubs.

/* ------------- enums */
enum /* weapon states */
{
	_weapon_idle, /* if weapon_delay is non-zero, the weapon cannot be fired again yet */
	_weapon_raising, /* weapon is rising to idle position */
	_weapon_lowering, /* weapon is lowering off the screen */
	_weapon_charging, /* Weapon is charging to fire.. */
	_weapon_charged, /* Ready to fire.. */
	_weapon_firing, /* in firing animation */
	_weapon_recovering, /* Weapon is recovering from firing. */
	_weapon_awaiting_reload, /* About to start reload sequence */
	_weapon_waiting_to_load, /* waiting to actually put bullets in */
	_weapon_finishing_reload, /* finishing the reload */

	_weapon_lowering_for_twofisted_reload,	/* lowering so the other weapon can reload */
	_weapon_awaiting_twofisted_reload, /* waiting for other to lower.. */
	_weapon_waiting_for_twofist_to_reload, /* we are offscreen, waiting for the other to finish its load */
	_weapon_sliding_over_to_second_position, /* pistol is going across when the weapon is present */
	_weapon_sliding_over_from_second_position, /* Pistol returning to center of screen.. */
	_weapon_waiting_for_other_idle_to_reload, /* Pistol awaiting friend's idle.. */
	NUMBER_OF_WEAPON_STATES
};

enum {
	_trigger_down= 0x0001,
	_primary_weapon_is_up= 0x0002,
	_secondary_weapon_is_up= 0x0004,
	_wants_twofist= 0x0008,
	_flip_state= 0x0010
};

enum {
	_weapon_type= 0,
	_shell_casing_type,
	NUMBER_OF_DATA_TYPES
};

enum { /* For the flags */ /* [11.unused 1.horizontal 1.vertical 3.unused] */
	_flip_shape_horizontal= 0x08,
	_flip_shape_vertical= 0x10
};

#define PRIMARY_WEAPON_IS_VALID(wd) ((wd)->flags & _primary_weapon_is_up)
#define SECONDARY_WEAPON_IS_VALID(wd) ((wd)->flags & _secondary_weapon_is_up)
#define SET_PRIMARY_WEAPON_IS_VALID(wd, v)  ((v) ? ((wd)->flags |= _primary_weapon_is_up) : ((wd)->flags &= ~_primary_weapon_is_up))
#define SET_SECONDARY_WEAPON_IS_VALID(wd, v)  ((v) ? ((wd)->flags |= _secondary_weapon_is_up) : ((wd)->flags &= ~_secondary_weapon_is_up))

#define SET_WEAPON_WANTS_TWOFIST(wd, v)  ((v) ? ((wd)->flags |= _wants_twofist) : ((wd)->flags &= ~_wants_twofist))
#define WEAPON_WANTS_TWOFIST(wd) ((wd)->flags & _wants_twofist)

#define TRIGGER_IS_DOWN(wd) ((wd)->flags & _trigger_down)
#define SET_TRIGGER_DOWN(wd, v)  ((v) ? ((wd)->flags |= _trigger_down) : ((wd)->flags &= ~_trigger_down))

#define GET_WEAPON_VARIANCE_SIGN(wd) (((wd)->flags & _flip_state) ? (1) : (-1))
#define FLIP_WEAPON_VARIANCE_SIGN(wd) (((wd)->flags & _flip_state) ? ((wd)->flags &= ~_flip_state) : ((wd)->flags |= _flip_state))

#define PISTOL_SEPARATION_WIDTH (FIXED_ONE/4)
#define AUTOMATIC_STILL_FIRING_DURATION (4)
#define FIRING_BEFORE_SHELL_CASING_SOUND_IS_PLAYED (TICKS_PER_SECOND/2)
#define COST_PER_CHARGED_WEAPON_SHOT 4					
#define ANGULAR_VARIANCE (32)

enum
{
	MAXIMUM_SHELL_CASINGS= 4
};

enum // shell casing flags
{
	_shell_casing_is_reversed= 0x0001
};
#define SHELL_CASING_IS_REVERSED(s) ((s)->flags&_shell_casing_is_reversed)

/* ----------- structures */
struct trigger_data {
	short state, phase;
	short rounds_loaded;
	short shots_fired, shots_hit;
	short ticks_since_last_shot; /* used to play shell casing sound, and to calculate arc for shell casing drawing... */
	short ticks_firing; /* How long have we been firing? (only valid for automatics) */
	word sequence; /* what step of the animation are we in? (NOT guaranteed to be in sync!) */
};

struct weapon_data {
	short weapon_type; /* stored here to make life easier.. */
	word flags;
	word unused; /* non zero-> weapon is powered up */
	struct trigger_data triggers[NUMBER_OF_TRIGGERS];
};

struct shell_casing_data
{
	short type;
	short frame;

	word flags;

	fixed x, y;
	fixed vx, vy;
};

struct player_weapon_data {
	short current_weapon;
	short desired_weapon;
	struct weapon_data weapons[NUMBER_OF_WEAPONS];
	struct shell_casing_data shell_casings[MAXIMUM_SHELL_CASINGS];
};

/* ------------- globals */
/* The array of player weapon states */
static struct player_weapon_data *player_weapons_array;

/* ------------- macros */
#define get_maximum_number_of_players() (MAXIMUM_NUMBER_OF_PLAYERS)
#define BUILD_WEAPON_IDENTIFIER(weapon, trigger) (weapon<<1+trigger)
#define GET_WEAPON_FROM_IDENTIFIER(identifier) (identifier>>1)
#define GET_TRIGGER_FROM_IDENTIFIER(identifier) (identifier&1)

/* -------------- accessors */
#ifdef DEBUG
static struct player_weapon_data *get_player_weapon_data(
	short player_index)
{
	assert(player_index>=0 && player_index<get_maximum_number_of_players());
	return (player_weapons_array+player_index);
}

static struct weapon_definition *get_weapon_definition(
	short weapon_type)
{
	assert(weapon_type>=0 && weapon_type<NUMBER_OF_WEAPONS);
	return weapon_definitions+weapon_type;
}

static struct shell_casing_definition *get_shell_casing_definition(
	short type)
{
	assert(type>=0&&type<NUMBER_OF_SHELL_CASING_TYPES);
	return shell_casing_definitions+type;
}
#else
#define get_player_weapon_data(index) (player_weapons_array+(index))
#define get_weapon_definition(index) (weapon_definitions+(index))
#define get_shell_casing_definition(index) (shell_casing_definitions+(index))
#endif

/* ------------- local prototypes */
static void reset_trigger_data(short player_index, short weapon_type, short which_trigger);
static boolean weapon_works_in_current_environment(short weapon_index);
static void select_next_best_weapon(short player_index);
static struct trigger_data *get_player_trigger_data(short player_index, 
	short which_trigger);
struct trigger_data *get_trigger_data(short player_index, short weapon_index, 
	short which_trigger);
static struct weapon_data *get_player_current_weapon(short player_index);
static void fire_weapon(short player_index, short which_trigger,
	fixed charged_amount, boolean flail_wildly);
static struct trigger_definition *get_trigger_definition(short player_index, short which_weapon, 
	short which_trigger);
static boolean should_switch_to_weapon(short player_index, short new_weapon);
static boolean ready_weapon(short player_index, short weapon_index);
struct weapon_definition *get_current_weapon_definition(short player_index);
static boolean reload_weapon(short player_index, short which_trigger);
static struct trigger_definition *get_player_trigger_definition(short player_index,
	short which_trigger);
static boolean handle_trigger_down(short player_index, short which_trigger);
static boolean handle_trigger_up(short player_index, short which_trigger);
static void put_rounds_into_weapon(short player_index, short which_weapon, short which_trigger);
static void blow_up_player(short player_index);
static void select_next_weapon(short player_index, boolean forward);
static void calculate_weapon_position_for_idle(short player_index, short count, short weapon_type,
	fixed *height, fixed *width);
static void add_random_flutter(fixed flutter_base, fixed *height, fixed *width);
static void calculate_weapon_origin_and_vector(short player_index, short which_trigger,
	world_point3d *origin, world_point3d *vector, short *origin_polygon, angle delta_theta);
static void play_weapon_sound(short player_index, short sound, fixed pitch);
static boolean player_weapon_has_ammo(short player_index, short weapon_index);
static void lower_weapon(short player_index, short weapon_index);
static void raise_weapon(short player_index, short weapon_index);
static boolean check_reload(short player_index, short which_trigger);
static short get_active_trigger_count_and_states(short player_index,
	short weapon_index, long action_flags, short *first_trigger, boolean *triggers_down);
static boolean dual_function_secondary_has_control(short player_index);
static void calculate_ticks_from_shapes(void);
static void update_sequence(short player_index, short which_trigger);
static void update_automatic_sequence(short player_index, short which_trigger);
static boolean get_weapon_data_type_for_count(short player_index, short count, short *type, 
	short *index, short *flags);
static void update_player_ammo_count(short player_index);
static boolean player_has_valid_weapon(short player_index);
static void idle_weapon(short player_index);
static void test_raise_double_weapon(short player_index, long *action_flags);
static void modify_position_for_two_weapons(short player_index, short count, fixed *width, fixed *height);
static void change_to_desired_weapon(short player_index);
static void destroy_current_weapon(short player_index);
static void initialize_shell_casings(short player_index);
static short new_shell_casing(short player_index, short type, word flags);
static void update_shell_casings(short player_index);
static boolean get_shell_casing_display_data(struct weapon_display_information *display, short index);
static boolean automatic_still_firing(short player_index, short which_trigger);
static void	play_shell_casing_sound(short player_index, short sound_index);
static short find_weapon_power_index(short weapon_type);

#ifdef DEBUG
	static void debug_weapon(short index);
	static void debug_trigger_data(short weapon_type, short which_trigger);
#endif

/* ------------ code starts */
void initialize_weapon_manager(
	void)
{
	player_weapons_array= (struct player_weapon_data *) malloc(MAXIMUM_NUMBER_OF_PLAYERS*sizeof(struct player_weapon_data));
	assert(player_weapons_array);

	memset(player_weapons_array, 0, MAXIMUM_NUMBER_OF_PLAYERS*sizeof(struct player_weapon_data));

#if 0
{
	short index;
	
	for(index= 0; index<NUMBER_OF_WEAPONS; ++index)
	{
		struct weapon_definition *definition= get_weapon_definition(index);
		definition->ready_ticks *= 10;
	}
}
#endif
	
	return;
}

void initialize_player_weapons_for_new_game(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

	/* Clear the shots fired and all that jazz */
	memset(player_weapons, 0, sizeof(struct player_weapon_data));
	
	/* initialize the weapons to known states. */
	initialize_player_weapons(player_index);
	initialize_shell_casings(player_index);
}

/* initialize the given players weapons-> called after creating a player */
void initialize_player_weapons(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	struct player_data *player= get_player_data(player_index);
	short weapon_type;
	
	for(weapon_type= 0; weapon_type<NUMBER_OF_WEAPONS; ++weapon_type)
	{
		short which_trigger;

		for(which_trigger= 0; which_trigger<NUMBER_OF_TRIGGERS; ++which_trigger)
		{
			reset_trigger_data(player_index, weapon_type, which_trigger);
		}
	
		/* We _could_ figure this out, but this is easier.. */
		player_weapons->weapons[weapon_type].weapon_type= weapon_type;
		player_weapons->weapons[weapon_type].flags= 0;
		player_weapons->weapons[weapon_type].unused= 0;
	}
	
	/* Reset the current and desired weapon.. */
	player_weapons->current_weapon= NONE;
	player_weapons->desired_weapon= NONE;
	
	/* Reset the player crap.. */
	player->weapon_intensity= NATURAL_LIGHT_INTENSITY;
	player->weapon_intensity_decay= 0;
}

/* Mark the weapon collections for loading or unloading.. */
void mark_weapon_collections(
	boolean loading)
{
	short index;
	
	for(index= 0; index<NUMBER_OF_WEAPONS; ++index)
	{
		struct weapon_definition *definition= get_weapon_definition(index);
		
		/* Mark the weapon�s collection */	
		loading ? mark_collection_for_loading(definition->collection) : 
			mark_collection_for_unloading(definition->collection);

		/* Mark the projectile�s collection, NONE is handled correctly */
if(index != _weapon_ball)
{
		mark_projectile_collections(definition->weapons_by_trigger[_primary_weapon].projectile_type, loading);
		mark_projectile_collections(definition->weapons_by_trigger[_secondary_weapon].projectile_type, loading);
}
	}
	
	return;
}

void player_hit_target(
	short player_index,
	short weapon_identifier)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	short weapon_id, trigger;
	
	weapon_id= GET_WEAPON_FROM_IDENTIFIER(weapon_identifier);
	trigger= GET_TRIGGER_FROM_IDENTIFIER(weapon_identifier);
	
	assert(weapon_id>=0 && weapon_id<NUMBER_OF_WEAPONS);
	player_weapons->weapons[weapon_id].triggers[trigger].shots_hit++;

	return;
}

/* Called on entry to a level, and will change weapons if this one doesn't work */
/*  in the given environment. */
void check_player_weapons_for_environment_change(
	void)
{
	short player_index;
	
	for(player_index=0; player_index<dynamic_world->player_count; ++player_index)
	{
		if(player_has_valid_weapon(player_index))
		{
			struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

			if(!weapon_works_in_current_environment(player_weapons->current_weapon))
			{
				/* Change out of the current weapon immediately.. */
				player_weapons->current_weapon= NONE;
			
				/* Select the next best weapon for this person.. */
				select_next_best_weapon(player_index);
			}
		}
	}

	/* while we are at it, setup the definitions for the weapons */
	calculate_ticks_from_shapes();

#if 0
{
	struct weapon_definition *definition= get_weapon_definition(_weapon_shotgun);
	definition->ready_ticks *= 10;
}
#endif
}

/* Called when a player dies to discharge the weapons that they have charged up. */
void discharge_charged_weapons(
	short player_index)
{
	struct trigger_data *weapon;
	short which_trigger;

	if(player_has_valid_weapon(player_index))
	{
		short first_trigger, trigger_count;
		struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	
		trigger_count= get_active_trigger_count_and_states(player_index, player_weapons->current_weapon,
			0l, &first_trigger, NULL);
		for(which_trigger= first_trigger; which_trigger<trigger_count; ++which_trigger)
		{
			weapon= get_player_trigger_data(player_index, which_trigger);
			if(weapon->state==_weapon_charged)
			{
				/* Fire the weapon! (give it to them fully charged..) */
				fire_weapon(player_index, which_trigger, FIXED_ONE, TRUE);
			}
		}
	}
}

/* When the player runs over an item, check for reloads, etc. */
void process_new_item_for_reloading(
	short player_index, 
	short item_type)
{
	short weapon_type;
	
	/* Is this a weapon? */
	switch(get_item_kind(item_type))
	{
		case _ball:
		case _weapon:
			for(weapon_type= 0; weapon_type<NUMBER_OF_WEAPONS; ++weapon_type)
			{
				struct weapon_definition *definition= get_weapon_definition(weapon_type);
				
				/* If this is actually a weapon.. */
				if(definition->item_type==item_type)
				{
					short which_trigger, first_trigger, trigger_count;
					struct player_data *player= get_player_data(player_index);

					/* Load the weapons */
					if(definition->weapon_class==_twofisted_pistol_class)
					{
						assert(definition->item_type>=0 && definition->item_type<NUMBER_OF_ITEMS);
						if(player->items[definition->item_type]>1)
						{
							/* Just load the secondary one.. */
							first_trigger= _secondary_weapon;
							trigger_count= 2;
							
							/* Go ahead and mark the weapon display as dirty (because it will be) */
							update_player_ammo_count(player_index);
							if(player_index==current_player_index) mark_weapon_display_as_dirty();
						} else {
							first_trigger= _primary_weapon;
							trigger_count= 1;
						}
					} else {
						/* They aren't twofisted pistols */
						first_trigger= _primary_weapon;
						switch(definition->weapon_class)
						{
							case _normal_class:
								trigger_count= 1;
								break;
								
							case _dual_function_class:			
							case _melee_class:
							case _multipurpose_class:
								trigger_count= 2;
								break;
								
							case _twofisted_pistol_class:
							default:
								halt();
								break;
						}
					}

					/* Reset the trigger data.. */					
					for(which_trigger= first_trigger; which_trigger<trigger_count; ++which_trigger)
					{
						struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
						struct trigger_definition *trigger_definition= 
							get_trigger_definition(player_index, weapon_type, which_trigger);

						/* Reset it. */
						reset_trigger_data(player_index, weapon_type, which_trigger);
							
						if(definition->flags & _weapon_has_random_ammo_on_pickup)
						{
							short rounds_given;
		
							rounds_given= trigger_definition->rounds_per_magazine/2 + 
								(random()%(trigger_definition->rounds_per_magazine/2+1));
							player_weapons->weapons[weapon_type].triggers[which_trigger].rounds_loaded= 
								rounds_given;
								
							/* Sync the weapons ammo amounts.. */
							if(definition->flags & _weapon_triggers_share_ammo)
							{
								player_weapons->weapons[weapon_type].triggers[!which_trigger].rounds_loaded= 
									rounds_given;
							}
						} else {
							player_weapons->weapons[weapon_type].triggers[which_trigger].rounds_loaded= 
								trigger_definition->rounds_per_magazine;
						}
					}
		
					if(should_switch_to_weapon(player_index, weapon_type))
					{
						if(!ready_weapon(player_index, weapon_type))
						{
							dprintf("Error! Unable to ready something I should: %d weapon: %d;g",
								player_index, weapon_type);
						}
					}
					break; /* Out of the for loop */
				}
			}
			assert(weapon_type!=NUMBER_OF_WEAPONS);
			break;

		case _ammunition:
			/* Don't try to reload if the player has an invalid weapon */
			if(player_has_valid_weapon(player_index))
			{
				for(weapon_type= 0; weapon_type<NUMBER_OF_WEAPONS; ++weapon_type)
				{
					struct weapon_definition *definition= get_weapon_definition(weapon_type);
					short which_trigger, first_trigger, trigger_count;
				
					trigger_count= get_active_trigger_count_and_states(player_index, weapon_type, 0l, &first_trigger, NULL);
					for(which_trigger= first_trigger; which_trigger<trigger_count; ++which_trigger)
					{
						struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
						struct trigger_definition *trigger_definition= 
							get_trigger_definition(player_index, weapon_type, which_trigger);
			
						if(trigger_definition->ammunition_type==item_type && 
							weapon_type==player_weapons->current_weapon &&
							player_weapons->current_weapon==player_weapons->desired_weapon)
						{
							struct weapon_data *weapon= get_player_current_weapon(player_index);
		
							/* reload it. */
							if(weapon->triggers[which_trigger].state==_weapon_idle &&
								weapon->triggers[which_trigger].rounds_loaded==0)
							{
								if(!reload_weapon(player_index, which_trigger))
								{
									dprintf("Error reloading??!");
								}
							}
						}
					}
				}
			}
			break;

		default:
			break;
	}

	return;
}

#define IDLE_PHASE_COUNT 1000 // doesn't matter
#define CHARGED_WEAPON_OVERLOAD (60*TICKS_PER_SECOND)
#define WEAPON_FORWARD_DISPLACEMENT (WORLD_ONE_FOURTH/2)
#define WEAPON_SHORTED_SOUND NONE

/* Update the given player's weapons */
void update_player_weapons(
	short player_index, 
	long action_flags)
{
	update_shell_casings(player_index);

	if(player_has_valid_weapon(player_index))
	{
		struct player_data *player= get_player_data(player_index);
		struct weapon_data *weapon= get_player_current_weapon(player_index);
		struct weapon_definition *definition= get_current_weapon_definition(player_index);
		short which_trigger, trigger_count, first_trigger;
		boolean triggers_down[NUMBER_OF_TRIGGERS];

		/* Did they want to raise a second weapon? */
		test_raise_double_weapon(player_index, &action_flags);
		
		/* Calculate how many triggers we have to deal with.. */	
		trigger_count= get_active_trigger_count_and_states(player_index, weapon->weapon_type, 
			action_flags, &first_trigger, triggers_down);
	
		/* twiddle with their weapon_intensity decay */		
		if (player->weapon_intensity_decay>0)
		{
			if(definition->firing_intensity_decay_ticks)
			{
				player->weapon_intensity= NATURAL_LIGHT_INTENSITY + 
					((definition->firing_light_intensity-NATURAL_LIGHT_INTENSITY)*
					player->weapon_intensity_decay)/definition->firing_intensity_decay_ticks;
				player->weapon_intensity_decay--;
			} else {
				player->weapon_intensity_decay= 0;
			}
		}
	
		// wheee!!!
		for(which_trigger= first_trigger; which_trigger<trigger_count; ++which_trigger)
		{
			struct trigger_data *trigger= get_player_trigger_data(player_index, which_trigger);
	
			/* Twiddle the sequence (done after we change our states..) */
			/* This has to be in front of the trigger stuff, otherwise you won't see the first */
			/* tick of the firing animation frame.  But it also needs to be after the state change */
			/*  so that you don't wrap on the reloading sequence.  This is fixed by special casing */
			/*  the loading such that it doesn't update on the last tick. */
			update_sequence(player_index, which_trigger);
	
			/* Check for trigger down states before entering the state machine. */
			if(triggers_down[which_trigger])
			{
				handle_trigger_down(player_index, which_trigger);
			} else {
				handle_trigger_up(player_index, which_trigger);
			}
	
			/* Increment ticks since last shot, decrement the phase. */
			trigger->ticks_since_last_shot++;		
			trigger->phase--;

			/* Update the ticks firing */
			if(which_trigger==_primary_weapon && 
				(definition->flags & _weapon_is_automatic) && 
				(trigger->state==_weapon_firing || trigger->state==_weapon_recovering || 
				(trigger->state==_weapon_idle && automatic_still_firing(player_index, _primary_weapon))))
			{
				trigger->ticks_firing++;
			} else {
				trigger->ticks_firing= 0;
			}

			/* If we had a state change.. */
			if(trigger->phase<=0)
			{
				switch(trigger->state)
				{
					case _weapon_waiting_to_load:
						/* This is th point that you actually load the wepaon */
						if(definition->finish_loading_ticks)
						{
							trigger->state= _weapon_finishing_reload;
							trigger->phase= definition->finish_loading_ticks;
						} else {
							/* Don't go to a state with a zero duration */
							trigger->state= _weapon_idle;
							trigger->sequence= 0;
							trigger->phase= IDLE_PHASE_COUNT;
						}
						
						/* Bracketed for optimization (stupidity) gains. */
						{
							struct player_weapon_data *player_weapons= 
								get_player_weapon_data(player_index);
	
							 put_rounds_into_weapon(player_index, player_weapons->current_weapon,
								which_trigger);
						}
						break;
					
					case _weapon_raising:
						trigger->phase= IDLE_PHASE_COUNT; // doesn't matter.
						trigger->state= _weapon_idle;
						trigger->sequence= 0;
						check_reload(player_index, which_trigger);
						break;
						
					case _weapon_finishing_reload:
					case _weapon_idle:
					case _weapon_sliding_over_to_second_position:
					case _weapon_sliding_over_from_second_position:
						trigger->phase= IDLE_PHASE_COUNT; // doesn't matter.
						trigger->state= _weapon_idle;
						trigger->sequence= 0;
						break;
	
					case _weapon_recovering:
						trigger->phase= IDLE_PHASE_COUNT; // doesn't matter.
						trigger->state= _weapon_idle;
						trigger->sequence= 0;

						check_reload(player_index, which_trigger);
						break;

					case _weapon_waiting_for_other_idle_to_reload:
						/* This is a twofisted weapon, waiting for its friend to be idle so it can */
						/*  load.. */
						{
							struct trigger_data *other_trigger= get_player_trigger_data(player_index, !which_trigger);
				
							if(other_trigger->state==_weapon_idle)
							{
								trigger->state= _weapon_awaiting_twofisted_reload;
								trigger->phase= definition->ready_ticks;
								other_trigger->state= _weapon_lowering_for_twofisted_reload;
								other_trigger->phase= definition->ready_ticks;
							} else {
								/* Try again next time.. */
								trigger->phase= 0;
							}
						}
						break;
						
					case _weapon_lowering:
						if(definition->item_type==_i_red_ball)
						{
							short ball_color= find_player_ball_color(player_index);
							world_point3d origin, vector;
							short origin_polygon, item_type;

							assert(ball_color!=NONE);
							item_type= ball_color+BALL_ITEM_BASE;

							calculate_weapon_origin_and_vector(player_index, _primary_weapon, 
								&origin, &vector, &origin_polygon, 0);

							drop_the_ball(&origin, origin_polygon, player->monster_index, 
								_monster_marine, item_type);
							destroy_current_weapon(player_index);
						} 

						if(which_trigger==_primary_weapon)
						{
							struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

							if(trigger_count==1 && player_weapons->current_weapon != player_weapons->desired_weapon)
							{
								change_to_desired_weapon(player_index);
							}
							
							/* Reset for the next time.. */
							SET_PRIMARY_WEAPON_IS_VALID(weapon, FALSE);
// dprintf("prim down;g");
						} else {
							struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
							
							if(player_weapons->current_weapon!=player_weapons->desired_weapon)
							{
								change_to_desired_weapon(player_index);
							}

							/* Lowering the second weapon for a twofisted weapon */
							/* Reset for the next time.. */
							SET_SECONDARY_WEAPON_IS_VALID(weapon, FALSE);
// dprintf("second down;g");
						}

						/* Reset to idle to be consistent */
						trigger->phase= IDLE_PHASE_COUNT;
						trigger->state= _weapon_idle;
						trigger->sequence= 0;
						break;
						
					case _weapon_charging:
						trigger->phase= CHARGED_WEAPON_OVERLOAD;
						trigger->state= _weapon_charged;
						break;
						
					case _weapon_charged:
						if(definition->flags & _weapon_overloads)
						{
							/* just in case we don't kill them.. */
							trigger->phase= IDLE_PHASE_COUNT; // doesn't matter.
							trigger->state= _weapon_idle;
							trigger->sequence= 0;

							/* oops. */
							blow_up_player(player_index);
						} else {
							/* Reenter the _weapon_charged state. */
							trigger->phase= CHARGED_WEAPON_OVERLOAD;
							trigger->state= _weapon_charged;
						}
						break;
					
					case _weapon_firing:
						{
							struct trigger_definition *trigger_definition= 
								get_player_trigger_definition(player_index, which_trigger);
	
							if(trigger_definition->recovery_ticks==0)
							{
								trigger->state= _weapon_idle;
								trigger->phase= 0;

								if(which_trigger==_primary_weapon && TRIGGER_IS_DOWN(weapon) && (definition->flags & _weapon_is_automatic))
								{
									/* Don't reset the sequence for the amount of time we consider to be still firing... */
									trigger->phase= AUTOMATIC_STILL_FIRING_DURATION;
// dprintf("Ticks firing: %d;g", trigger->ticks_firing);
								} else {
									trigger->sequence= 0;
								}
								
								/* Check reload state & phase take precedence over the AUTOMATIC_STILL_FIRING stuff. */
								check_reload(player_index, which_trigger);
							} else {
								trigger->state= _weapon_recovering;
								trigger->phase= trigger_definition->recovery_ticks;
							}
						}
						break;
											
					case _weapon_awaiting_reload:
						/* enter the state where we wait to actually put the bullets in */
						assert(definition->loading_ticks);
						trigger->state= _weapon_waiting_to_load;
						trigger->phase= definition->loading_ticks;
						break;

					case _weapon_lowering_for_twofisted_reload:
						/* We are off the screen, and waiting for the other guy to finish loading */
						trigger->state= _weapon_waiting_for_twofist_to_reload;
						trigger->phase= definition->await_reload_ticks+definition->loading_ticks+
							definition->finish_loading_ticks;
						break;
						
					case _weapon_awaiting_twofisted_reload:
						/* The other guy is off the screen, we can load now.. */
						if(definition->await_reload_ticks)
						{
							trigger->state= _weapon_awaiting_reload;
							trigger->phase= definition->await_reload_ticks; // should this be by trigger?
						} else {
							trigger->state= _weapon_waiting_to_load;
							trigger->phase= definition->loading_ticks; // should this be by trigger?
						}
						break;
					
					case _weapon_waiting_for_twofist_to_reload:
						/* Raise back up.. */
						trigger->state= _weapon_raising;
						trigger->phase= definition->ready_ticks;
						break;
					
					default:
						vhalt(csprintf(temporary, "What the hell is state: %d?", trigger->state));
						break;
				}
			}
		}
	}
	
	/* If they didn't just finish switching weapons.. */
	if(action_flags & _cycle_weapons_forward)
	{
		select_next_weapon(player_index, TRUE);
	} 
		
	/* Cycle the weapon backward */
	if(action_flags & _cycle_weapons_backward)
	{
		select_next_weapon(player_index, FALSE);
	}

	/* And switch the weapon.. */
	idle_weapon(player_index);

// dprintf("done;g");
	return;
}

short get_player_desired_weapon(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	
	return player_weapons->desired_weapon;
}

short get_player_weapon_ammo_count(
	short player_index, 
	short which_weapon,
	short which_trigger)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	struct weapon_definition *definition= get_weapon_definition(which_weapon);
	short rounds_loaded;
	struct player_data *player= get_player_data(player_index);
	
	assert(which_weapon>=0 && which_weapon<NUMBER_OF_WEAPONS);
	assert(which_trigger>=0 && which_trigger<NUMBER_OF_TRIGGERS);
	
	switch(definition->weapon_class)
	{
		case _melee_class:
		case _normal_class:
		case _dual_function_class:
		case _multipurpose_class:
			rounds_loaded= player_weapons->weapons[which_weapon].triggers[which_trigger].rounds_loaded;
			break;

		case _twofisted_pistol_class:
			if(player->items[definition->item_type]<=1 && which_trigger==_secondary_weapon)
			{
				rounds_loaded= NONE;
			} else {
				rounds_loaded= player_weapons->weapons[which_weapon].triggers[which_trigger].rounds_loaded;
			}
			break;
			
		default:
			halt();
			break;		
	}
	
	return rounds_loaded;
}

#ifdef DEBUG
void debug_print_weapon_status(
	void)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(current_player_index);
	short index;
	
	dprintf("Current: %d Desired: %d;g", player_weapons->current_weapon, player_weapons->desired_weapon);
	for(index= 0; index<NUMBER_OF_WEAPONS; ++index)
	{
		debug_weapon(index);
	}
}

static void debug_weapon(
	short index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(current_player_index);

	dprintf("Weapon: %d;g", index);
	dprintf("weapon_type: %d flags: %d unused: %d;g", player_weapons->weapons[index].weapon_type,
		player_weapons->weapons[index].flags, player_weapons->weapons[index].unused);
	
	debug_trigger_data(index, _primary_weapon);

	switch(index)
	{
		case _weapon_missile_launcher:
		case _weapon_flamethrower:
		case _weapon_alien_shotgun:
		case _weapon_smg:
			break;
			
		default:
			debug_trigger_data(index, _secondary_weapon);
			break;
	}
}

static void debug_trigger_data(
	short weapon_type,
	short which_trigger)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(current_player_index);
	struct trigger_data *trigger;
	struct trigger_definition *trigger_definition;
	struct weapon_definition *weapon_definition= get_weapon_definition(weapon_type);
	
	trigger= &player_weapons->weapons[weapon_type].triggers[which_trigger];
	
	dprintf("%d) State: %d Phase: %d;g", which_trigger, trigger->state, trigger->phase);
	dprintf("%d) Loaded: %d Shot: %d Hit: %d;g", which_trigger, trigger->rounds_loaded, 
		trigger->shots_fired, trigger->shots_hit);
	dprintf("%d) Ticks Since Last: %d;g", which_trigger, trigger->ticks_since_last_shot);

	trigger_definition= get_trigger_definition(current_player_index,
		weapon_type, which_trigger);

	if(weapon_definition->item_type != NONE)
	{
		dprintf("%d) Has %d items;g", which_trigger, 
			current_player->items[weapon_definition->item_type]);
	} else {
		dprintf("%d) Item type is NONE;g", which_trigger);
	}

	if(trigger_definition->ammunition_type != NONE)
	{
		dprintf("%d) Player has %d clips;g", which_trigger, 
			current_player->items[trigger_definition->ammunition_type]);
	} else {
		dprintf("%d) Ammunition type is NONE;g", which_trigger);
	}
}
#endif

void *get_weapon_array(
	void)
{
	return player_weapons_array;
}

long calculate_weapon_array_length(
	void)
{
	return dynamic_world->player_count*sizeof(struct player_weapon_data);
}

/* -------------------------- functions related to rendering */
/* Functions related to rendering! */
/* while this returns true, keep calling.. */
boolean get_weapon_display_information(
	short *count, 
	struct weapon_display_information *data)
{
	boolean valid= FALSE;
	short player_index= current_player_index;

	/* If the player's current weapon is not NONE.. */	
	if(player_has_valid_weapon(player_index))
	{
		struct weapon_data *weapon= get_player_current_weapon(player_index);
		struct weapon_definition *definition= get_weapon_definition(weapon->weapon_type);
		fixed width, height;
		short frame, which_trigger, shape_index, type, flags;
		struct shape_animation_data *high_level_data;
	
		/* Get the default width and height */
		width= definition->idle_width;
		height= definition->idle_height;	
		modify_position_for_two_weapons(player_index, *count, &width, &height);
	
		/* What type of item is this? */
		if(get_weapon_data_type_for_count(player_index, *count, &type, &which_trigger, &flags))
		{
			struct shape_and_transfer_mode owner_transfer_data;
	
			/* Assume the best.. */
			valid= TRUE;
	
			if(type==_weapon_type)
			{
				short phase;

				/* Tell the weapon a frame passed, in case it cares. */
				phase= weapon->triggers[which_trigger].phase;

				/* Calculate the base location.. */
				calculate_weapon_position_for_idle(player_index, which_trigger, 
					weapon->weapon_type, &height, &width);

				/* Figure out where to draw it. */
				switch(weapon->triggers[which_trigger].state)
				{
					case _weapon_lowering:
						assert(definition->ready_ticks);
						height = (3*FIXED_ONE/2)-(((3*FIXED_ONE/2)-definition->idle_height)*phase)/definition->ready_ticks;
						shape_index= definition->idle_shape;
						break;
			
					case _weapon_raising:
						assert(definition->ready_ticks);
						height += (((3*FIXED_ONE/2)-definition->idle_height)*phase)/definition->ready_ticks;
						shape_index= definition->idle_shape;
						break;
			
					case _weapon_charged:
						if(definition->flags & _weapon_overloads)
						{
							fixed flutter_base;
						
							/* 0-> FIXED ONE as it gets closer to nova.. */
							flutter_base= (FIXED_ONE*(CHARGED_WEAPON_OVERLOAD-phase))/CHARGED_WEAPON_OVERLOAD;
							add_random_flutter(flutter_base, &height, &width);
						} else {
							/* Calculate for idle, and then make it bounce */
							add_random_flutter(FIXED_ONE, &height, &width);
						}
			
						/* This is a fully charged weapon.. */
						if(definition->charged_shape != NONE)
						{
							shape_index= definition->charged_shape;
						} else {
							shape_index= definition->idle_shape;
						}
						break;
			
					case _weapon_charging:
						/* This is a fully charged weapon.. */
						if(definition->charging_shape != NONE)
						{
							shape_index= definition->charging_shape;
						} else {
							shape_index= definition->idle_shape;
						}
						break;

					case _weapon_waiting_for_twofist_to_reload:
						/* Get it off the screen.. */
						height= 4*FIXED_ONE;
						shape_index= definition->idle_shape;
						break;
			
					case _weapon_awaiting_twofisted_reload:
					case _weapon_waiting_for_other_idle_to_reload:
						shape_index= definition->idle_shape;
						break;

					case _weapon_idle:
						if(definition->flags & _weapon_is_automatic)
						{
							if(which_trigger==_primary_weapon || 
								(which_trigger==_secondary_weapon && (definition->flags & _weapon_secondary_has_angular_flipping)))
							{
								if(automatic_still_firing(current_player_index, which_trigger))
								{
									shape_index= definition->firing_shape;
								} else {
									shape_index= definition->idle_shape;
									weapon->triggers[which_trigger].sequence= 0;
								}
							} else {
								shape_index= definition->idle_shape;
							}
						} else {
							shape_index= definition->idle_shape;
						}
						break;

					case _weapon_sliding_over_to_second_position:
						if(definition->weapon_class==_twofisted_pistol_class)
						{
							if(which_trigger==_primary_weapon)
							{
								width-= ((weapon->triggers[which_trigger].phase)
									*(PISTOL_SEPARATION_WIDTH/2))/definition->ready_ticks;
							} else {
								width+= ((weapon->triggers[which_trigger].phase)
									*(PISTOL_SEPARATION_WIDTH/2))/definition->ready_ticks;
							}
						} else {
							/* Melee weapons stay where they were. */
						}
						shape_index= definition->idle_shape;
						break;

					case _weapon_sliding_over_from_second_position:
						if(definition->weapon_class==_twofisted_pistol_class)
						{
							short sign;

							if(which_trigger==_primary_weapon)
							{
								sign= -1;
							} else {
								sign= 1;
							}	
							width+= sign*((definition->ready_ticks-weapon->triggers[which_trigger].phase)*(PISTOL_SEPARATION_WIDTH/2))/
								definition->ready_ticks;
						} else {
							/* Melee weapons stay where they were. */
						}
						shape_index= definition->idle_shape;
						break;
			
					case _weapon_firing:
						/* We are going up.. */
						{
							struct trigger_definition *trigger_definition;
	
							trigger_definition= get_player_trigger_definition(player_index, which_trigger);
							height -= (definition->kick_height*(trigger_definition->ticks_per_round-phase))/trigger_definition->ticks_per_round;
							shape_index= definition->firing_shape;
						}
						break;
			
					case _weapon_recovering:
						/* Going back down.. */
						{
							struct trigger_definition *trigger_definition= get_player_trigger_definition(player_index, which_trigger);
							assert(trigger_definition->recovery_ticks);
							height-= (definition->kick_height*phase)/trigger_definition->recovery_ticks;
							shape_index= definition->firing_shape;
						}
						break;
			
					case _weapon_awaiting_reload:
						if(definition->reloading_shape==NONE)
						{
							assert(definition->await_reload_ticks);
							height+= (((3*FIXED_ONE/2)-definition->idle_height)*(definition->await_reload_ticks-phase))/definition->await_reload_ticks;
							shape_index= definition->idle_shape;
						} else {
							height= definition->reload_height;
							shape_index= definition->reloading_shape;
						}
						break;
			
					case _weapon_waiting_to_load:
						if(definition->reloading_shape==NONE)
						{
							/* Consider it offscreen */
							valid= FALSE;
							shape_index= definition->idle_shape;
						} else {
							height= definition->reload_height;
							shape_index= definition->reloading_shape;
						}
						break;
			
					case _weapon_finishing_reload:
						if(definition->reloading_shape==NONE)
						{
							assert(definition->finish_loading_ticks);
							height+= (((3*FIXED_ONE/2)-definition->idle_height)*phase)/definition->finish_loading_ticks;
			
							shape_index= definition->idle_shape;
						} else {
							height= definition->reload_height;
							shape_index= definition->reloading_shape;
						}
						break;
				
					case _weapon_lowering_for_twofisted_reload:
						assert(definition->ready_ticks);
						height = (3*FIXED_ONE/2)-(((3*FIXED_ONE/2)-definition->idle_height)*phase)/definition->ready_ticks;
						shape_index= definition->idle_shape;
						break;

					default:
						halt();
						break;
				}

				/* Determine our frame. */		
				frame= GET_SEQUENCE_FRAME(weapon->triggers[which_trigger].sequence);

				/* Go to the next frame for automatics.. */
				update_automatic_sequence(current_player_index, which_trigger);

				/* setup the positioning information */
				high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, 
					shape_index));
				vassert(frame>=0 && frame<high_level_data->frames_per_view,
					csprintf(temporary, "frame: %d max: %d trigger: %d state: %d count: %d phase: %d", 
					frame, high_level_data->frames_per_view,
					which_trigger, weapon->triggers[which_trigger].state, *count, 
					weapon->triggers[which_trigger].phase));

				data->collection= BUILD_COLLECTION(definition->collection, 0);
				data->low_level_shape_index= high_level_data->low_level_shape_indexes[frame];
				data->vertical_positioning_mode= _position_center;
				data->horizontal_positioning_mode= _position_center;
				data->vertical_position= height;
				data->horizontal_position= width;
				if(flags & _flip_shape_vertical)
				{
					data->flip_vertical= TRUE;
				} else {
					data->flip_vertical= FALSE;
				}
				
				if(flags & _flip_shape_horizontal)
				{
					data->flip_horizontal= TRUE;
				} else {
					data->flip_horizontal= FALSE;
				}
			
				/* Fill in the transfer mode and phase */
				/* Cached, so that we only do it the first time through.. */
				if(!(*count))
				{
					struct player_data *player= get_player_data(player_index);
				
					get_object_shape_and_transfer_mode(&player->camera_location, player->object_index, 
						&owner_transfer_data);
					data->transfer_mode= owner_transfer_data.transfer_mode;
					data->transfer_phase= owner_transfer_data.transfer_phase;
				}
			} 
			else if(type==_shell_casing_type)
			{
				get_shell_casing_display_data(data, which_trigger);
			} else {
				halt();
			}
		}
	
		(*count)++;
	}
	
	return valid;
}

void get_player_weapon_mode_and_type(
	short player_index,
	short *shape_weapon_type,
	short *shape_mode)
{

	if(player_has_valid_weapon(player_index))
	{
		struct weapon_definition *definition= get_current_weapon_definition(player_index);
		struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
		struct weapon_data *weapon_data= get_player_current_weapon(player_index);
		short which_trigger, mode;
		short first_trigger, trigger_count;
	
		*shape_weapon_type= player_weapons->current_weapon;
	
		/* If we are a twofisted pistol, and both are up */
		if(definition->weapon_class==_twofisted_pistol_class && PRIMARY_WEAPON_IS_VALID(weapon_data) && SECONDARY_WEAPON_IS_VALID(weapon_data))
		{
			switch(player_weapons->current_weapon)
			{
				case _weapon_shotgun:
					*shape_weapon_type= _weapon_doublefisted_shotguns;
					break;
					
				case _weapon_pistol:
				default:
					*shape_weapon_type= _weapon_doublefisted_pistols;
					break;
			}
		}
			
		mode= NONE;
	
		trigger_count= get_active_trigger_count_and_states(player_index, player_weapons->current_weapon, 0l, &first_trigger, NULL);
		for(which_trigger= first_trigger; which_trigger<trigger_count; ++which_trigger)
		{
			struct trigger_data *trigger= get_player_trigger_data(player_index, which_trigger);
	
			switch (trigger->state)
			{
				case _weapon_idle:
					if(automatic_still_firing(player_index, which_trigger))
					{
						mode= MAX(mode, _shape_weapon_firing);
					} else {
						mode= MAX(mode, _shape_weapon_idle);
					}
					break;
					
				case _weapon_raising:
				case _weapon_lowering:
				case _weapon_awaiting_reload:
				case _weapon_waiting_to_load:
				case _weapon_finishing_reload:
				case _weapon_sliding_over_to_second_position:
				case _weapon_sliding_over_from_second_position:
				case _weapon_lowering_for_twofisted_reload:
				case _weapon_awaiting_twofisted_reload:
				case _weapon_waiting_for_twofist_to_reload:
				case _weapon_waiting_for_other_idle_to_reload:
					mode= MAX(mode, _shape_weapon_idle);
					break;
				
				case _weapon_charging:
				case _weapon_charged:
					mode= MAX(_shape_weapon_charging, mode);
					break;
		
				case _weapon_firing:
				case _weapon_recovering:
					mode= MAX(_shape_weapon_firing, mode);
					break;
		
				default: 
					halt();
					break;
			}
		}

		assert(mode != NONE);	
		*shape_mode= mode;
	} else {
		/* No weapon.. */
		*shape_mode= _shape_weapon_idle;
		*shape_weapon_type= _weapon_fist;
	}

	return;
}

/* -------- general static code */
static void reset_trigger_data(
	short player_index,
	short weapon_type,
	short which_trigger)
{
	struct trigger_data *trigger= get_trigger_data(player_index, weapon_type, which_trigger);

	trigger->state= _weapon_idle;
	trigger->phase= 0;
	trigger->rounds_loaded= 0;
/* Note that trigger->shots_fired, and trigger->shots_hit are not reset (for total carnage graphs) */
	trigger->ticks_since_last_shot= 0;
	trigger->ticks_firing= 0;
	trigger->sequence= 0;
}

static boolean weapon_works_in_current_environment(
	short weapon_index)
{
	boolean weapon_works= TRUE;

	if(weapon_index!=NONE)
	{
		struct weapon_definition *definition= get_weapon_definition(weapon_index);
		weapon_works= item_valid_in_current_environment(definition->item_type);
	}
	
	return weapon_works;
}

/* This selects the next best weapon */
static void select_next_best_weapon(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	short index, current_weapon_index;
	
	/* Find where we are in the array. */
	current_weapon_index= find_weapon_power_index(player_weapons->current_weapon);
	
	/* Find the next weapon that the player has. */
	for(index=current_weapon_index+1; index<NUMBER_OF_WEAPONS; ++index)
	{
		if(ready_weapon(player_index, weapon_ordering_array[index])) break;
	}
	
	if(index==NUMBER_OF_WEAPONS)
	{
		/* Try going down.. */
		for(index= current_weapon_index-1; index>=0; index--)
		{
			if(ready_weapon(player_index, weapon_ordering_array[index])) break;
		}
	}
	
	/* if we didn't change, we punt */
	return;
}

static void select_next_weapon(
	short player_index,
	boolean forward)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

	if(player_weapons->desired_weapon!=_weapon_ball && player_weapons->current_weapon!=_weapon_ball)
	{
		short index, desired_weapon_index;

		desired_weapon_index= find_weapon_power_index(player_weapons->desired_weapon);

		/* This starts at one because we are trying to go forward or backward.. */	
		for(index=1; index<NUMBER_OF_WEAPONS; ++index)
		{
			short test_weapon_index;
			
			if(forward)
			{
				test_weapon_index= (index+desired_weapon_index)%NUMBER_OF_WEAPONS;
			} else {
				test_weapon_index= (desired_weapon_index-index+NUMBER_OF_WEAPONS)%NUMBER_OF_WEAPONS;
			}
			
			if(ready_weapon(player_index, weapon_ordering_array[test_weapon_index])) break;
		}
	}

	return;
}

static struct trigger_definition *get_player_trigger_definition(
	short player_index,
	short which_trigger)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	return get_trigger_definition(player_index, player_weapons->current_weapon, which_trigger);
}

static struct trigger_definition *get_trigger_definition(
	short player_index,
	short which_weapon,
	short which_trigger)
{
	struct weapon_definition *definition= get_weapon_definition(which_weapon);
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	struct trigger_definition *trigger_definition;

	assert(which_trigger>=0 && which_trigger<NUMBER_OF_TRIGGERS);
	assert(which_weapon>=0 && which_weapon<NUMBER_OF_WEAPONS);

#ifdef DEBUG
	switch(which_weapon)
	{
		case _weapon_missile_launcher:
		case _weapon_flamethrower:
		case _weapon_smg:
			vassert(which_trigger==_primary_weapon, csprintf(temporary, "which: %d weapon: %d",
				which_trigger, which_weapon));
			break;

		case _weapon_fist:
		case _weapon_alien_shotgun:
		case _weapon_shotgun:
		case _weapon_pistol:
		case _weapon_plasma_pistol:
		case _weapon_assault_rifle:
		case _weapon_ball:
			vassert(which_trigger==_primary_weapon || which_trigger==_secondary_weapon, 
				csprintf(temporary, "which: %d weapon: %d", which_trigger, which_weapon));
			break;
			
		default:
			halt();
			break;
	}
#endif

	trigger_definition= &definition->weapons_by_trigger[which_trigger];
	
	return trigger_definition;
}

static struct trigger_data *get_player_trigger_data(
	short player_index,
	short which_trigger)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	
	assert(player_weapons->current_weapon>=0 && player_weapons->current_weapon<NUMBER_OF_WEAPONS);
	
	return get_trigger_data(player_index, player_weapons->current_weapon, which_trigger);
}

struct trigger_data *get_trigger_data(
	short player_index, 
	short weapon_index, 
	short which_trigger)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	
	assert(which_trigger>=0 && which_trigger<NUMBER_OF_TRIGGERS);
	assert(weapon_index>=0 && weapon_index<NUMBER_OF_WEAPONS);
	
	return &player_weapons->weapons[weapon_index].triggers[which_trigger];
}

static struct weapon_data *get_player_current_weapon(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	
	assert(player_weapons->current_weapon>=0 && player_weapons->current_weapon<NUMBER_OF_WEAPONS);
	
	return &player_weapons->weapons[player_weapons->current_weapon];
}

/* 
	This function does the following:
		1) Calculates how many shots to fire.
		2) creates all of the projectiles,
		3) handles recoil
		4) plays sounds
		5) updates shots fired/rounds loaded
*/
static void fire_weapon(
	short player_index,
	short which_trigger,
	fixed charged_amount,
	boolean flail_wildly)
{
	struct player_data *player= get_player_data(player_index);
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	struct weapon_definition *definition= get_weapon_definition(player_weapons->current_weapon);
	struct trigger_definition *trigger_definition;
	struct trigger_data	*trigger;
	world_point3d origin, vector;
	short origin_polygon, flailing_bonus, rounds_to_fire;
	fixed damage_modifier;

	/* if they are under water, and it isn't a melee weapon, they lose */
	if ((player->variables.flags&_HEAD_BELOW_MEDIA_BIT) && !(definition->flags&_weapon_fires_under_media))
	{
		play_weapon_sound(player_index, WEAPON_SHORTED_SOUND, FIXED_ONE);
		return;
	}

	/* Delta theta.. */
	flailing_bonus= flail_wildly ? 30 : 0;

	/* Get the weapon they are using... */
	trigger_definition= get_player_trigger_definition(player_index, which_trigger);
	trigger= get_player_trigger_data(player_index, which_trigger);

	/* Calculate the number of rounds to fire.. */
	if(trigger_definition->burst_count)
	{
		if(trigger_definition->charging_ticks)
		{
			rounds_to_fire= (trigger_definition->burst_count*charged_amount)/FIXED_ONE;
		} else {
			/* Non charging weapon.. */
			rounds_to_fire= trigger_definition->burst_count;
		}
	} else {
		rounds_to_fire= 1;
	}

	/* I hear you.. */
	if(definition->weapon_class != _melee_class)
	{
		if(which_trigger==_primary_weapon && (definition->flags & _weapon_is_automatic) && trigger->ticks_firing<2)
		{
			activate_nearby_monsters(player->monster_index, player->monster_index, 
				_pass_one_zone_border|_activate_invisible_monsters);
		} else {
			activate_nearby_monsters(player->monster_index, player->monster_index, 
				_pass_one_zone_border|_activate_invisible_monsters);
		}
	}

	/* If we have any rounds of ammunition */
	if(trigger->rounds_loaded>0)
	{
		angle delta_theta= 0;
		
		/* Handle the second trigger of the alien weapon.. */
		if(which_trigger==_secondary_weapon && (definition->flags & _weapon_secondary_has_angular_flipping))
		{
			struct weapon_data *weapon= get_player_current_weapon(player_index);

			delta_theta= ANGULAR_VARIANCE*GET_WEAPON_VARIANCE_SIGN(weapon);
			FLIP_WEAPON_VARIANCE_SIGN(weapon);
		}
	
		/* Note that we no longer pin the rounds to fire by the amount in the gun.. */
		if(rounds_to_fire>trigger->rounds_loaded) 
		{
			rounds_to_fire= trigger->rounds_loaded;
		}
			
		/* Spawn the projectile. I can't update the shots_hit until it actually hits the */
		/* target, which comes in update */
		calculate_weapon_origin_and_vector(player_index, which_trigger, 
			&origin, &vector, &origin_polygon, delta_theta);
	
		/* If it is a melee weapon, add in the damage based on their speed.. */
		if(definition->weapon_class==_melee_class)
		{
			/* Damage is in range of [FIXED_ONE/8, FIXED_ONE] */
			/* Therefore melee damage should be pretty high. */
			damage_modifier= get_player_forward_velocity_scale(player_index);
			damage_modifier= MAX(damage_modifier, FIXED_ONE/8);
		} else {
			damage_modifier= FIXED_ONE;
		}

		while(rounds_to_fire--)
		{
			/* Increment rounds fired count */
			player_weapons->weapons[player_weapons->current_weapon].triggers[which_trigger].shots_fired++;
			player_weapons->weapons[player_weapons->current_weapon].triggers[which_trigger].rounds_loaded--;

			/* Update the player ammunition count */
			update_player_ammo_count(player_index);

			/* on certain weapons, keep the weapon ammo pools synced.... */
			if(definition->flags & _weapon_triggers_share_ammo)
			{
				player_weapons->weapons[player_weapons->current_weapon].triggers[!which_trigger].rounds_loaded--;
				assert(player_weapons->weapons[player_weapons->current_weapon].triggers[0].rounds_loaded==
					player_weapons->weapons[player_weapons->current_weapon].triggers[1].rounds_loaded);
			}
			
			/* on dual function classes, keep the two ammo pools synched.. */
			if(definition->weapon_class==_dual_function_class)
			{
#ifdef OBSOLETE
				short other_trigger;
				
				other_trigger= !which_trigger;
				player_weapons->weapons[player_weapons->current_weapon].triggers[other_trigger].rounds_loaded--;
				assert(player_weapons->weapons[player_weapons->current_weapon].triggers[0].rounds_loaded==
					player_weapons->weapons[player_weapons->current_weapon].triggers[1].rounds_loaded);
#endif
				/* Dual function class, charging weapons use 4x the ammo. */
				if(which_trigger==_secondary_weapon && trigger_definition->charging_ticks)
				{
					short rounds_count;

					rounds_count= MIN(COST_PER_CHARGED_WEAPON_SHOT-1, player_weapons->weapons[player_weapons->current_weapon].triggers[which_trigger].rounds_loaded);

					/* Decrement the ammo.. */					
					player_weapons->weapons[player_weapons->current_weapon].triggers[which_trigger].rounds_loaded-= rounds_count;
		
					/* Decrement the other ammo as well.. */				
					if(definition->flags & _weapon_triggers_share_ammo)
					{
						player_weapons->weapons[player_weapons->current_weapon].triggers[!which_trigger].rounds_loaded-= rounds_count;
						assert(player_weapons->weapons[player_weapons->current_weapon].triggers[0].rounds_loaded==
							player_weapons->weapons[player_weapons->current_weapon].triggers[1].rounds_loaded);
					}
				}
			}
			
			/* Fire the projectile */
			if(trigger_definition->projectile_type!=_projectile_ball_dropped)
			{
				new_projectile(&origin, origin_polygon, &vector, 
					trigger_definition->theta_error+flailing_bonus, 
					trigger_definition->projectile_type, 
					player->monster_index, _monster_marine, NONE, damage_modifier);
			}
		}

		/* Spawn a shell casing.... */
		if (trigger_definition->shell_casing_type!=NONE)
		{
			short type= trigger_definition->shell_casing_type;
			word flags= 0;
			
			if (type==_shell_casing_pistol)
			{
				struct weapon_data *weapon= get_player_current_weapon(player_index);

				if (SECONDARY_WEAPON_IS_VALID(weapon))
				{
					if (PRIMARY_WEAPON_IS_VALID(weapon))
					{
						type= which_trigger ? _shell_casing_pistol_left : _shell_casing_pistol_right;
					}
					else
					{
						flags|= _shell_casing_is_reversed;
					}
				}
			}
			
			new_shell_casing(player_index, type, flags);
		}

		/* setup the weapon phase.. */
		trigger->state= _weapon_firing;
		trigger->phase= trigger_definition->ticks_per_round;
		trigger->ticks_since_last_shot= 0;
	
		/* Setup the decay.. */
		player->weapon_intensity_decay= definition->firing_intensity_decay_ticks;
	
		/* Blam */
		play_weapon_sound(player_index, trigger_definition->firing_sound, FIXED_ONE);
					
		/* Push them back.. */
		if(trigger_definition->recoil_magnitude)
		{
			accelerate_monster(player->monster_index, 
				-(trigger_definition->recoil_magnitude*sine_table[player->elevation])>>TRIG_SHIFT,
				NORMALIZE_ANGLE(player->facing+HALF_CIRCLE), 
				(trigger_definition->recoil_magnitude*cosine_table[player->elevation])>>TRIG_SHIFT);
		}
	} else {
		/* uh oh, no bullets. */
		while(rounds_to_fire--)
		{
			play_weapon_sound(player_index, trigger_definition->click_sound, FIXED_ONE);
		}
	}

	return;
}

struct weapon_definition *get_current_weapon_definition(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	
	assert(player_weapons->current_weapon>=0 && player_weapons->current_weapon<NUMBER_OF_WEAPONS);
	
	return get_weapon_definition(player_weapons->current_weapon);
}

static void calculate_weapon_origin_and_vector(
	short player_index,
	short which_trigger,
	world_point3d *origin,
	world_point3d *vector,
	short *origin_polygon,
	angle delta_theta)
{
	struct player_data *player= get_player_data(player_index);
	struct object_data *object= get_object_data(player->object_index);
	struct trigger_definition *trigger_definition;
	struct weapon_definition *definition= get_current_weapon_definition(player_index);
	struct weapon_data *weapon_data= get_player_current_weapon(player_index);
	short dx_translation_amount;
	world_point3d destination;
	angle projectile_facing;

	trigger_definition= get_player_trigger_definition(player_index, which_trigger);
	
	*origin= player->camera_location;	
	origin->z += trigger_definition->dz;
	
	/* Translate the projectile out to the end of the gun barrel.. */
	translate_point3d(origin, WEAPON_FORWARD_DISPLACEMENT, 
		player->facing, player->elevation);

	/* Do left/right translation */
	/* if it is twofisted, and both weapons aren't up, don't translate.. */
	if(definition->weapon_class==_twofisted_pistol_class && 
		(PRIMARY_WEAPON_IS_VALID(weapon_data) && !SECONDARY_WEAPON_IS_VALID(weapon_data)) ||
		(!PRIMARY_WEAPON_IS_VALID(weapon_data) && SECONDARY_WEAPON_IS_VALID(weapon_data)))
	{
		dx_translation_amount= 0;
	} else {
		dx_translation_amount= trigger_definition->dx;
	}

	/* Handle the left/right translation */
	translate_point2d((world_point2d *) origin, dx_translation_amount, 
		NORMALIZE_ANGLE(player->facing+QUARTER_CIRCLE));

	/* Get a second point to build up a vector.. */
	destination= *origin;
	projectile_facing= NORMALIZE_ANGLE(player->facing+delta_theta);
	translate_point3d(&destination, WORLD_ONE_HALF, 
		projectile_facing, player->elevation);
	
	/* And create the vector.. */
	vector->x= destination.x-origin->x;
	vector->y= destination.y-origin->y;
	vector->z= destination.z-origin->z;

	/* Now calculate the origin polygon index */
	*origin_polygon= find_new_object_polygon((world_point2d *) &player->location,
		(world_point2d *)origin, object->polygon);
		
	/* They blew the pooch- this is expensive, therefore let's hope it doesn't happen often */
	if(*origin_polygon==NONE)
	{
		short source_polygon= object->polygon;
		short line_crossed;
		struct line_data *line;
		world_distance distance;

		while(source_polygon != NONE)
		{
			line_crossed= find_line_crossed_leaving_polygon(source_polygon, (world_point2d *)
				&player->location, (world_point2d *) origin);
			source_polygon= find_adjacent_polygon(source_polygon, line_crossed);
		}

		line= get_line_data(line_crossed);
		find_line_intersection(&get_endpoint_data(line->endpoint_indexes[0])->vertex,
			&get_endpoint_data(line->endpoint_indexes[1])->vertex, 
			&player->location, origin, origin);
		
		/* Now guess the distance.. */
		distance= distance2d((world_point2d *)&player->location, (world_point2d *) origin);
		distance-= 50; /* Fudge factor */
						
		*origin= player->location;
		translate_point2d((world_point2d *) origin, distance, 
			NORMALIZE_ANGLE(player->facing+QUARTER_CIRCLE));

		*origin_polygon= find_new_object_polygon((world_point2d *) &player->location,
			(world_point2d *)origin, object->polygon);
		assert(*origin_polygon != NONE);
	}
		
	return;
}

static boolean reload_weapon(
	short player_index,
	short which_trigger)
{
	struct trigger_data *trigger= get_player_trigger_data(player_index, which_trigger);
	struct trigger_definition *trigger_definition= 
		get_player_trigger_definition(player_index, which_trigger);
	struct player_data *player= get_player_data(player_index);
	struct weapon_definition *definition= get_current_weapon_definition(player_index);
	boolean can_reload;

	assert(trigger->state==_weapon_idle && trigger->rounds_loaded==0);
	assert(trigger_definition->ammunition_type==NONE || (trigger_definition->ammunition_type>=0 && trigger_definition->ammunition_type<NUMBER_OF_ITEMS));

	if(trigger_definition->ammunition_type != NONE && player->items[trigger_definition->ammunition_type]>0)
	{
		struct weapon_data *weapon= get_player_current_weapon(player_index);
	
		/* If this is a twofisted weapon & it doesn't reload in one hand... */
		/* If it is a twofisted weapon, doesn't reload in one hand, and both weapons are valid */
		if(definition->weapon_class==_twofisted_pistol_class && 
			!(definition->flags & _weapon_reloads_in_one_hand)
			&& PRIMARY_WEAPON_IS_VALID(weapon) && SECONDARY_WEAPON_IS_VALID(weapon))
		{
			struct trigger_data *other_trigger= get_player_trigger_data(player_index, !which_trigger);
				
/* Note the presence of two weapons trying to reload simultaneously.. */
			if(other_trigger->state==_weapon_waiting_for_other_idle_to_reload)
			{
				/* He already had priority. */
				trigger->state= _weapon_lowering_for_twofisted_reload;
				trigger->phase= definition->ready_ticks;
				other_trigger->state= _weapon_awaiting_twofisted_reload;
				other_trigger->phase= definition->ready_ticks;
			} else {
				/* Try as we go through the loop next time.. */
				trigger->state= _weapon_waiting_for_other_idle_to_reload;
				trigger->phase= 0;
			}
		} else {
			/* Don't go to a state that has a zero duration.. */
			if(definition->await_reload_ticks)
			{
				trigger->state= _weapon_awaiting_reload;
				trigger->phase= definition->await_reload_ticks; // should this be by trigger?
			} else {
				trigger->state= _weapon_waiting_to_load;
				trigger->phase= definition->loading_ticks; // should this be by trigger?
			}
		}
		trigger->sequence= 0; // Reset this here, because it won't get reset otherwise.
		can_reload= TRUE;
	} else {
		/* If this weapon needs to dissappear */
		if(definition->flags & _weapon_disappears_after_use)
		{
			destroy_current_weapon(player_index);
		}
		can_reload= FALSE;
	}
	
	return can_reload;
}

static void destroy_current_weapon(
	short player_index)
{
	struct player_data *player= get_player_data(player_index);
	struct weapon_definition *definition= get_current_weapon_definition(player_index);
	short item_type;

	assert(definition->item_type>=0 && definition->item_type<NUMBER_OF_ITEMS);
	if(get_item_kind(definition->item_type)==_ball)
	{
		/* Drop the ball.. */
		short ball_color= find_player_ball_color(player_index);
		
		assert(ball_color!=NONE);
		item_type= find_player_ball_color(player_index)+BALL_ITEM_BASE;
	} else {
		item_type= definition->item_type;
	}

	/* Decrement the item.. */
	player->items[item_type]--;
	if(player->items[item_type]<=0)	player->items[item_type]= NONE;

	/* Second parameter: */
	/* NONE- don't switch to ammo list */
	/* _i_magnum_magazine- switch to ammo list */
	mark_player_inventory_as_dirty(player_index, _i_magnum_magazine);
}

static boolean check_reload(
	short player_index,
	short which_trigger)
{
	struct trigger_data *trigger= get_player_trigger_data(player_index, which_trigger);
	struct weapon_definition *definition= 
		get_current_weapon_definition(player_index);
	boolean reloaded_weapon= FALSE;

	/* Check to see if the weapon is empty.. */
	if(!trigger->rounds_loaded)
	{
		switch(definition->weapon_class)
		{
			case _melee_class:
				/* Always have a round loaded.. */
				trigger->rounds_loaded= 1;
				break;
				
			case _dual_function_class:
				if(definition->flags & _weapon_triggers_share_ammo)
				{
					which_trigger= _primary_weapon;
				}
				/* Fallthrough */
			case _normal_class:
				/* Hmmm.. we are out of ammunition */
				if(!reload_weapon(player_index, which_trigger))
				{
					/* Switch to the next weapon.. */
					select_next_best_weapon(player_index);
				} else {
					reloaded_weapon= TRUE;
				}
				break;

			case _twofisted_pistol_class:
				{
					struct weapon_data *weapon= get_player_current_weapon(player_index);

					if(PRIMARY_WEAPON_IS_VALID(weapon) && SECONDARY_WEAPON_IS_VALID(weapon))
					{
						if(!reload_weapon(player_index, which_trigger))
						{
							/* Okay, we couldn't reload one of them.  */
							/* Does the other one have ammo? */
							struct trigger_data *other_trigger_data= 
								get_player_trigger_data(player_index, !which_trigger);

							if(!other_trigger_data->rounds_loaded)
							{
								/* We have to change... */
								select_next_best_weapon(player_index);
							} else {
								if(which_trigger==_primary_weapon)
								{
/* ���Problems? */
									struct trigger_data *other_trigger= 
										get_player_trigger_data(player_index, !which_trigger);
										
									trigger->state= _weapon_lowering;
									trigger->phase= definition->ready_ticks-1;
									trigger->sequence= 0;
									
									other_trigger->state= _weapon_sliding_over_from_second_position;
									other_trigger->phase= definition->ready_ticks;
									other_trigger->sequence= 0;
								} else {
									struct trigger_data *other_trigger= 
										get_player_trigger_data(player_index, !which_trigger);

									assert(which_trigger==_secondary_weapon);
									trigger->state= _weapon_lowering;
									trigger->phase= definition->ready_ticks;
									trigger->sequence= 0;
									
									other_trigger->state= _weapon_sliding_over_from_second_position;
									other_trigger->phase= definition->ready_ticks;
									other_trigger->sequence= 0;
								}
							}
						} else {
							reloaded_weapon= TRUE;
						}
					} else {
						/* Hmmm.. we are out of ammunition */
						if(!reload_weapon(player_index, which_trigger))
						{
							/* Switch to the next weapon.. */
							select_next_best_weapon(player_index);
						} else {
							reloaded_weapon= TRUE;
						}
					} 
				}
				break;

			case _multipurpose_class:
				/* Only switch if both of them are out of ammo, and we can't reload the one we */
				/*  just fired with. */
				if(!reload_weapon(player_index, which_trigger))
				{
					short other_trigger= !which_trigger;
					struct trigger_data *other_trigger_data;
						
					/* check to see if the other one is empty. */
					other_trigger_data= get_player_trigger_data(
						player_index, other_trigger);
						
					if(!other_trigger_data->rounds_loaded)
					{
						switch(other_trigger_data->state)
						{
							case _weapon_awaiting_reload:
							case _weapon_waiting_to_load:
							case _weapon_finishing_reload:
								/* Don't switch, because the other is empty now, but not for long.. */
								break;
								
							default:
//dprintf("Unable to reload ar & other out of ammo (Which: %d)", which_trigger);
								/* Switch to the next weapon.. */
								select_next_best_weapon(player_index);
								break;
						}
					}
				} else {
					reloaded_weapon= TRUE;
				}
				break;
				
			default:
				halt();
				break;
		}
	}

	return reloaded_weapon;
}

static void put_rounds_into_weapon(
	short player_index,
	short which_weapon,
	short which_trigger)
{
	struct trigger_data *trigger= get_trigger_data(player_index, which_weapon, which_trigger);
	struct trigger_definition *trigger_definition= get_trigger_definition(player_index, which_weapon, which_trigger); 
	struct weapon_definition *definition= get_weapon_definition(which_weapon);
	struct player_data *player= get_player_data(player_index);

	assert(trigger_definition->ammunition_type>=0 && trigger_definition->ammunition_type<NUMBER_OF_ITEMS);
	assert(player->items[trigger_definition->ammunition_type]>0);

	/* Load the gun */
	trigger->rounds_loaded= trigger_definition->rounds_per_magazine;

	/* Decrement the ammo magazine count. */
	player->items[trigger_definition->ammunition_type]--;

	/* Update the inventory display. Second parameter: NONE- don't switch to ammo list */
	/* _i_magnum_magazine- switch to ammo list */
	mark_player_inventory_as_dirty(player_index, _i_magnum_magazine);

	/* Keep the ammo stores in sync for dual function class weapons */
#ifdef OBSOLETE
	if(definition->weapon_class==_dual_function_class)
	{
		trigger= get_player_trigger_data(player_index, !which_trigger);
		trigger->rounds_loaded= trigger_definition->rounds_per_magazine;
	}
#endif
	if(definition->flags & _weapon_triggers_share_ammo)
	{
		trigger= get_player_trigger_data(player_index, !which_trigger);
		trigger->rounds_loaded= trigger_definition->rounds_per_magazine;
	}


	/* Update the world ammunition count for the placement data */
	object_was_just_destroyed(_object_is_item, trigger_definition->ammunition_type);

	/* Update the player ammunition count */
	update_player_ammo_count(player_index);

	/* Play the sound. */
	play_weapon_sound(player_index, trigger_definition->reloading_sound, FIXED_ONE);
}

/* This, my friend, is the trickiest of the functions */
static boolean handle_trigger_down(
	short player_index, 
	short which_trigger)
{
	struct weapon_data *weapon= get_player_current_weapon(player_index);
	boolean fired= FALSE;

	/* IF this weapon is idle.. */
	if(weapon->triggers[which_trigger].state==_weapon_idle)
	{
		struct trigger_definition *trigger_definition= get_player_trigger_definition(
			player_index, which_trigger);

		/* If this weapon charges.. */
		if(trigger_definition->charging_ticks)
		{
			struct weapon_definition *definition= get_weapon_definition(weapon->weapon_type);

			/* Either the other weapon is idle and we are dual, or we aren't dual */
			if(definition->weapon_class != _dual_function_class || 
				(definition->weapon_class==_dual_function_class && weapon->triggers[!which_trigger].state==_weapon_idle))
			{
				/* start charging... */
				weapon->triggers[which_trigger].state= _weapon_charging;
				weapon->triggers[which_trigger].phase= trigger_definition->charging_ticks;
				play_weapon_sound(player_index, trigger_definition->charging_sound, FIXED_ONE);
			}
		} else {
			struct weapon_definition *definition= get_current_weapon_definition(player_index);
	
			switch(definition->weapon_class)
			{
				case _melee_class:
				case _twofisted_pistol_class:
					/* Stagger the weapons firing. */
					/* Rocky modification */
					if(PRIMARY_WEAPON_IS_VALID(weapon) && SECONDARY_WEAPON_IS_VALID(weapon))
					{
						if(definition->flags & _weapon_fires_out_of_phase)
						{
							short total_delay, delay;
							struct trigger_definition *other_trigger_definition= get_player_trigger_definition(
								player_index, !which_trigger);
							
							total_delay= other_trigger_definition->recovery_ticks+other_trigger_definition->ticks_per_round;
							delay= total_delay-weapon->triggers[!which_trigger].phase;
							switch(weapon->triggers[!which_trigger].state)
							{
								case _weapon_firing: delay-= trigger_definition->recovery_ticks;
								case _weapon_recovering:
									break;
								
								default:
									total_delay= delay= 1;
									break;
							}
			
							/* Only able to fire exactly out of sync. */				
							if(delay>total_delay/2)
							{
								fired= TRUE;
							}
						} 
						else 
						{
							switch(weapon->triggers[!which_trigger].state)
							{
								case _weapon_firing:
								case _weapon_recovering:
									break;
							
								default:
									fired= TRUE;
									break;
							}
						}
					} 
					else 
					{
						/* Both aren't up.  They can fire.. */
						fired= TRUE;
					}
					break;
					
				case _normal_class:
					/* Always able to fire.. */
					fired= TRUE;
					break;
					
				case _multipurpose_class:
					if(which_trigger==_secondary_weapon)
					{
						switch(weapon->triggers[_primary_weapon].state)
						{
							case _weapon_idle:
							case _weapon_recovering:
							case _weapon_firing:
								fired= TRUE;
								break;
						
							default:
								/* Don't fire the other weapon when we are reloading, raising, lowering, etc. */
								break;
						}
					} else {
						/* Can always fire an idle multifunction class */
						fired= TRUE;
					}
					break;
					
				case _dual_function_class:
					fired= TRUE;
					break;
					
				default:
					halt();
					break;
			}
			
			if(fired)
			{
				fire_weapon(player_index, which_trigger, FIXED_ONE, FALSE);
			}
		}
	}
	
	/* Set the trigger down state */
	if(which_trigger==_primary_weapon) SET_TRIGGER_DOWN(weapon, TRUE);

	return fired;
}

/* we only care about the trigger going up on charged weapons.. */
static boolean handle_trigger_up(
	short player_index, 
	short which_trigger)
{
	struct weapon_data *weapon= get_player_current_weapon(player_index);
	struct trigger_definition *trigger_definition= 
		get_player_trigger_definition(player_index, which_trigger);
	boolean discharge;
	fixed charged_amount;

	/* On charged weapons, when the trigger goes up, we discharge.. */
	switch(weapon->triggers[which_trigger].state)
	{
		case _weapon_charging:
			discharge= TRUE;
			charged_amount= ((trigger_definition->charging_ticks-weapon->triggers[which_trigger].phase)*FIXED_ONE)/FIXED_ONE;
			break;
			
		case _weapon_charged:
			/* These are the only states that we really care about. */
			discharge= TRUE;
			charged_amount= FIXED_ONE;
			break;
		
		default:
			discharge= FALSE;
			break;
	}

	/* Fire the weapon (but only if it is at least 90% charged.) */
	if(discharge)
	{
		struct player_data *player= get_player_data(player_index);
		
		/* Don't discharge on teleporting. */
		if(!PLAYER_IS_TELEPORTING(player) && !PLAYER_IS_INTERLEVEL_TELEPORTING(player))
		{
			if(charged_amount>(9*FIXED_ONE)/10)
			{
				fire_weapon(player_index, which_trigger, charged_amount, FALSE);
			} else {
				struct player_data *player= get_player_data(player_index);
			
				/* You lose. */
				weapon->triggers[which_trigger].state= _weapon_idle;
				weapon->triggers[which_trigger].phase= IDLE_PHASE_COUNT;
				weapon->triggers[which_trigger].sequence= 0;
				stop_sound(player->object_index, trigger_definition->charging_sound);
			}
		}
	}

	/* Trigger down is only maintained to avoid switching a weapon */
	if(which_trigger==_primary_weapon) 
	{
		SET_TRIGGER_DOWN(weapon, FALSE);
											
		if(weapon->triggers[which_trigger].ticks_firing>FIRING_BEFORE_SHELL_CASING_SOUND_IS_PLAYED)
		{
			struct trigger_definition *trigger_definition= get_player_trigger_definition(player_index, 
				which_trigger);
				
			weapon->triggers[which_trigger].ticks_firing= 0;
			play_shell_casing_sound(player_index, trigger_definition->shell_casing_sound);
		}
	}

	return discharge;
}

/* Need to put a reload weapon in here, if that is what we want to do.. */
static boolean ready_weapon(
	short player_index,
	short weapon_index)
{
	boolean able_to_ready= FALSE;
	struct weapon_definition *definition= get_weapon_definition(weapon_index);
	struct player_data *player= get_player_data(player_index);
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

	if(weapon_works_in_current_environment(weapon_index))
	{
		if(player_weapons->desired_weapon != weapon_index)
		{
			if(player->items[definition->item_type] > 0 || weapon_index==_weapon_ball)
			{
				if(player_weapon_has_ammo(player_index, weapon_index))
				{
					player_weapons->desired_weapon= weapon_index;
	
					/* update the weapon display */
					if(player_index==current_player_index) mark_weapon_display_as_dirty();
					able_to_ready= TRUE;
				}
			}
		}
	}
	
	return able_to_ready;
}

static boolean player_weapon_has_ammo(
	short player_index,
	short weapon_index)
{
	struct player_data *player= get_player_data(player_index);
	struct weapon_definition *definition= get_weapon_definition(weapon_index);
	boolean has_ammo= FALSE;

	if(definition->weapon_class==_melee_class)
	{
		has_ammo= TRUE;
	} else {
		short which_trigger, max_triggers;
		
		switch(definition->weapon_class)
		{
			case _melee_class:
			case _dual_function_class:
			case _twofisted_pistol_class:
			case _multipurpose_class:
				max_triggers= 2;
				break;
				
			case _normal_class:
				max_triggers= 1;
				break;
				
			default:
				max_triggers= 1;
				halt();
				break;
		}

		for(which_trigger= _primary_weapon; !has_ammo && which_trigger<max_triggers; ++which_trigger)
		{
			struct trigger_data *trigger= get_trigger_data(player_index, weapon_index,
				which_trigger);

			if(trigger->rounds_loaded>0)
			{
				has_ammo= TRUE;
			} else {
				struct trigger_definition *trigger_definition=
					get_trigger_definition(player_index, weapon_index, which_trigger);
					
				/* Could we load this guy? */
				if(	trigger_definition->ammunition_type != NONE &&
					player->items[trigger_definition->ammunition_type] > 0)
				{
					has_ammo=  TRUE;
				}
			}
		}
	}
	
	return has_ammo;
}

static void lower_weapon(
	short player_index,
	short weapon_index)
{
	struct player_data *player= get_player_data(player_index);
	struct weapon_definition *definition= get_weapon_definition(weapon_index);
	short which_trigger, active_trigger_count, first_trigger;

	active_trigger_count= get_active_trigger_count_and_states(player_index, weapon_index,
		0l, &first_trigger, NULL);
	for(which_trigger= first_trigger; which_trigger<active_trigger_count; ++which_trigger)
	{
		struct trigger_data *trigger= get_trigger_data(player_index, weapon_index, 
			which_trigger);

		/* If this weapon isn't already lowering.. */
		if(trigger->state != _weapon_lowering) 
		{
			struct trigger_definition *trigger_definition= 
				get_trigger_definition(player_index, weapon_index, which_trigger);

			/* If we aren't raising, set the phase.  Otherwise use */
			/*  the current phase to go back down.. */	
			if( trigger->state==_weapon_idle)
			{
				trigger->phase= definition->ready_ticks;
			} else {
				assert(trigger->state==_weapon_raising);
				trigger->phase= definition->ready_ticks - trigger->phase;
			}
			
			trigger->state= _weapon_lowering;
			trigger->sequence= 0;
		} 
	}
}

/*
	note that if we are raising a weapon, we want to fill it up with ammo first, 
	if we can (and should).  This means that if you switch weapons during a lengthy
	reload, you can win.
*/
static void raise_weapon(
	short player_index,
	short weapon_index)
{
	struct player_data *player= get_player_data(player_index);
	struct weapon_definition *definition= get_weapon_definition(weapon_index);
	short which_trigger, active_trigger_count, first_trigger;

// dprintf("Raising: %d;g", weapon_index);
	active_trigger_count= get_active_trigger_count_and_states(player_index, weapon_index, 0l,
		&first_trigger, NULL);
	for(which_trigger= first_trigger; which_trigger<active_trigger_count; ++which_trigger)
	{
		struct trigger_data *trigger= get_trigger_data(player_index, weapon_index, 
			which_trigger);

		if(trigger->state!=_weapon_raising)
		{
			struct trigger_definition *trigger_definition= 
				get_trigger_definition(player_index, weapon_index, which_trigger);
	
			/* If we aren't raising, set the phase.  Otherwise use */
			/*  the current phase to go back down.. */	
			if(trigger->state==_weapon_idle)
			{
				trigger->phase= definition->ready_ticks;
			} else {
				vassert(trigger->state==_weapon_lowering, csprintf(temporary, "State: %d phase: %d trigger: %d weapon: %d",
					trigger->state, trigger->phase, which_trigger, weapon_index));
				trigger->phase= definition->ready_ticks-trigger->phase;
// dprintf("Lowering: Phase: %d;g", trigger->phase);
			}
			trigger->state= _weapon_raising;
			trigger->sequence= 0;

			if(which_trigger==_primary_weapon)
			{
				struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	
				SET_PRIMARY_WEAPON_IS_VALID(&player_weapons->weapons[weapon_index], TRUE);
			} else {
				struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

// dprintf("Second valid;g");
				SET_SECONDARY_WEAPON_IS_VALID(&player_weapons->weapons[weapon_index], TRUE);
			}
		}
	}
	
	return;
}

/* Note that we are guaranteed that the current weapon has ammo.. */
static boolean should_switch_to_weapon(
	short player_index,
	short new_weapon)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	boolean should_change= FALSE;

	assert(player_weapon_has_ammo(player_index, new_weapon));

	/* Switch to it if we should.. */
	/* Change if: */
	/*		The current weapon is the desired weapon, the trigger is not down, AND either: */
	/*		A) They don't have a weapon */
	/*		B) The new weapon is better and the current weapon is not charging OR */
	/*		C) The current weapon has no ammunition */
	if(weapon_works_in_current_environment(new_weapon))
	{
		if(player_weapons->desired_weapon==player_weapons->current_weapon || new_weapon==_weapon_ball)
		{
			short new_weapon_index, current_weapon_index;

			/* Find the relative powers of the weapons to determine which is greater */
			new_weapon_index= find_weapon_power_index(new_weapon);
			current_weapon_index= find_weapon_power_index(player_weapons->current_weapon);

			/* If the new weapon is better.. */
			if(new_weapon_index>current_weapon_index)
			{
				if(player_weapons->current_weapon==NONE)
				{
					/* Always change if they didn't have a weapon.. */
					should_change= TRUE;
				} else {
					struct weapon_data *weapon= get_player_current_weapon(player_index);
		
					if(new_weapon==_weapon_ball)
					{
						should_change= TRUE;
					} else {
						/* If this weapon is not charging or charged.. */
						if(weapon->triggers[_primary_weapon].state!=_weapon_charging &&
							weapon->triggers[_primary_weapon].state!=_weapon_charged &&
							weapon->triggers[_secondary_weapon].state!=_weapon_charging &&
							weapon->triggers[_secondary_weapon].state!=_weapon_charged)
						{
							/* If this weapon doesn't have the trigger down... */
							if(!TRIGGER_IS_DOWN(weapon)) should_change= TRUE;
						}
					}
				}
			}
			/* or the current weapon doesn't have ammo */
			else if(!player_weapon_has_ammo(player_index, player_weapons->current_weapon))
			{
				should_change= TRUE;
			}
		}
	}
	
	return should_change;
}

static void calculate_weapon_position_for_idle(
	short player_index,
	short count,
	short weapon_type,
	fixed *height,
	fixed *width)
{
	struct weapon_definition *definition= get_weapon_definition(weapon_type);
	struct player_data *player= get_player_data(player_index);
	fixed horizontal_phase, vertical_angle, bob_height, bob_width;
	short *table;

	if(count==0)
	{
		table= sine_table;
		horizontal_phase= player->variables.step_phase;
		vertical_angle= player->variables.step_phase>>(FIXED_FRACTIONAL_BITS-ANGULAR_BITS);
	} else {
		table= cosine_table;
		vertical_angle= NORMALIZE_ANGLE((player->variables.step_phase>>(FIXED_FRACTIONAL_BITS-ANGULAR_BITS))+HALF_CIRCLE);
		horizontal_phase= player->variables.step_phase;
	}
	/* Weapons are the first thing drawn */
	bob_height= (player->variables.step_amplitude*definition->bob_amplitude)>>FIXED_FRACTIONAL_BITS;
	bob_height= (bob_height*table[vertical_angle])>>TRIG_SHIFT;
	bob_height+= sine_table[player->elevation]<<3;
	*height+= bob_height;

	bob_width= (player->variables.step_amplitude*definition->horizontal_amplitude)>>FIXED_FRACTIONAL_BITS;
	bob_width= (bob_width*table[horizontal_phase>>(FIXED_FRACTIONAL_BITS-ANGULAR_BITS)])>>TRIG_SHIFT;
	*width += bob_width;
}

static void modify_position_for_two_weapons(
	short player_index, 
	short count,
	fixed *width,
	fixed *height)
{
	struct weapon_definition *definition= get_current_weapon_definition(player_index);

	/* Handle modifying the width for twofisted weapons.. */
	switch(definition->weapon_class)
	{
		case _melee_class:
			{
				struct weapon_data *weapon= get_player_current_weapon(player_index);
				struct trigger_definition *trigger_definition;
				short total_delay, delay, breakpoint_delay, sign;
				
				if(PRIMARY_WEAPON_IS_VALID(weapon) && SECONDARY_WEAPON_IS_VALID(weapon))
				{
					switch(count)
					{
						case 0: 
						case 1: 
							sign= (count) ? -1 : 1;
							*width+= (PISTOL_SEPARATION_WIDTH*sign); 
							trigger_definition= get_player_trigger_definition(player_index, !count);
							total_delay= trigger_definition->ticks_per_round+trigger_definition->recovery_ticks;
							breakpoint_delay= (2*total_delay)/3;
							delay= total_delay-weapon->triggers[!count].phase;
							switch(weapon->triggers[!count].state)
							{
								case _weapon_firing: delay-= trigger_definition->recovery_ticks;
								case _weapon_recovering:
									if(delay>=breakpoint_delay)
									{
										/* Delay needs to go from breakpoint_delay to zero */
										delay= total_delay - delay;
										*width+= sign*((FIXED_ONE/8)*delay)/(total_delay-breakpoint_delay);
										*height+= ((FIXED_ONE/16)*delay)/(total_delay-breakpoint_delay);
									} else {
										*width+= sign*((FIXED_ONE/8)*delay)/breakpoint_delay;
										*height+= ((FIXED_ONE/16)*delay)/breakpoint_delay;
									}
									break;
							}
							break;
							
						default: 
							break;
					}
				} else {
					*width+= FIXED_ONE_HALF/2;
				}
			}
			break;

		case _twofisted_pistol_class:
			{
				struct weapon_data *weapon= get_player_current_weapon(player_index);
				
				if(PRIMARY_WEAPON_IS_VALID(weapon) && SECONDARY_WEAPON_IS_VALID(weapon))
				{
					switch(count)
					{
						case 0: *width += PISTOL_SEPARATION_WIDTH/2; break;
						case 1: *width -= PISTOL_SEPARATION_WIDTH/2; break;
						default: break;
					}
				}
			}
			break;

		default:
			break;
	}
}

static void add_random_flutter(
	fixed flutter_base,
	fixed *height, 
	fixed *width)
{
	fixed delta_height, delta_width;

	delta_height= flutter_base>>4;
	delta_width= flutter_base>>6;
	
	/* Note that we MUST use rand() here, and not random() */
	if(delta_height) *height += (rand()%delta_height)-(delta_height/2);
	if(delta_width) *width += (rand()%delta_width)-(delta_width/2);
}				

static void play_weapon_sound(
	short player_index, 
	short sound,
	fixed pitch)
{
	struct player_data *player= get_player_data(player_index);
	struct monster_data *monster= get_monster_data(player->monster_index);
	struct object_data *object= get_object_data(monster->object_index);
	fixed old_pitch= object->sound_pitch;

	object->sound_pitch= pitch;
	play_object_sound(monster->object_index, sound);
	object->sound_pitch= old_pitch;
	
	return;
}

static short get_active_trigger_count_and_states(
	short player_index,
	short weapon_index,
	long action_flags,
	short *first_trigger,
	boolean *triggers_down)
{
	struct weapon_definition *definition= get_weapon_definition(weapon_index);
	short active_count;

	assert(first_trigger);

	if(triggers_down) 
	{
		triggers_down[0]= triggers_down[1]= FALSE;
	}

	(*first_trigger)= _primary_weapon;
	
	switch(definition->weapon_class)
	{
		case _normal_class:
			active_count= 1;
			if(triggers_down)
			{
				if(action_flags & _left_trigger_state || action_flags & _right_trigger_state) triggers_down[0]= TRUE;
			}
			break;
		
		case _melee_class:
		case _twofisted_pistol_class:
			{
				struct player_data *player= get_player_data(player_index);
				struct weapon_data *weapon= get_player_current_weapon(player_index);

				if(player->items[definition->item_type]>1 && 
					PRIMARY_WEAPON_IS_VALID(weapon) && SECONDARY_WEAPON_IS_VALID(weapon))
				{
					if(triggers_down)
					{
						if(action_flags & _left_trigger_state) triggers_down[0]= TRUE;
						if(action_flags & _right_trigger_state) triggers_down[1]= TRUE;
					}
					active_count= 2;
				} else {
					if(SECONDARY_WEAPON_IS_VALID(weapon))
					{
						if(triggers_down)
						{
							if(action_flags & _left_trigger_state) triggers_down[1]= TRUE;
							if(action_flags & _right_trigger_state) triggers_down[1]= TRUE;
						}

						(*first_trigger)= _secondary_weapon;
						active_count= 2;
					} else {
						if(triggers_down)
						{
							if(action_flags & _left_trigger_state) triggers_down[0]= TRUE;
							if(action_flags & _right_trigger_state) triggers_down[0]= TRUE;
						}
						active_count= 1;
					}
				}
			}
			break;
			
		case _dual_function_class:
			if(triggers_down)
			{
				if(dual_function_secondary_has_control(player_index))
				{
					triggers_down[_primary_weapon]= FALSE;
				} else {
					if(action_flags & _left_trigger_state) triggers_down[0]= TRUE;
				}

				/* check the right trigger */
				if(action_flags & _right_trigger_state) triggers_down[1]= TRUE;
			}
			active_count= 2;
			break;
			
		case _multipurpose_class: 
			if(triggers_down)
			{
				if(action_flags & _left_trigger_state) triggers_down[0]= TRUE;
				if(action_flags & _right_trigger_state) triggers_down[1]= TRUE;
			}
			active_count= 2;
			break;
			
		default:
			halt();
			break;
	}

	return active_count;
}

static boolean dual_function_secondary_has_control(
	short player_index)
{
	struct trigger_data *trigger= get_player_trigger_data(player_index, _secondary_weapon);
	boolean secondary_has_control;
		
	switch(trigger->state)
	{
		case _weapon_recovering:
		case _weapon_firing:
		case _weapon_charging:
		case _weapon_charged:
			/* You can't fire your primary until secondary is idle */
			secondary_has_control= TRUE;
			break;
		
		default:
			secondary_has_control= FALSE;
			break;
	}
	
	return secondary_has_control;
}				

/* Called once the shapes are loaded */
static void calculate_ticks_from_shapes(
	void)
{
	short weapon_type;
	
	for(weapon_type= 0; weapon_type<NUMBER_OF_WEAPONS; ++weapon_type)
	{
		struct weapon_definition *definition= get_weapon_definition(weapon_type);

		/* Handle the firing ticks */
		if(definition->weapons_by_trigger[_primary_weapon].ticks_per_round==NONE)
		{
			struct shape_animation_data *high_level_data;
			short total_ticks;
		
			high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection,
				definition->firing_shape));

			total_ticks= high_level_data->ticks_per_frame*high_level_data->frames_per_view;
				
			if(definition->flags & _weapon_is_automatic)
			{
				/* All automatic weapons have no recovery time.. */
				definition->weapons_by_trigger[_primary_weapon].ticks_per_round= total_ticks;
				definition->weapons_by_trigger[_primary_weapon].recovery_ticks= 0;
			} else {
				definition->weapons_by_trigger[_primary_weapon].ticks_per_round= 
					(high_level_data->key_frame+1)*high_level_data->ticks_per_frame;
				definition->weapons_by_trigger[_primary_weapon].recovery_ticks= total_ticks-
					definition->weapons_by_trigger[_primary_weapon].ticks_per_round;
			}

			/* Fixup the secondary trigger.. */
			if(definition->weapons_by_trigger[_secondary_weapon].ticks_per_round== NONE && (definition->flags & _weapon_triggers_share_ammo))
			{
				definition->weapons_by_trigger[_secondary_weapon].ticks_per_round= 
					definition->weapons_by_trigger[_primary_weapon].ticks_per_round;
				definition->weapons_by_trigger[_secondary_weapon].recovery_ticks= 
					definition->weapons_by_trigger[_primary_weapon].recovery_ticks;
			}

			/* Rocky modification */
			if(definition->weapon_class==_melee_class || definition->weapon_class==_twofisted_pistol_class)
			{
				definition->weapons_by_trigger[_secondary_weapon].ticks_per_round= 
					(high_level_data->key_frame+1)*high_level_data->ticks_per_frame;
				definition->weapons_by_trigger[_secondary_weapon].recovery_ticks= total_ticks-
					definition->weapons_by_trigger[_secondary_weapon].ticks_per_round;
			}
		}

		/* Handle the reloading ticks */
		if(definition->reloading_shape != NONE)
		{
			struct shape_animation_data *high_level_data;
			short total_ticks;
		
			high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, 
				definition->reloading_shape));
	
			total_ticks= high_level_data->ticks_per_frame*high_level_data->frames_per_view;
			definition->await_reload_ticks= 0;
			definition->loading_ticks= high_level_data->ticks_per_frame*(high_level_data->key_frame+1)
				- definition->await_reload_ticks;
			definition->finish_loading_ticks= total_ticks - definition->loading_ticks 
				- definition->await_reload_ticks;
		} else {
			/* This shape doesn't have a reloading shape, therefore the reload */
			/*  time is calculated here. */
			short total_ticks= definition->await_reload_ticks + 
								definition->loading_ticks + 
								definition->finish_loading_ticks;
								
			/* Each are set to the same value.. */
			definition->await_reload_ticks= total_ticks/3;
			definition->loading_ticks= total_ticks/3;
			definition->finish_loading_ticks= total_ticks/3;
		}
	}

	return;
}

static void update_automatic_sequence(
	short player_index,
	short which_trigger)
{
	struct weapon_definition *definition= get_current_weapon_definition(player_index);

	if(definition->flags & _weapon_is_automatic)
	{
		if(which_trigger==_primary_weapon || 
			(which_trigger==_secondary_weapon && (definition->flags & _weapon_secondary_has_angular_flipping)))
		{
			struct trigger_data *trigger= get_player_trigger_data(player_index, which_trigger);
			struct shape_animation_data *high_level_data= NULL;
	
			switch(trigger->state)
			{
				case _weapon_firing:
				case _weapon_recovering:
					high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, 
						definition->firing_shape));
					break;
	
				case _weapon_idle:
					if(automatic_still_firing(player_index, which_trigger))
					{
						high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, 
							definition->firing_shape));
					} else {
						trigger->sequence= BUILD_SEQUENCE(0, 0);
					}
					break;
					
				default:
					break;
			}
	
			if(high_level_data)
			{
				short frame;
		
				frame= GET_SEQUENCE_FRAME(trigger->sequence);
				if(++frame>=high_level_data->frames_per_view)
				{
					frame= 0;
				}				
				trigger->sequence= BUILD_SEQUENCE(frame, 0);
			}
		}
	}

	return;
}

/* Automatic weapons should always show every frame of their firing animation */
/* If it is automatic: do nothing for idle/firing/recovery.  frame time: increment frame */
static void update_sequence(
	short player_index, 
	short which_trigger)
{
	struct weapon_definition *definition= get_current_weapon_definition(player_index);
	struct trigger_data *trigger= get_player_trigger_data(player_index, which_trigger);
	struct shape_animation_data *high_level_data= NULL;
	boolean prevent_wrap= FALSE; /* GROSS! */
	fixed pitch= FIXED_ONE;
	short sound_id= NONE;

	switch(trigger->state)
	{
		case _weapon_firing:
		case _weapon_recovering:
			if(which_trigger==_primary_weapon && (definition->flags & _weapon_is_automatic)
				|| (which_trigger==_secondary_weapon && (definition->flags & _weapon_is_automatic) && (definition->flags & _weapon_secondary_has_angular_flipping)))
			{
			} else {
				high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, 
					definition->firing_shape));
			}
			break;

		case _weapon_idle:
			if((which_trigger==_primary_weapon && (definition->flags & _weapon_is_automatic))
			|| (which_trigger==_secondary_weapon && (definition->flags & _weapon_is_automatic) && (definition->flags & _weapon_secondary_has_angular_flipping)))
			{
			} else {
				trigger->sequence= 0;
			}
			break;

		case _weapon_charged:
			{
				struct trigger_definition *trigger_definition= get_player_trigger_definition(player_index, which_trigger);

				high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, 
					definition->charged_shape));

				/* Get the charged sound */
				sound_id= trigger_definition->charged_sound;
					
				/* If this weapon overloads, play with the pitch */
				if(definition->flags & _weapon_overloads)
				{
					pitch= FIXED_ONE+(FIXED_ONE*(CHARGED_WEAPON_OVERLOAD-trigger->phase))/CHARGED_WEAPON_OVERLOAD;
				}
			}
			break;
			
		case _weapon_awaiting_reload:
		case _weapon_waiting_to_load:
		case _weapon_finishing_reload:
			if(definition->reloading_shape!=NONE)
			{
				high_level_data= get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, 
					definition->reloading_shape));
				prevent_wrap= TRUE;
			} else {
				trigger->sequence= 0;
			}
			break;
			
		default:
			trigger->sequence= 0;
			break;
	}

	if(high_level_data)
	{
		short frame, phase;

		frame= GET_SEQUENCE_FRAME(trigger->sequence);
		phase= GET_SEQUENCE_PHASE(trigger->sequence);

		if(++phase>=high_level_data->ticks_per_frame)
		{
			if(++frame>=high_level_data->frames_per_view)
			{
				/* Gross hack, but necessary due to weapons synchronicity problems. */
				if(prevent_wrap)
				{
					frame--;
					assert(trigger->phase==1);
					assert(frame>=0);
				} else {
					frame= 0;
				}
			}

			/* Play the keyframe sound. */
			if(frame==high_level_data->key_frame)
			{
				play_weapon_sound(player_index, sound_id, pitch);
			}
			
			phase= 0;
		}
		trigger->sequence= BUILD_SEQUENCE(frame, phase);
	}

	return;
}

static void blow_up_player(
	short player_index)
{
	struct player_data *player= get_player_data(player_index);
	struct object_data *object= get_object_data(player->object_index);
	struct weapon_definition *definition= get_current_weapon_definition(player_index);

	detonate_projectile(&object->location, object->polygon, _projectile_overloaded_fusion_dispersal,
		player->monster_index, _monster_marine, FIXED_ONE);
	
	return;
}

static boolean get_weapon_data_type_for_count(
	short player_index,
	short count, 
	short *type,
	short *index,
	short *flags)
{
	struct weapon_definition *definition= get_current_weapon_definition(player_index);
	struct weapon_data *weapon= get_player_current_weapon(player_index);
	boolean valid= TRUE;
		
	*flags= 0;
	
	switch(definition->weapon_class)
	{
		case _normal_class:
			switch(count)
			{
				case 0: 
					*type= _weapon_type; 
					*index= _primary_weapon; 
					break;
				default: 
					*type= _shell_casing_type;	
					*index= count-1;
					valid= get_shell_casing_display_data((struct weapon_display_information *) NULL, *index);
					break;
			}
			break;

		case _multipurpose_class:
			switch(count)
			{
				case 0:
					*type= _weapon_type; 
					if(definition->flags & _weapon_secondary_has_angular_flipping)
					{
						switch(weapon->triggers[_secondary_weapon].state)
						{
							case _weapon_idle:
								if(automatic_still_firing(player_index, _secondary_weapon))
								{
									*index= _secondary_weapon;
								} else {
									*index= _primary_weapon;
								}
								break;
								
							case _weapon_firing:
							case _weapon_recovering:
								*index= _secondary_weapon;
								break;
								
							default:
								*index= _primary_weapon;
								break;
						}
					} else {
						*index= _primary_weapon;
					}
					break;

				default: 
					*type= _shell_casing_type;	
					*index= count-1;
					valid= get_shell_casing_display_data((struct weapon_display_information *) NULL, *index);
					break;
			}
			break;

		case _dual_function_class:
			switch(count)
			{
				case 0:
					*type= _weapon_type;
					if(dual_function_secondary_has_control(player_index))
					{
						*index= _secondary_weapon;
					} else {
						*index= _primary_weapon;
					}
					break;
					
				default:
					*type= _shell_casing_type;	
					*index= count-1;
					valid= get_shell_casing_display_data((struct weapon_display_information *) NULL, *index);
					break;
			}
			break;

		case _melee_class:
		case _twofisted_pistol_class:
			{
				struct player_data *player= get_player_data(player_index);
				
				if(player->items[definition->item_type]>1 && 
					PRIMARY_WEAPON_IS_VALID(weapon) && SECONDARY_WEAPON_IS_VALID(weapon))
				{
					switch(count)
					{
						case 0: *type= _weapon_type; *index= _primary_weapon; break;
						case 1: *type= _weapon_type; *index= _secondary_weapon; *flags |= _flip_shape_horizontal; break;
						default:
							*type= _shell_casing_type;	
							*index= count-2;
							valid= get_shell_casing_display_data((struct weapon_display_information *) NULL, *index);
							break;
					}
				} else {
					/* Only have one up */
					switch(count)
					{
						case 0: 
							*type= _weapon_type; 
							if(SECONDARY_WEAPON_IS_VALID(weapon))
							{
								*index= _secondary_weapon; 
								*flags |= _flip_shape_horizontal;
							} else {
								*index= _primary_weapon; 
							}
							break;

						default:
							*type= _shell_casing_type;	
							*index= count-1;
							valid= get_shell_casing_display_data((struct weapon_display_information *) NULL, *index);
							break;
					}
				}
			}
			break;
			
		default:
			halt();
			break;
	}

	return valid;
}

static void update_player_ammo_count(
	short player_index)
{
	if(player_index==current_player_index)
	{
		mark_ammo_display_as_dirty();
	}

	return;
}

static boolean player_has_valid_weapon(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

	return (player_weapons->current_weapon!=NONE);
}

static void idle_weapon(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

	if(player_weapons->desired_weapon != player_weapons->current_weapon)
	{
		if(player_weapons->current_weapon==NONE)
		{
			change_to_desired_weapon(player_index);
		} else {
			struct weapon_data *weapon= get_player_current_weapon(player_index);
			short active_triggers, which_trigger, first_trigger;
			boolean should_change= TRUE;

			active_triggers= get_active_trigger_count_and_states(player_index, 
				player_weapons->current_weapon, 0l, &first_trigger, NULL);

			for(which_trigger= first_trigger; which_trigger<active_triggers; ++which_trigger)
			{
				switch(weapon->triggers[which_trigger].state)
				{
					case _weapon_idle:
					case _weapon_raising:
						break;

					case _weapon_waiting_to_load:
						{
							struct weapon_definition *definition= get_weapon_definition(player_weapons->current_weapon);

							if(definition->reloading_shape==NONE)
							{
								/* Allow them to change if it is completely offscreen */
								weapon->triggers[_primary_weapon].state= _weapon_lowering;
								weapon->triggers[_primary_weapon].phase= 1; 
								weapon->triggers[_primary_weapon].sequence= 0;

								weapon->triggers[_secondary_weapon].state= _weapon_lowering;
								weapon->triggers[_secondary_weapon].phase= 1; 
								weapon->triggers[_secondary_weapon].sequence= 0;
							} else {
								should_change= FALSE;
							}
						}
						break;
					
					default:
						should_change= FALSE;
						break;
				}
			}
			
			if(should_change)
			{
				lower_weapon(player_index, player_weapons->current_weapon);
			}
		}
	} 
	else /* desired==current */
	{
		if(player_weapons->current_weapon!=NONE)
		{
			struct weapon_data *weapon= get_player_current_weapon(player_index);
			short active_triggers, which_trigger, first_trigger;
			boolean should_change= TRUE;
			
			active_triggers= get_active_trigger_count_and_states(player_index, 
				player_weapons->current_weapon, 0l, &first_trigger, NULL);
	
			for(which_trigger= first_trigger; which_trigger<active_triggers; ++which_trigger)
			{
				switch(weapon->triggers[which_trigger].state)
				{
					case _weapon_lowering:
						break;
					
					default:
						should_change= FALSE;
						break;
				}
			}
	
			/* They switched back to this weapon... */
			if(should_change)
			{
				raise_weapon(player_index, player_weapons->current_weapon);
			} else {
				struct weapon_definition *definition= get_current_weapon_definition(player_index);

				/* Check for double weapon activation... */
				if(definition->weapon_class==_twofisted_pistol_class || 
					definition->weapon_class==_melee_class)
				{
					if(WEAPON_WANTS_TWOFIST(weapon))
					{
						should_change= TRUE;
					
						/* Start raising the other one.. */
						for(which_trigger= first_trigger; which_trigger<active_triggers; ++which_trigger)
						{
							switch(weapon->triggers[which_trigger].state)
							{
								case _weapon_idle:
									break;
								
								default:
									should_change= FALSE;
									break;
							}
						}
						
						if(should_change)
						{
							short moving_weapon, raising_weapon;
						
							if(!PRIMARY_WEAPON_IS_VALID(weapon))
							{
								/* Raise the primary.. */
								moving_weapon= _secondary_weapon;
								raising_weapon= _primary_weapon;
								SET_PRIMARY_WEAPON_IS_VALID(weapon, TRUE);
// dprintf("Prim up TF;g");
							} else {
								assert(!SECONDARY_WEAPON_IS_VALID(weapon));
								moving_weapon= _primary_weapon;
								raising_weapon= _secondary_weapon;
								SET_SECONDARY_WEAPON_IS_VALID(weapon, TRUE);
// dprintf("Second up TF;g");
							}
							
							/* Raise the secondary.. */
							weapon->triggers[moving_weapon].state= _weapon_sliding_over_to_second_position;
							weapon->triggers[raising_weapon].state= _weapon_raising;
							weapon->triggers[moving_weapon].phase= definition->ready_ticks;
							weapon->triggers[raising_weapon].phase= definition->ready_ticks;
							SET_WEAPON_WANTS_TWOFIST(weapon, FALSE);
						}
					}
				}
			}
		}	
	}

	return;
}

/* We are requesting to raise the second pistol for a twofisted class... */
static void test_raise_double_weapon(
	short player_index,
	long *action_flags)
{
	struct weapon_definition *definition= get_current_weapon_definition(player_index);
	struct player_data *player= get_player_data(player_index);
	boolean raising= FALSE;
	
	if(definition->weapon_class==_twofisted_pistol_class || definition->weapon_class==_melee_class)
	{
		struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);

		/* Don't switch if it isn't hte current & the desired weapon.. */
		if(player_weapons->desired_weapon==player_weapons->current_weapon)
		{
			struct weapon_data *weapon= get_player_current_weapon(player_index);

			/* Only raise if they have more than one, and both aren't up.. */
			if(player->items[definition->item_type]>1)
			{
				if (((*action_flags) & _left_trigger_state) && !PRIMARY_WEAPON_IS_VALID(weapon))
				{
					struct player_data *player= get_player_data(player_index);
					struct trigger_definition *trigger_definition= 
						get_player_trigger_definition(player_index, _primary_weapon);

					assert(SECONDARY_WEAPON_IS_VALID(weapon));
						
					/* Try to raise the secondary weapon.. */
					if(weapon->triggers[_primary_weapon].rounds_loaded || 
						trigger_definition->ammunition_type==NONE ||
						player->items[trigger_definition->ammunition_type]>0)
					{
//dprintf("1 twofist;g");
						SET_WEAPON_WANTS_TWOFIST(weapon, TRUE);
						(*action_flags) &= ~_left_trigger_state;
					}
				} 
				else if (((*action_flags) & _right_trigger_state) && !SECONDARY_WEAPON_IS_VALID(weapon))
				{
					struct player_data *player= get_player_data(player_index);
					struct trigger_definition *trigger_definition= 
						get_player_trigger_definition(player_index, _secondary_weapon);

					assert(PRIMARY_WEAPON_IS_VALID(weapon));

					/* Try to raise the primary weapon.. */
					if(weapon->triggers[_secondary_weapon].rounds_loaded || 
						trigger_definition->ammunition_type==NONE ||
						player->items[trigger_definition->ammunition_type]>0)
					{
//dprintf("2 twofist;g");
						SET_WEAPON_WANTS_TWOFIST(weapon, TRUE);
						(*action_flags) &= ~_right_trigger_state;
					}
				}
			}
		}
	}

	return;
}

static void change_to_desired_weapon(
	short player_index)
{
	struct player_weapon_data *player_weapons= get_player_weapon_data(player_index);
	struct weapon_definition *definition= get_weapon_definition(player_weapons->desired_weapon);
	short first_trigger, which_trigger, trigger_count;

// dprintf("Changing!");	
	assert(player_weapons->desired_weapon != player_weapons->current_weapon);
	
	/* Reset this weapons flags */
	if(player_weapons->current_weapon!=NONE)
	{
		player_weapons->weapons[player_weapons->current_weapon].flags= 0;
	}

	/* Reset the desired weapon flags. */
	player_weapons->weapons[player_weapons->desired_weapon].flags= 0;
	
	/* Set the desired weapon. */
	player_weapons->current_weapon= player_weapons->desired_weapon;

	/* How many triggers do I need to update? */
	switch(definition->weapon_class)
	{
		case _normal_class:
			first_trigger= _primary_weapon;
			trigger_count= 1; 
			break;
			
		case _multipurpose_class:
		case _dual_function_class:
			first_trigger= _primary_weapon;
			trigger_count= 2; 
			break;

		case _melee_class:
		case _twofisted_pistol_class:
			/* We only ever raise one of them. */
			/* If the first one has ammunition, we raise it.  If they have */
			/* two of them, and the first one doesn't have ammunition, then */
			/* we raise the second one. */
			{
				struct player_data *player= get_player_data(player_index);
				if(player->items[definition->item_type]==1)
				{
					/* Only can raise the first one. */
					first_trigger= _primary_weapon;
					trigger_count= 1;
				} else {
					struct trigger_data *trigger= get_trigger_data(player_index, 
						player_weapons->desired_weapon, _primary_weapon);
					struct trigger_definition *trigger_definition= get_trigger_definition(player_index, 
						player_weapons->desired_weapon, _primary_weapon);
					
					/* If it is loaded or we can load it... */
					if(trigger->rounds_loaded || trigger_definition->ammunition_type==NONE || 
						player->items[trigger_definition->ammunition_type]>0)
					{
						first_trigger= _primary_weapon;
						trigger_count= 1;
					} else {
						assert(get_trigger_data(player_index, player_weapons->desired_weapon, _secondary_weapon)->rounds_loaded);
						first_trigger= _secondary_weapon;
						trigger_count= 2;
					}
				}
			}
			break;
		
		default:
			halt();
			break;
	}

	/* Essentially the same as raise weapon, but it is always a full sequence */
	for(which_trigger= first_trigger; which_trigger<trigger_count; ++which_trigger)
	{
		struct trigger_data *trigger= get_trigger_data(player_index, player_weapons->desired_weapon, which_trigger);
		struct trigger_definition *trigger_definition= get_trigger_definition(player_index, 
			player_weapons->desired_weapon, which_trigger);

		trigger->phase= definition->ready_ticks;
		trigger->state= _weapon_raising;

		if(which_trigger==_primary_weapon)
		{
			SET_PRIMARY_WEAPON_IS_VALID(&player_weapons->weapons[player_weapons->desired_weapon], TRUE);
		} else {
			SET_SECONDARY_WEAPON_IS_VALID(&player_weapons->weapons[player_weapons->desired_weapon], TRUE);
		}

		/* if it has no ammunition, load it- only if it has an offscreen reload. */
		if(!trigger->rounds_loaded && definition->reloading_shape==NONE)
		{
			struct trigger_definition *trigger_definition= 
				get_trigger_definition(player_index, player_weapons->current_weapon, which_trigger);
			struct player_data *player= get_player_data(player_index);
			
			if(player->items[trigger_definition->ammunition_type] > 0)
			{
				put_rounds_into_weapon(player_index, player_weapons->current_weapon, which_trigger);
			}
		}
	}

	return;
}

static boolean automatic_still_firing(
	short player_index,
	short which_trigger)
{
	boolean still_firing= FALSE;
	struct weapon_data *weapon= get_player_current_weapon(player_index);
	struct weapon_definition *definition= get_weapon_definition(weapon->weapon_type);

	if(which_trigger==_primary_weapon ||
		(which_trigger==_secondary_weapon && (definition->flags & _weapon_secondary_has_angular_flipping)))
	{
		assert(weapon->triggers[which_trigger].state==_weapon_idle);
		if(definition->flags & _weapon_is_automatic)
		{
			if(TRIGGER_IS_DOWN(weapon) && weapon->triggers[which_trigger].ticks_since_last_shot<AUTOMATIC_STILL_FIRING_DURATION)
			{
				still_firing= TRUE;
			}
		}
	}
	
	return still_firing;
}

static void	play_shell_casing_sound(
	short player_index, 
	short sound_index)
{
	struct player_data *player= get_player_data(player_index);
	
	if (!(player->variables.flags&_FEET_BELOW_MEDIA_BIT))
	{
		struct world_location3d location;
		
		location.point= player->camera_location;
		location.polygon_index= player->camera_polygon_index;
		location.yaw= location.pitch= 0;
		location.velocity.i= location.velocity.j= location.velocity.k= 0;
	
		play_sound(sound_index, &location, NONE);
	}

	return;
}

static short find_weapon_power_index(
	short weapon_type)
{
	short index;
	
	if(weapon_type==NONE)
	{
		index= NONE;
	} else {
		for(index= 0; index<NUMBER_OF_WEAPONS; ++index)
		{
			if(weapon_type==weapon_ordering_array[index]) break;
		}
		assert(index!=NUMBER_OF_WEAPONS);
	}
	
	return index;
}

/* ---------- shell casing stuff */

static void initialize_shell_casings(
	short player_index)
{
	short shell_casing_index;
	struct shell_casing_data *shell_casing;
	
	for (shell_casing_index= 0, shell_casing= get_player_weapon_data(player_index)->shell_casings; shell_casing_index<MAXIMUM_SHELL_CASINGS; ++shell_casing_index, ++shell_casing)
	{
		MARK_SLOT_AS_FREE(shell_casing);
	}
	
	return;
}

static short new_shell_casing(
	short player_index,
	short type,
	word flags)
{
	short shell_casing_index;
	struct shell_casing_data *shell_casing;
	
	for (shell_casing_index= 0, shell_casing= get_player_weapon_data(player_index)->shell_casings; shell_casing_index<MAXIMUM_SHELL_CASINGS; ++shell_casing_index, ++shell_casing)
	{
		if (SLOT_IS_FREE(shell_casing)) break;
	}
	
	if (shell_casing_index==MAXIMUM_SHELL_CASINGS)
	{
		shell_casing_index= NONE;
	}
	else
	{
		struct shell_casing_definition *definition= get_shell_casing_definition(type);
		
		shell_casing->type= type;
		shell_casing->flags= flags;
		shell_casing->x= definition->x0, shell_casing->y= definition->y0;
		shell_casing->vx= SHELL_CASING_IS_REVERSED(shell_casing) ? -definition->vx0 : definition->vx0, shell_casing->vy= definition->vy0;
		
		shell_casing->frame= local_random()&7;
		shell_casing->x+= ((local_random()&0xff)*shell_casing->vx)>>9;
		shell_casing->y+= ((local_random()&0xff)*shell_casing->vy)>>9;

		MARK_SLOT_AS_USED(shell_casing);
	}

	return shell_casing_index;
}

static void update_shell_casings(
	short player_index)
{
	short shell_casing_index;
	struct shell_casing_data *shell_casing;

	for (shell_casing_index= 0, shell_casing= get_player_weapon_data(player_index)->shell_casings; shell_casing_index<MAXIMUM_SHELL_CASINGS; ++shell_casing_index, ++shell_casing)
	{
		if (SLOT_IS_USED(shell_casing))
		{
			struct shell_casing_definition *definition= get_shell_casing_definition(shell_casing->type);
			
			shell_casing->x+= shell_casing->vx;
			shell_casing->y+= shell_casing->vy;
			shell_casing->vx+= SHELL_CASING_IS_REVERSED(shell_casing) ? -definition->dvx : definition->dvx;
			shell_casing->vy+= definition->dvy;
			
			if (shell_casing->x>=FIXED_ONE || shell_casing->x<0)
			{
				MARK_SLOT_AS_FREE(shell_casing);
			}
		}
	}

	return;
}

static boolean get_shell_casing_display_data(
	struct weapon_display_information *display,
	short index)
{
	boolean valid= FALSE;
	short shell_casing_index;
	struct shell_casing_data *shell_casing;
	
	for (shell_casing_index= 0, shell_casing= get_player_weapon_data(current_player_index)->shell_casings; shell_casing_index<MAXIMUM_SHELL_CASINGS; ++shell_casing_index, ++shell_casing)
	{
		if (SLOT_IS_USED(shell_casing))
		{
			if ((index-= 1)<0)
			{
				if (display)
				{
					struct shell_casing_definition *definition= get_shell_casing_definition(shell_casing->type);
					struct shape_animation_data *high_level_data=
						get_shape_animation_data(BUILD_DESCRIPTOR(definition->collection, definition->shape));
					
					// animate it
					if ((shell_casing->frame+= 1)>=high_level_data->frames_per_view) shell_casing->frame= 0;
					
					display->collection= definition->collection;
					display->low_level_shape_index= high_level_data->low_level_shape_indexes[shell_casing->frame];
					display->flip_horizontal= display->flip_vertical= FALSE;
					display->vertical_positioning_mode= display->horizontal_positioning_mode= _position_center;
					display->vertical_position= FIXED_ONE-shell_casing->y, display->horizontal_position= shell_casing->x;
					display->transfer_mode= _xfer_normal, display->transfer_phase= 0;
				}
				
				valid= TRUE;
				break;
			}
		}
	}
	
	return valid;
}

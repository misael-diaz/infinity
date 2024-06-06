/*
PLAYER.H
Sunday, July 10, 1994 10:07:21 PM
*/

/* ---------- constants */

#ifdef DEMO
#define MAXIMUM_NUMBER_OF_PLAYERS 2
#else
#define MAXIMUM_NUMBER_OF_PLAYERS 8
#endif

enum
{
	NUMBER_OF_ITEMS= 64
};

#define NATURAL_LIGHT_INTENSITY FIXED_ONE_HALF

enum /* physics models */
{
	_editor_model,
	_earth_gravity_model,
	_low_gravity_model /* cyberspace? */
};

enum /* player actions; irrelevant if the player is dying or something */
{
	_player_stationary,
	_player_walking,
	_player_running,
	_player_sliding,
	_player_airborne,
	NUMBER_OF_PLAYER_ACTIONS
};

enum /* team colors */
{
	_violet_team,
	_red_team,
	_tan_team,
	_light_blue_team,
	_yellow_team,
	_brown_team,
	_blue_team,
	_green_team,
	NUMBER_OF_TEAM_COLORS
};

/* ---------- action flags */

#define ABSOLUTE_YAW_BITS 7
#define MAXIMUM_ABSOLUTE_YAW (1<<ABSOLUTE_YAW_BITS)
#define GET_ABSOLUTE_YAW(i) (((i)>>(_absolute_yaw_mode_bit+1))&(MAXIMUM_ABSOLUTE_YAW-1))
#define SET_ABSOLUTE_YAW(i,y) (((i)&~((MAXIMUM_ABSOLUTE_YAW-1)<<(_absolute_yaw_mode_bit+1))) | ((y)<<(_absolute_yaw_mode_bit+1)))

#define ABSOLUTE_PITCH_BITS 5
#define MAXIMUM_ABSOLUTE_PITCH (1<<ABSOLUTE_PITCH_BITS)
#define GET_ABSOLUTE_PITCH(i) (((i)>>(_absolute_pitch_mode_bit+1))&(MAXIMUM_ABSOLUTE_PITCH-1))
#define SET_ABSOLUTE_PITCH(i,y) (((i)&~((MAXIMUM_ABSOLUTE_PITCH-1)<<(_absolute_pitch_mode_bit+1))) | ((y)<<(_absolute_pitch_mode_bit+1)))

#define ABSOLUTE_POSITION_BITS 7
#define MAXIMUM_ABSOLUTE_POSITION (1<<ABSOLUTE_POSITION_BITS)
#define GET_ABSOLUTE_POSITION(i) (((i)>>(_absolute_position_mode_bit+1))&(MAXIMUM_ABSOLUTE_POSITION-1))
#define SET_ABSOLUTE_POSITION(i,y) (((i)&~((MAXIMUM_ABSOLUTE_POSITION-1)<<(_absolute_position_mode_bit+1)))|((y)<<(_absolute_position_mode_bit+1)))

enum /* action flag bit offsets */
{
	_absolute_yaw_mode_bit,
	_turning_left_bit,
	_turning_right_bit,
	_sidestep_dont_turn_bit,
	_looking_left_bit,
	_looking_right_bit,
	_absolute_yaw_bit0,
	_absolute_yaw_bit1,
 
	_absolute_pitch_mode_bit,
	_looking_up_bit,
	_looking_down_bit,
	_looking_center_bit,
	_absolute_pitch_bit0,
	_absolute_pitch_bit1,

	_absolute_position_mode_bit,
	_moving_forward_bit,
	_moving_backward_bit,
	_run_dont_walk_bit,
	_look_dont_turn_bit,
	_absolute_position_bit0,
	_absolute_position_bit1,
	_absolute_position_bit2,

	_sidestepping_left_bit,
	_sidestepping_right_bit,
	_left_trigger_state_bit,
	_right_trigger_state_bit,
	_action_trigger_state_bit,
	_cycle_weapons_forward_bit,
	_cycle_weapons_backward_bit,
	_toggle_map_bit,
	_microphone_button_bit,
	_swim_bit,
	
	NUMBER_OF_ACTION_FLAG_BITS /* should be <=32 */
};

#define _override_absolute_yaw (_turning_left|_turning_right|_looking_left|_looking_right)
#define _override_absolute_pitch (_looking_up|_looking_down|_looking_center)
#define _override_absolute_position (_moving_forward|_moving_backward)

enum /* action_flags */
{
	_absolute_yaw_mode= 1<<_absolute_yaw_mode_bit,
	_turning_left= 1<<_turning_left_bit,
	_turning_right= 1<<_turning_right_bit,
	_sidestep_dont_turn= 1<<_sidestep_dont_turn_bit,
	_looking_left= 1<<_looking_left_bit,
	_looking_right= 1<<_looking_right_bit,

	_absolute_pitch_mode= 1<<_absolute_pitch_mode_bit,
	_looking_up= 1<<_looking_up_bit,
	_looking_down= 1<<_looking_down_bit,
	_looking_center= 1<<_looking_center_bit,
	_look_dont_turn= 1<<_look_dont_turn_bit,

	_absolute_position_mode= 1<<_absolute_position_mode_bit,
	_moving_forward= 1<<_moving_forward_bit,
	_moving_backward= 1<<_moving_backward_bit,
	_run_dont_walk= 1<<_run_dont_walk_bit,

	_sidestepping_left= 1<<_sidestepping_left_bit,
	_sidestepping_right= 1<<_sidestepping_right_bit,
	_left_trigger_state= 1<<_left_trigger_state_bit,
	_right_trigger_state= 1<<_right_trigger_state_bit,
	_action_trigger_state= 1<<_action_trigger_state_bit,
	_cycle_weapons_forward= 1<<_cycle_weapons_forward_bit,
	_cycle_weapons_backward= 1<<_cycle_weapons_backward_bit,
	_toggle_map= 1<<_toggle_map_bit,
	_microphone_button= 1<<_microphone_button_bit,
	_swim= 1<<_swim_bit,

	_turning= _turning_left|_turning_right,
	_looking= _looking_left|_looking_right,
	_moving= _moving_forward|_moving_backward,
	_sidestepping= _sidestepping_left|_sidestepping_right,
	_looking_vertically= _looking_up|_looking_down|_looking_center
};

/* ---------- structures */

enum /* player flag bits */
{
	_RECENTERING_BIT= 0x8000,
	_ABOVE_GROUND_BIT= 0x4000,
	_BELOW_GROUND_BIT= 0x2000,
	_FEET_BELOW_MEDIA_BIT= 0x1000,
	_HEAD_BELOW_MEDIA_BIT= 0x0800,
	_STEP_PERIOD_BIT= 0x0400
};

struct physics_variables
{
	fixed head_direction;
	fixed last_direction, direction, elevation, angular_velocity, vertical_angular_velocity;
	fixed velocity, perpendicular_velocity; /* in and perpendicular to direction, respectively */
	fixed_point3d last_position, position;
	fixed actual_height;

	/* used by mask_in_absolute_positioning_information (because it is not really absolute) to
		keep track of where we�re going */
	fixed adjusted_pitch, adjusted_yaw;
	
	fixed_vector3d external_velocity; /* from impacts; slowly absorbed */
	fixed external_angular_velocity; /* from impacts; slowly absorbed */
	
	fixed step_phase; /* step_phase is in [0,1) and is some function of the distance travelled
		(for bobbing the gun and the viewpoint) */
	fixed step_amplitude; /* step amplitude is in [0,1) and is some function of velocity */
	
	fixed floor_height; /* the height of the floor on the polygon where we ended up last time */
	fixed ceiling_height; /* same as above, but ceiling height */
	fixed media_height; /* media height */

	short action; /* what the player�s legs are doing, basically */
	word old_flags, flags; /* stuff like _RECENTERING */
};

enum { /* Player flags */
	_player_is_interlevel_teleporting_flag= 0x0100,
	_player_has_cheated_flag= 0x0200,
	_player_is_teleporting_flag= 0x0400,	
	_player_has_map_open_flag= 0x0800,	
	_player_is_totally_dead_flag= 0x1000,
	_player_is_zombie_flag= 0x2000, // IS THIS USED??
	_player_is_dead_flag= 0x4000
};

#define PLAYER_IS_DEAD(p) ((p)->flags&_player_is_dead_flag)
#define SET_PLAYER_DEAD_STATUS(p,v) ((v)?((p)->flags|=(word)_player_is_dead_flag):((p)->flags&=(word)~_player_is_dead_flag))

#define PLAYER_IS_ZOMBIE(p) ((p)->flags&_player_is_zombie_flag)
#define SET_PLAYER_ZOMBIE_STATUS(p,v) ((v)?((p)->flags|=(word)_player_is_zombie_flag):((p)->flags&=(word)~_player_is_zombie_flag))

/* i.e., our animation has stopped */
#define PLAYER_IS_TOTALLY_DEAD(p) ((p)->flags&_player_is_totally_dead_flag)
#define SET_PLAYER_TOTALLY_DEAD_STATUS(p,v) ((v)?((p)->flags|=(word)_player_is_totally_dead_flag):((p)->flags&=(word)~_player_is_totally_dead_flag))

#define PLAYER_HAS_MAP_OPEN(p) ((p)->flags&_player_has_map_open_flag)
#define SET_PLAYER_MAP_STATUS(p,v) ((v)?((p)->flags|=(word)_player_has_map_open_flag):((p)->flags&=(word)~_player_has_map_open_flag))

#define PLAYER_IS_TELEPORTING(p) ((p)->flags&_player_is_teleporting_flag)
#define SET_PLAYER_TELEPORTING_STATUS(p,v) ((v)?((p)->flags|=(word)_player_is_teleporting_flag):((p)->flags&=(word)~_player_is_teleporting_flag))

#define PLAYER_IS_INTERLEVEL_TELEPORTING(p) ((p)->flags&_player_is_interlevel_teleporting_flag)
#define SET_PLAYER_INTERLEVEL_TELEPORTING_STATUS(p,v) ((v)?((p)->flags|=(word)_player_is_interlevel_teleporting_flag):((p)->flags&=(word)~_player_is_interlevel_teleporting_flag))

#define PLAYER_HAS_CHEATED(p) ((p)->flags&_player_has_cheated_flag)
#define SET_PLAYER_HAS_CHEATED(p) ((p)->flags|=(word)_player_has_cheated_flag)

#define PLAYER_MAXIMUM_SUIT_ENERGY (150)
#define PLAYER_MAXIMUM_SUIT_OXYGEN (6*TICKS_PER_MINUTE)

#define MAXIMUM_PLAYER_NAME_LENGTH 32

#define PLAYER_TELEPORTING_DURATION TELEPORTING_DURATION
#define PLAYER_TELEPORTING_MIDPOINT TELEPORTING_MIDPOINT

struct damage_record
{
	long damage;
	short kills;
};

struct player_data
{
	short identifier;
	short flags; /* [unused.1] [dead.1] [zombie.1] [totally_dead.1] [map.1] [teleporting.1] [unused.10] */

	short color;
	short team;
	char name[MAXIMUM_PLAYER_NAME_LENGTH+1];
	
	/* shadowed from physics_variables structure below and the player�s object (read-only) */
	world_point3d location;
	world_point3d camera_location; // beginning of fake world_location3d structure
	short camera_polygon_index;
	angle facing, elevation;
	short supporting_polygon_index; /* what polygon is actually supporting our weight */
	short last_supporting_polygon_index;

	/* suit energy shadows vitality in the player�s monster slot */
	short suit_energy, suit_oxygen;
	
	short monster_index; /* this player�s entry in the monster list */
	short object_index; /* monster->object_index */
	
	/* Reset by initialize_player_weapons */
	short weapon_intensity_decay; /* zero is idle intensity */
	fixed weapon_intensity;

	/* powerups */
	short invisibility_duration;
	short invincibility_duration;
	short infravision_duration;
	short extravision_duration;

	/* teleporting */
	short delay_before_teleport; /* This is only valid for interlevel teleports (teleporting_destination is a negative number) */
	short teleporting_phase; /* NONE means no teleporting, otherwise [0,TELEPORTING_PHASE) */
	short teleporting_destination; /* level number or NONE if intralevel transporter */
	short interlevel_teleport_phase; /* This is for the other players when someone else initiates the teleport */

	/* there is no state information associated with items; each item slot is only a count */
	short items[NUMBER_OF_ITEMS];

	/* Used by the game window code to keep track of the interface state. */
	short interface_flags;
	short interface_decay;

	struct physics_variables variables;

	struct damage_record total_damage_given;
	struct damage_record damage_taken[MAXIMUM_NUMBER_OF_PLAYERS];
	struct damage_record monster_damage_taken, monster_damage_given;

	short reincarnation_delay;

	short control_panel_side_index; // NONE, or the side index of a control panel the user is using that requires passage of time
	
	long ticks_at_last_successful_save;

	long netgame_parameters[2];
	
	short unused[256];
};

/* ---------- globals */

extern struct player_data *players;

/* use set_local_player_index() and set_current_player_index() to change these! */
extern short local_player_index, current_player_index;
extern struct player_data *local_player, *current_player;

/* ---------- prototypes/PLAYER.C */

void initialize_players(void);
void reset_player_queues(void);
void allocate_player_memory(void);

void set_local_player_index(short player_index);
void set_current_player_index(short player_index);

short new_player(short team, short color, short player_identifier);
void delete_player(short player_number);

void recreate_players_for_new_level(void);

void update_players(void); /* assumes �t==1 tick */

void walk_player_list(void);

void queue_action_flags(short player_index, long *action_flags, short count);
long dequeue_action_flags(short player_index);
short get_action_queue_size(short player_index);

void damage_player(short monster_index, short aggressor_index, short aggressor_type,
	struct damage_definition *damage);

void mark_player_collections(boolean loading);

short player_identifier_to_player_index(short player_identifier);

#ifdef DEBUG
struct player_data *get_player_data(short player_index);
#else
#define get_player_data(i) (players+(i))
#endif

short monster_index_to_player_index(short monster_index);

short get_polygon_index_supporting_player(short player_index);

boolean legal_player_powerup(short player_index, short item_index);
void process_player_powerup(short player_index, short item_index);

world_distance dead_player_minimum_polygon_height(short polygon_index);

boolean try_and_subtract_player_item(short player_index, short item_type);

/* ---------- prototypes/PHYSICS.C */

void initialize_player_physics_variables(short player_index);
void update_player_physics_variables(short player_index, long action_flags);

void adjust_player_for_polygon_height_change(short monster_index, short polygon_index, world_distance new_floor_height,
	world_distance new_ceiling_height);
void accelerate_player(short monster_index, world_distance vertical_velocity, angle direction, world_distance velocity);

void kill_player_physics_variables(short player_index);

long mask_in_absolute_positioning_information(long action_flags, fixed yaw, fixed pitch, fixed velocity);
void get_absolute_pitch_range(fixed *minimum, fixed *maximum);

void instantiate_absolute_positioning_information(short player_index, fixed facing, fixed elevation);
void get_binocular_vision_origins(short player_index, world_point3d *left, short *left_polygon_index,
	angle *left_angle, world_point3d *right, short *right_polygon_index, angle *right_angle);

fixed get_player_forward_velocity_scale(short player_index);

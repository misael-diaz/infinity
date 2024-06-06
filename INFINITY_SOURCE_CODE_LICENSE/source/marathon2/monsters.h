/*
MONSTERS.H
Tuesday, June 28, 1994 7:07:59 PM
*/

/* ---------- constants */

#define FLAMING_DEAD_SHAPE BUILD_DESCRIPTOR(_collection_rocket, 7)
#define FLAMING_DYING_SHAPE BUILD_DESCRIPTOR(_collection_rocket, 8)

enum /* constants for activate_nearby_monsters */
{
	_pass_one_zone_border= 0x0001,
	_passed_zone_border= 0x0002,
	_activate_invisible_monsters= 0x0004, // sound or teleport trigger
	_activate_deaf_monsters= 0x0008, // i.e., trigger
	_pass_solid_lines= 0x0010, // i.e., not a sound (trigger)
	_use_activation_biases= 0x0020, // inactive monsters follow their editor instructions (trigger)
	_activation_cannot_be_avoided= 0x0040 // cannot be suppressed because of recent activation (trigger)
};

/* activation biases are only used when the monster is activated by a trigger */
enum /* activation biases (set in editor) */
{
	_activate_on_player,
	_activate_on_nearest_hostile,
	_activate_on_goal,
	_activate_randomly
};

/* ---------- monsters */

#define MAXIMUM_MONSTERS_PER_MAP 220

/* player monsters are never active */

#define MONSTER_IS_PLAYER(m) ((m)->type==_monster_marine)
enum /* monster types */
{
	_monster_marine,
	_monster_tick_energy,
	_monster_tick_oxygen,
	_monster_tick_kamakazi,
	_monster_compiler_minor,
	_monster_compiler_major,
	_monster_compiler_minor_invisible,
	_monster_compiler_major_invisible,
	_monster_fighter_minor,
	_monster_fighter_major,
	_monster_fighter_minor_projectile,
	_monster_fighter_major_projectile,
	_civilian_crew,
	_civilian_science,
	_civilian_security,
	_civilian_assimilated,
	_monster_hummer_minor, // slow hummer
	_monster_hummer_major, // fast hummer
	_monster_hummer_big_minor, // big hummer
	_monster_hummer_big_major, // angry hummer
	_monster_hummer_possessed, // hummer from durandal
	_monster_cyborg_minor,
	_monster_cyborg_major,
	_monster_cyborg_flame_minor,
	_monster_cyborg_flame_major,
	_monster_enforcer_minor,
	_monster_enforcer_major,
	_monster_hunter_minor,
	_monster_hunter_major,
	_monster_trooper_minor,
	_monster_trooper_major,
	_monster_mother_of_all_cyborgs,
	_monster_mother_of_all_hunters,
	_monster_sewage_yeti,
	_monster_water_yeti,
	_monster_lava_yeti,
	_monster_defender_minor,
	_monster_defender_major,
	_monster_juggernaut_minor,
	_monster_juggernaut_major,
	_monster_tiny_fighter,
	_monster_tiny_bob,
	_monster_tiny_yeti,
	_vacuum_civilian_crew,
	_vacuum_civilian_science,
	_vacuum_civilian_security,
	_vacuum_civilian_assimilated,
	NUMBER_OF_MONSTER_TYPES
};

/* uses SLOT_IS_USED(), SLOT_IS_FREE(), MARK_SLOT_AS_FREE(), MARK_SLOT_AS_USED() macros (0x8000 bit) */

#define MONSTER_NEEDS_PATH(m) ((m)->flags&(word)0x4000)
#define SET_MONSTER_NEEDS_PATH_STATUS(m,v) ((v)?((m)->flags|=(word)0x4000):((m)->flags&=(word)~0x4000))

/* the recovering from hit flag is set randomly after a monster finishes his being-hit
	animation, and when set it prevents him from being immediately dragged into another
	being-hit animation.  this makes monsters twitch when being pinned down by a big gun,
	and allows them a small chance to react.  the flag is cleared every frame. */
#define MONSTER_RECOVERING_FROM_HIT(m) ((m)->flags&(word)0x2000)
#define CLEAR_MONSTER_RECOVERING_FROM_HIT(m) ((m)->flags&=(word)~0x2000)
#define SET_MONSTER_RECOVERING_FROM_HIT(m) ((m)->flags|=(word)0x2000)

#define MONSTER_IS_ACTIVE(m) ((m)->flags&(word)0x1000)
#define SET_MONSTER_ACTIVE_STATUS(m,v) ((v)?((m)->flags|=(word)0x1000):((m)->flags&=(word)~0x1000))

/* berserk monsters will only switch targets when their target dies and then choose the
	geometerically closest monster.  what sets this bit is still unclear.  */
#define MONSTER_IS_BERSERK(m) ((m)->flags&(word)0x0400)
#define SET_MONSTER_BERSERK_STATUS(m,v) ((v)?((m)->flags|=(word)0x0400):((m)->flags&=(word)~0x0400))

#define MONSTER_IS_IDLE(m) ((m)->flags&(word)0x0800)
#define SET_MONSTER_IDLE_STATUS(m,v) ((v)?((m)->flags|=(word)0x0800):((m)->flags&=(word)~0x0800))

/* this flag is set if our current target has inflicted damage on us (because if he hasn�t, we�ll
	probably go after somebody else if they hit us first) */
#define TARGET_HAS_DONE_DAMAGE(m) ((m)->flags&(word)0x0200)
#define CLEAR_TARGET_DAMAGE_FLAG(m) ((m)->flags&=(word)~0x0200)
#define SET_TARGET_DAMAGE_FLAG(m) ((m)->flags|=(word)0x0200)

#define SET_MONSTER_HAS_BEEN_ACTIVATED(m) ((m)->flags&=(word)~_monster_has_never_been_activated)

#define MONSTER_IS_BLIND(m) ((m)->flags&(word)_monster_is_blind)
#define MONSTER_IS_DEAF(m) ((m)->flags&(word)_monster_is_deaf)
#define MONSTER_TELEPORTS_OUT_WHEN_DEACTIVATED(m) ((m)->flags&(word)_monster_teleports_out_when_deactivated)

#define MONSTER_IS_DYING(m) ((m)->action==_monster_is_dying_hard||(m)->action==_monster_is_dying_soft||(m)->action==_monster_is_dying_flaming)
#define MONSTER_IS_ATTACKING(m) ((m)->action==_monster_is_attacking_close||(m)->action==_monster_is_attacking_far)
#define MONSTER_IS_TELEPORTING(m) ((m)->action==_monster_is_teleporting)
enum /* monster actions */
{
	_monster_is_stationary,
	_monster_is_waiting_to_attack_again,
	_monster_is_moving,
	_monster_is_attacking_close, /* melee */
	_monster_is_attacking_far, /* ranged */
	_monster_is_being_hit,
	_monster_is_dying_hard,
	_monster_is_dying_soft,
	_monster_is_dying_flaming,
	_monster_is_teleporting, // transparent
	_monster_is_teleporting_in,
	_monster_is_teleporting_out,
	NUMBER_OF_MONSTER_ACTIONS
};

#define MONSTER_IS_LOCKED(m) ((m)->mode==_monster_locked||(m)->mode==_monster_losing_lock)
#define MONSTER_HAS_VALID_TARGET(m) (MONSTER_IS_ACTIVE(m)&&MONSTER_IS_LOCKED(m))
enum /* monster modes */
{
	_monster_locked,
	_monster_losing_lock,
	_monster_lost_lock,
	_monster_unlocked,
	_monster_running,
	NUMBER_OF_MONSTER_MODES
};

enum /* monster flags */
{
	_monster_was_promoted= 0x1,
	_monster_was_demoted= 0x2,
	_monster_has_never_been_activated= 0x4,
	_monster_is_blind= 0x8,
	_monster_is_deaf= 0x10,
	_monster_teleports_out_when_deactivated= 0x20
};

struct monster_data /* 64 bytes */
{
	short type;
	short vitality; /* if ==NONE, will be properly initialized when the monster is first activated */
	word flags; /* [slot_used.1] [need_path.1] [recovering_from_hit.1] [active.1] [idle.1] [berserk.1] [target_damage.1] [unused.6] [never_activated.1] [demoted.1] [promoted.1] */
	
	short path; /* NONE is no path (the need path bit should be set in this case) */
	world_distance path_segment_length; /* distance until we�re through with this segment of the path */
	world_distance desired_height;
	
	short mode, action;
	short target_index; /* a monster_index */
	world_distance external_velocity; /* per tick, in the direction -facing, only updated during hit/death animations */
	world_distance vertical_velocity; /* per tick, is rarely positive; absorbed immediately on contact with ground */
	short ticks_since_attack, attack_repetitions;
	short changes_until_lock_lost; /* number of times more the target can change polygons until _losing_lock becomes _lost_lock */

	world_distance elevation; /* valid when attacking; z-component of projectile vector */

	short object_index;
	
	long ticks_since_last_activation;

	short activation_bias;
	
	short goal_polygon_index; // used instead of NONE when generating random paths

	// copied from monster�s object every tick but updated with height
	world_point3d sound_location;
	short sound_polygon_index;

	short random_desired_height;
	
	short unused[7];
};

/* ---------- globals */

extern struct monster_data *monsters;

/* ---------- prototypes/MONSTERS.C */

void initialize_monsters(void);
void initialize_monsters_for_new_level(void); /* when a map is loaded */

void move_monsters(void); /* assumes �t==1 tick */

short new_monster(struct object_location *location, short monster_code);
void remove_monster(short monster_index);

void activate_monster(short monster_index);
void deactivate_monster(short monster_index);
short find_closest_appropriate_target(short aggressor_index, boolean full_circle);

void mark_monster_collections(short type, boolean loading);
void load_monster_sounds(short monster_type);

void monster_moved(short target_index, short old_polygon_index);
short legal_monster_move(short monster_index, angle facing, world_point3d *new_location);
short legal_player_move(short monster_index, world_point3d *new_location, world_distance *object_floor);

#define LOCAL_INTERSECTING_MONSTER_BUFFER_SIZE 16
#define GLOBAL_INTERSECTING_MONSTER_BUFFER_SIZE 64
boolean possible_intersecting_monsters(short *object_indexes, short *object_count, short maximum_object_count, short polygon_index, boolean include_scenery);
#define monsters_nearby(polygon_index) possible_intersecting_monsters(0, 0, 0, (polygon_index), FALSE)

void get_monster_dimensions(short monster_index, world_distance *radius, world_distance *height);

void activate_nearby_monsters(short target_index, short caller_index, short flags);

void damage_monsters_in_radius(short primary_target_index, short aggressor_index, short aggressor_type,
	world_point3d *epicenter, short epicenter_polygon_index, world_distance radius, struct damage_definition *damage);
void damage_monster(short monster_index, short aggressor_index, short aggressor_type, world_point3d *epicenter, struct damage_definition *damage);

#ifdef DEBUG
struct monster_data *get_monster_data(short monster_index);
#else
#define get_monster_data(i) (monsters+(i))
#endif

boolean bump_monster(short aggressor_index, short monster_index);

boolean legal_polygon_height_change(short polygon_index, world_distance new_floor_height, world_distance new_ceiling_height, struct damage_definition *damage);
void adjust_monster_for_polygon_height_change(short monster_index, short polygon_index, world_distance new_floor_height, world_distance new_ceiling_height);
void accelerate_monster(short monster_index, angle direction, angle elevation, world_distance velocity);

void monster_died(short target_index);

#ifdef OBSOLETE
	void try_and_drop_random_monsters(void);
#endif

short monster_placement_index(short monster_type);
short placement_index_to_monster_type(short placement_index);
void try_to_add_random_monster(short monster_type, boolean activate);

short get_monster_impact_effect(short monster_index);
short get_monster_melee_impact_effect(short monster_index);

boolean live_aliens_on_map(void);

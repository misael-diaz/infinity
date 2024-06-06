#ifndef __PROJECTILES_H__
#define __PROJECTILES_H__

/*
PROJECTILES.H
Tuesday, June 28, 1994 7:12:00 PM
*/

/* ---------- projectile structure */

#define MAXIMUM_PROJECTILES_PER_MAP 32

enum /* projectile types */
{
	_projectile_rocket,
	_projectile_grenade,
	_projectile_pistol_bullet,
	_projectile_rifle_bullet,
	_projectile_shotgun_bullet,
	_projectile_staff,
	_projectile_staff_bolt,
	_projectile_flamethrower_burst,
	_projectile_compiler_bolt_minor,
	_projectile_compiler_bolt_major,
	_projectile_alien_weapon,
	_projectile_fusion_bolt_minor,
	_projectile_fusion_bolt_major,
	_projectile_hunter,
	_projectile_fist,
	_projectile_unused,
	_projectile_armageddon_electricity,
	_projectile_juggernaut_rocket,
	_projectile_trooper_bullet,
	_projectile_trooper_grenade,
	_projectile_minor_defender,
	_projectile_major_defender,
	_projectile_juggernaut_missile,
	_projectile_minor_energy_drain,
	_projectile_major_energy_drain,
	_projectile_oxygen_drain,
	_projectile_minor_hummer,
	_projectile_major_hummer,
	_projectile_durandal_hummer,
	_projectile_minor_cyborg_ball,
	_projectile_major_cyborg_ball,
	_projectile_ball,
	_projectile_minor_fusion_dispersal,
	_projectile_major_fusion_dispersal,
	_projectile_overloaded_fusion_dispersal,
	_projectile_yeti,
	_projectile_sewage_yeti,
	_projectile_lava_yeti,
	_projectile_smg_bullet,
	NUMBER_OF_PROJECTILE_TYPES
};

#define PROJECTILE_HAS_MADE_A_FLYBY(p) ((p)->flags&(word)0x4000)
#define SET_PROJECTILE_FLYBY_STATUS(p,v) ((v)?((p)->flags|=(word)0x4000):((p)->flags&=(word)~0x4000))

/* only used for persistent projectiles */
#define PROJECTILE_HAS_CAUSED_DAMAGE(p) ((p)->flags&(word)0x2000)
#define SET_PROJECTILE_DAMAGE_STATUS(p,v) ((v)?((p)->flags|=(word)0x2000):((p)->flags&=(word)~0x2000))

#define PROJECTILE_HAS_CROSSED_MEDIA_BOUNDARY(p) ((p)->flags&(word)0x1000)
#define SET_PROJECTILE_CROSSED_MEDIA_BOUNDARY_STATUS(p,v) ((v)?((p)->flags|=(word)0x1000):((p)->flags&=(word)~0x1000))

/* uses SLOT_IS_USED(), SLOT_IS_FREE(), MARK_SLOT_AS_FREE(), MARK_SLOT_AS_USED() macros (0x8000 bit) */

struct projectile_data /* 32 bytes */
{
	short type;
	
	short object_index;

	short target_index; /* for guided projectiles, the current target index */

	angle elevation; /* facing is stored in the projectile�s object */
	
	short owner_index; /* ownerless if NONE */
	short owner_type; /* identical to the monster type which fired this projectile (valid even if owner==NONE) */
	word flags; /* [slot_used.1] [played_flyby_sound.1] [has_caused_damage.1] [unused.13] */
	
	/* some projectiles leave n contrails effects every m ticks */
	short ticks_since_last_contrail, contrail_count;

	world_distance distance_travelled;
	
	world_distance gravity; /* velocity due to gravity for projectiles affected by it */
	
	fixed damage_scale;
	
	short permutation; /* item type if we create one */
	
	short unused[2];
};

/* ---------- globals */

extern struct projectile_data *projectiles;

/* ---------- prototypes/PROJECTILES.C */

boolean preflight_projectile(world_point3d *origin, short polygon_index, world_point3d *vector,
	angle delta_theta, short type, short owner, short owner_type, short *target_index);
short new_projectile(world_point3d *origin, short polygon_index, world_point3d *vector,
	angle delta_theta, short type, short owner_index, short owner_type, short intended_target_index,
	fixed damage_scale);
void detonate_projectile(world_point3d *origin, short polygon_index, short type,
	short owner_index, short owner_type, fixed damage_scale);

void move_projectiles(void); /* assumes �t==1 tick */

void remove_projectile(short projectile_index);
void remove_all_projectiles(void);

void orphan_projectiles(short monster_index);

void mark_projectile_collections(short type, boolean loading);
void load_projectile_sounds(short type);

void drop_the_ball(world_point3d *origin, short polygon_index, short owner_index,
	short owner_type, short item_type);

#ifdef DEBUG
struct projectile_data *get_projectile_data(short projectile_index);
#else
#define get_projectile_data(i) (projectiles+(i))
#endif

#endif
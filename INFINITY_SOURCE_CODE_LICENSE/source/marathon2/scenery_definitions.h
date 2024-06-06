/*
SCENERY_DEFINITIONS.H
Sunday, September 25, 1994 4:05:35 AM  (Jason')
*/

/* ---------- constants */

enum
{
	_scenery_is_solid= 0x0001,
	_scenery_is_animated= 0x0002,
	_scenery_can_be_destroyed= 0x0004
};

/* ---------- structures */

struct scenery_definition
{
	word flags;
	shape_descriptor shape;
	
	world_distance radius, height;
	
	short destroyed_effect;
	shape_descriptor destroyed_shape;
};

/* ---------- globals */

#define NUMBER_OF_SCENERY_DEFINITIONS (sizeof(scenery_definitions)/sizeof(struct scenery_definition))
struct scenery_definition scenery_definitions[]=
{
	// lava
	{0, BUILD_DESCRIPTOR(_collection_scenery2, 3)}, // light dirt
	{0, BUILD_DESCRIPTOR(_collection_scenery2, 4)}, // dark dirt
	{0, BUILD_DESCRIPTOR(_collection_scenery2, 5)}, // bones
	{0, BUILD_DESCRIPTOR(_collection_scenery2, 6)}, // bone
	{0, BUILD_DESCRIPTOR(_collection_scenery2, 7)}, // ribs
	{0, BUILD_DESCRIPTOR(_collection_scenery2, 8)}, // skull
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery2, 9), WORLD_ONE/8, -WORLD_ONE/8, _effect_lava_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery2, 19)}, // hanging light
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery2, 10), WORLD_ONE/8, -WORLD_ONE/8, _effect_lava_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery2, 20)}, // hanging light
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery2, 12), WORLD_ONE/8, WORLD_ONE_HALF}, // small cylinder
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery2, 13), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // large cylinder
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery2, 14), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // block
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery2, 15), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // block
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery2, 16), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // block
	
	// water
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 4)}, // pistol clip
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery1, 5), WORLD_ONE/6, -WORLD_ONE/8, _effect_water_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery1, 6)}, // short light
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery1, 7), WORLD_ONE/8, -WORLD_ONE/8, _effect_water_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery1, 8)}, // long light
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery1, 9), WORLD_ONE/4, -WORLD_ONE/6, _effect_grenade_explosion, BUILD_DESCRIPTOR(_collection_scenery1, 23)}, // siren
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 10)}, // rocks
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 21)}, // blood drops
	{_scenery_is_animated, BUILD_DESCRIPTOR(_collection_scenery1, 11)}, // water thing
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 12)}, // gun
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 13)}, // bob remains
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 14)}, // puddles
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 15)}, // big puddles
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery1, 16), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // security monitor
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery1, 17), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // alien supply can
	{_scenery_is_animated, BUILD_DESCRIPTOR(_collection_scenery1, 18)}, // machine
	{0, BUILD_DESCRIPTOR(_collection_scenery1, 20)}, // fighter�s staff

	// sewage
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery3, 5), WORLD_ONE/6, -WORLD_ONE/8, _effect_sewage_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery3, 6)}, // stubby green light
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery3, 7), WORLD_ONE/6, -WORLD_ONE/8, _effect_sewage_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery3, 8)}, // long green light
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 4)}, // junk
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 9)}, // big antenna
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 10)}, // big antenna
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery3, 11), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // alien supply can
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 13)}, // bones
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 17)}, // big bones
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 12)}, // pfhor pieces
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 14)}, // bob pieces
	{0, BUILD_DESCRIPTOR(_collection_scenery3, 15)}, // bob blood

	// alien
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery5, 4), WORLD_ONE/6, -WORLD_ONE/8, _effect_alien_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery5, 5)}, // green light
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery5, 14), WORLD_ONE/6, -WORLD_ONE/8, _effect_alien_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery5, 15)}, // small alien light
	{_scenery_is_solid|_scenery_can_be_destroyed, BUILD_DESCRIPTOR(_collection_scenery5, 16), WORLD_ONE/6, -WORLD_ONE/8, _effect_alien_lamp_breaking, BUILD_DESCRIPTOR(_collection_scenery5, 17)}, // alien ceiling rod light
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 6)}, // bulbous yellow alien object
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 7)}, // square grey organic object
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 9)}, // pfhor skeleton
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 10)}, // pfhor mask
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 11)}, // green stuff
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 12)}, // hunter shield
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 13)}, // bones
	{0, BUILD_DESCRIPTOR(_collection_scenery5, 18)}, // alien sludge

	// jjaro
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery4, 5), WORLD_ONE/6, -WORLD_ONE/8, NONE, BUILD_DESCRIPTOR(_collection_scenery3, 6)},
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery4, 7), WORLD_ONE/8, WORLD_ONE, NONE, BUILD_DESCRIPTOR(_collection_scenery3, 8)}, // long green light
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 4)}, // junk
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 9)}, // big antenna
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 10)}, // big antenna
	{_scenery_is_solid, BUILD_DESCRIPTOR(_collection_scenery4, 11), WORLD_ONE_FOURTH, WORLD_ONE_HALF}, // alien supply can
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 13)}, // bones
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 17)}, // big bones
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 12)}, // pfhor pieces
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 14)}, // bob pieces
	{0, BUILD_DESCRIPTOR(_collection_scenery4, 15)}, // bob blood
};

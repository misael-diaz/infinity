/*
	TAGS.H
	Sunday, July 3, 1994 5:33:15 PM

	This is a list of all of the tags used by code that uses the wad file format. 
	One tag, KEY_TAG, has special meaning, and KEY_TAG_SIZE must be set to the 
	size of an index entry.  Each wad can only have one index entry.  You can get the
	index entry from a wad, or from all of the wads in the file easily.
	
	Marathon uses the KEY_TAG as the name of the level.
*/

#define MAXIMUM_LEVEL_NAME_SIZE 64

/* OSTypes.. */
#define APPLICATION_CREATOR '26.�'
#define SCENARIO_FILE_TYPE 'sce2'
#define SAVE_GAME_TYPE 'sga�'
#define FILM_FILE_TYPE 'fil�'
#define PHYSICS_FILE_TYPE 'phy�'
#define SHAPES_FILE_TYPE 'shp�'
#define SOUNDS_FILE_TYPE 'snd�'
#define PATCH_FILE_TYPE 'pat2'

/* Other tags-  */
#define POINT_TAG 'PNTS'
#define LINE_TAG 'LINS'
#define SIDE_TAG 'SIDS'
#define POLYGON_TAG 'POLY'
#define LIGHTSOURCE_TAG 'LITE'
#define ANNOTATION_TAG 'NOTE'
#define OBJECT_TAG 'OBJS'
#define GUARDPATH_TAG 'p�th'
#define MAP_INFO_TAG 'Minf'
#define ITEM_PLACEMENT_STRUCTURE_TAG 'plac'
#define DOOR_EXTRA_DATA_TAG 'door'
#define PLATFORM_STATIC_DATA_TAG 'plat'
#define ENDPOINT_DATA_TAG 'EPNT'
#define MEDIA_TAG 'medi'
#define AMBIENT_SOUND_TAG 'ambi'
#define RANDOM_SOUND_TAG 'bonk'
#define TERMINAL_DATA_TAG 'term'

/* Save/Load game tags. */
#define PLAYER_STRUCTURE_TAG 'plyr'
#define DYNAMIC_STRUCTURE_TAG 'dwol'
#define OBJECT_STRUCTURE_TAG 'mobj'
#define DOOR_STRUCTURE_TAG 'door'
#define MAP_INDEXES_TAG 'iidx'
#define AUTOMAP_LINES 'alin'
#define AUTOMAP_POLYGONS 'apol'
#define MONSTERS_STRUCTURE_TAG 'mOns'
#define EFFECTS_STRUCTURE_TAG 'fx  '
#define PROJECTILES_STRUCTURE_TAG 'bang'
#define PLATFORM_STRUCTURE_TAG 'PLAT'
#define WEAPON_STATE_TAG 'weap'
#define TERMINAL_STATE_TAG 'cint'

/* Physix model tags */
#define MONSTER_PHYSICS_TAG 'MNpx'
#define EFFECTS_PHYSICS_TAG 'FXpx'
#define PROJECTILE_PHYSICS_TAG 'PRpx'
#define PHYSICS_PHYSICS_TAG 'PXpx'
#define WEAPONS_PHYSICS_TAG 'WPpx'

/* Preferences Tags.. */
#define prefGRAPHICS_TAG 'graf'
#define prefSERIAL_TAG 'serl'
#define prefNETWORK_TAG 'netw'
#define prefPLAYER_TAG 'plyr'
#define prefINPUT_TAG 'inpu'
#define prefSOUND_TAG 'snd '
#define prefENVIRONMENT_TAG 'envr'
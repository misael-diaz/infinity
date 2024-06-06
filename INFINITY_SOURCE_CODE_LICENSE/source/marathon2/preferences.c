/*

	preferences.c
	Tuesday, June 13, 1995 10:02:50 AM- rdm created.

*/

#include "macintosh_cseries.h"

#include "map.h"
#include "shell.h" /* For the screen_mode structure */
#include "interface.h"
#include "game_sound.h"

#include "preferences.h"
#include "wad.h"
#include "wad_prefs.h"
#include "game_errors.h"
#include "network.h" // for _ethernet, etc.
#include "find_files.h"
#include "game_wad.h" // for set_map_file
#include "screen.h"
#include "fades.h"
#include "extensions.h"
#include "screen_definitions.h"

#include "tags.h"

#ifdef mpwc
	#pragma segment dialogs
#endif

enum {
	ditlGRAPHICS= 4001,
	iCHOOSE_MONITOR=1,
	iDRAW_EVERY_OTHER_LINE,
	iHARDWARE_ACCELERATION,
	iNUMBER_OF_COLORS,
	iWINDOW_SIZE,
	iDETAIL,
	iBRIGHTNESS,
	iRESOLUTION_SWITCHING	
};

enum {
	ditlPLAYER= 4002,
	iDIFFICULTY_LEVEL= 1,
	iNAME,
	iCOLOR,
	iTEAM
};

enum {
	ditlSOUND= 4003,
	iSTEREO= 1,
	iACTIVE_PANNING,
	iHIGH_QUALITY,
	iAMBIENT_SOUND,
	iVOLUME,
	iCHANNELS,
	iMORE_SOUNDS
};

#ifdef SUPPORT_INPUT_SPROCKET
enum {
	ditlINPUT= 4020,
	iMOUSE_CONTROL= 1,
	iKEYBOARD_CONTROL,
	iINPUT_SPROCKET_CONTROL,
	iSET_KEYS,
	iINPUT_SPROCKET_STATIC_TEXT
};
#else
enum {
	ditlINPUT= 4004,
	iMOUSE_CONTROL= 1,
	iKEYBOARD_CONTROL,
	iSET_KEYS
};
#endif

enum {
	ditlENVIRONMENT= 4005,
	iMAP= 1,
	iPHYSICS,
	iSHAPES,
	iSOUNDS,
	iPATCHES_LIST
};

enum {
	strPREFERENCES_GROUPS= 139,
	graphicsGroup= 0,
	playerGroup,
	soundGroup,
	inputGroup,
	environmentGroup
};

struct graphics_preferences_data *graphics_preferences;
struct serial_number_data *serial_preferences= NULL;
struct network_preferences_data *network_preferences;
struct player_preferences_data *player_preferences;
struct input_preferences_data *input_preferences;
struct sound_manager_parameters *sound_preferences;
struct environment_preferences_data *environment_preferences;

/* ----------- private prototypes */
static void default_graphics_preferences(struct graphics_preferences_data *preferences);
static boolean validate_graphics_preferences(struct graphics_preferences_data *preferences);
static void default_serial_number_preferences(struct serial_number_data *prefs);
static boolean validate_serial_number_preferences(struct serial_number_data *prefs);
static void default_network_preferences(struct network_preferences_data *preferences);
static boolean validate_network_preferences(struct network_preferences_data *prefs);
static void default_player_preferences(struct player_preferences_data *prefs);
static boolean validate_player_preferences(struct player_preferences_data *prefs);
static void default_input_preferences(struct input_preferences_data *preferences);
static boolean validate_input_preferences(struct input_preferences_data *prefs);
static void *get_graphics_pref_data(void);
static void setup_graphics_dialog(DialogPtr dialog, short first_item, void *prefs);
static void hit_graphics_item(DialogPtr dialog, short first_item, void *prefs, short item_hit);
static boolean teardown_graphics_dialog(DialogPtr dialog, short first_item, void *prefs);
static void *get_player_pref_data(void);
static void setup_player_dialog(DialogPtr dialog, short first_item, void *prefs);
static void hit_player_item(DialogPtr dialog, short first_item, void *prefs, short item_hit);
static boolean teardown_player_dialog(DialogPtr dialog, short first_item, void *prefs);
static void *get_sound_pref_data(void);
static void setup_sound_dialog(DialogPtr dialog, short first_item, void *prefs);
static void hit_sound_item(DialogPtr dialog, short first_item, void *prefs, short item_hit);
static boolean teardown_sound_dialog(DialogPtr dialog, short first_item, void *prefs);
static void *get_input_pref_data(void);
static void setup_input_dialog(DialogPtr dialog, short first_item, void *prefs);
static void hit_input_item(DialogPtr dialog, short first_item, void *prefs, short item_hit);
static boolean teardown_input_dialog(DialogPtr dialog, short first_item, void *prefs);
static void set_popup_enabled_state(DialogPtr dialog, short item_number, short item_to_affect,
	boolean enabled);
static void default_environment_preferences(struct environment_preferences_data *prefs);
static boolean validate_environment_preferences(struct environment_preferences_data *prefs);
static void *get_environment_pref_data(void);
static void setup_environment_dialog(DialogPtr dialog, short first_item, void *prefs);
static void hit_environment_item(DialogPtr dialog, short first_item, void *prefs, short item_hit);
static boolean teardown_environment_dialog(DialogPtr dialog, short first_item, void *prefs);
static void fill_in_popup_with_filetype(DialogPtr dialog, short item, OSType type, unsigned long checksum);

#ifndef VULCAN
static MenuHandle get_popup_menu_handle(DialogPtr dialog, short item);
#endif

static boolean allocate_extensions_memory(void);
static void free_extensions_memory(void);
static boolean increase_extensions_memory(void);
static void build_extensions_list(void);
static void search_from_directory(FSSpec *file);
static unsigned long find_checksum_and_file_spec_from_dialog(DialogPtr dialog, 
	short item_hit, OSType type, FSSpec *file);
static void	rebuild_patchlist(DialogPtr dialog, short item, unsigned long parent_checksum,
	struct environment_preferences_data *preferences);
static unsigned long get_file_modification_date(FSSpec *file);

/* ---------------- code */
void initialize_preferences(
	void)
{
	OSErr err;

	if(!w_open_preferences_file(getpstr(temporary, strFILENAMES, filenamePREFERENCES),
		PREFERENCES_TYPE))
	{
		/* Major memory error.. */
		alert_user(fatalError, strERRORS, outOfMemory, MemError());
	}

	if(error_pending())
	{
		short type;
		
		err= get_game_error(&type);
		dprintf("Er: %d type: %d", err, type);
		set_game_error(systemError, noErr);
	}
	
	/* If we didn't open, we initialized.. */
	graphics_preferences= (struct graphics_preferences_data *)get_graphics_pref_data();
	player_preferences= (struct player_preferences_data *)get_player_pref_data();
	input_preferences= (struct input_preferences_data *)get_input_pref_data();
	sound_preferences= (struct sound_manager_parameters *)get_sound_pref_data();
	serial_preferences= w_get_data_from_preferences(prefSERIAL_TAG,
		sizeof(struct serial_number_data), default_serial_number_preferences,
		validate_serial_number_preferences);
	network_preferences= w_get_data_from_preferences(prefNETWORK_TAG,
		sizeof(struct network_preferences_data), default_network_preferences,
		validate_network_preferences);
	environment_preferences= (struct environment_preferences_data *)get_environment_pref_data();
}

struct preferences_dialog_data prefs_data[]={
	{ strPREFERENCES_GROUPS, graphicsGroup, ditlGRAPHICS, get_graphics_pref_data, setup_graphics_dialog, hit_graphics_item, teardown_graphics_dialog },
	{ strPREFERENCES_GROUPS, playerGroup, ditlPLAYER, get_player_pref_data, setup_player_dialog, hit_player_item, teardown_player_dialog },
	{ strPREFERENCES_GROUPS, soundGroup, ditlSOUND, get_sound_pref_data, setup_sound_dialog, hit_sound_item, teardown_sound_dialog },
	{ strPREFERENCES_GROUPS, inputGroup, ditlINPUT, get_input_pref_data, setup_input_dialog, hit_input_item, teardown_input_dialog },
#ifndef DEMO
	{ strPREFERENCES_GROUPS, environmentGroup, ditlENVIRONMENT, get_environment_pref_data, setup_environment_dialog, hit_environment_item, teardown_environment_dialog }
#endif
};
#define NUMBER_OF_PREFS_PANELS (sizeof(prefs_data)/sizeof(struct preferences_dialog_data))

void handle_preferences(
	void)
{
	/* Save the existing preferences, in case we have to reload them. */
	write_preferences();

	if(set_preferences(prefs_data, NUMBER_OF_PREFS_PANELS, initialize_preferences))
	{
		/* Save the new ones. */
		write_preferences();
		set_sound_manager_parameters(sound_preferences);
#ifndef DEMO
		load_environment_from_preferences();
#endif
	}
	
	return;
}

void write_preferences(
	void)
{
	OSErr err;
	w_write_preferences_file();

	if(error_pending())
	{
		short type;
		
		err= get_game_error(&type);
		dprintf("Er: %d type: %d", err, type);
		set_game_error(systemError, noErr);
	}
}

/* ------------- private prototypes */
static void default_graphics_preferences(
	struct graphics_preferences_data *preferences)
{
	preferences->device_spec.slot= NONE;
	preferences->device_spec.flags= deviceIsColor;
	preferences->device_spec.bit_depth= 8;
	preferences->device_spec.width= 640;
	preferences->device_spec.height= 480;

	preferences->screen_mode.gamma_level= DEFAULT_GAMMA_LEVEL;
	if (hardware_acceleration_code(&preferences->device_spec) == _valkyrie_acceleration)
	{
		preferences->screen_mode.size= _100_percent;
		preferences->screen_mode.bit_depth = 16;
		preferences->screen_mode.high_resolution = FALSE;
		preferences->screen_mode.acceleration = _valkyrie_acceleration;
	}
	else if (system_information->machine_is_68k)
	{
		preferences->screen_mode.size= _100_percent;
		preferences->screen_mode.high_resolution= FALSE;
		preferences->screen_mode.acceleration = _no_acceleration;
		preferences->screen_mode.bit_depth = 8;
	}
	else // we got a good machine
	{
		preferences->screen_mode.size= _100_percent;
		preferences->screen_mode.high_resolution= TRUE;
		preferences->screen_mode.acceleration = _no_acceleration;
		preferences->screen_mode.bit_depth = 8;
	}

	preferences->unused[0]= 0;
	preferences->unused[1]= 0;
	preferences->unused[2]= 0;
#ifdef envppc
	preferences->do_resolution_switching= machine_has_display_manager();
#else
	preferences->do_resolution_switching= FALSE;
#endif

	preferences->screen_mode.draw_every_other_line= FALSE;
}

static boolean validate_graphics_preferences(
	struct graphics_preferences_data *preferences)
{
	boolean changed= FALSE;

	if(preferences->screen_mode.gamma_level<0 || preferences->screen_mode.gamma_level>=NUMBER_OF_GAMMA_LEVELS)
	{
		preferences->screen_mode.gamma_level= DEFAULT_GAMMA_LEVEL;
		changed= TRUE;
	}

	if (preferences->screen_mode.acceleration==_valkyrie_acceleration)
	{
		if (hardware_acceleration_code(&preferences->device_spec) != _valkyrie_acceleration)
		{
			preferences->screen_mode.size= _100_percent;
			preferences->screen_mode.bit_depth = 8;
			preferences->screen_mode.high_resolution = FALSE;
			preferences->screen_mode.acceleration = _no_acceleration;
			changed= TRUE;
		} else {
			if(preferences->screen_mode.high_resolution)
			{
				preferences->screen_mode.high_resolution= FALSE;
				changed= TRUE;
			}
			
			if(preferences->screen_mode.bit_depth != 16)
			{
				preferences->screen_mode.bit_depth= 16;
				changed= TRUE;
			}
			
			if(preferences->screen_mode.draw_every_other_line)
			{
				preferences->screen_mode.draw_every_other_line= FALSE;
				changed= TRUE;
			}
		}
	}

	if (preferences->screen_mode.bit_depth==32 && !machine_supports_32bit(&preferences->device_spec)) 
	{
		preferences->screen_mode.bit_depth= 16;
		changed= TRUE;
	}

	/* Don't change out of 16 bit if we are in valkyrie mode. */	
	if (preferences->screen_mode.acceleration!=_valkyrie_acceleration
		&& preferences->screen_mode.bit_depth==16 && !machine_supports_16bit(&preferences->device_spec)) 
	{
		preferences->screen_mode.bit_depth= 8;
		changed= TRUE;
	}

	return changed;
}

static void default_serial_number_preferences(
	struct serial_number_data *prefs)
{
#if !defined(DEMO) && !defined(TRILOGY) && (defined(GAMMA) || defined(FINAL))
	if (!serial_preferences)
#endif
	{
		memset(prefs, 0, sizeof(struct serial_number_data));

#if !defined(DEMO) && !defined(TRILOGY) && (defined(GAMMA) || defined(FINAL))
		/* This has to be done here, because ask_for_serial number expects the global */
		/* variable to be valid. */
		serial_preferences= prefs;
		ask_for_serial_number();
#endif
	}
#if !defined(DEMO) && !defined(TRILOGY) && (defined(GAMMA) || defined(FINAL))
	else
	{
		// this will ask them for us if the number is bad
		memcpy(prefs, serial_preferences, sizeof(struct serial_number_data));
 		validate_serial_number_preferences(prefs);
	}
#endif
}

#if !defined(DEMO) && !defined(TRILOGY) && (defined(GAMMA) || defined(FINAL))
#define DECODE_ONLY
#include "serial_numbers.c"
#endif

static boolean validate_serial_number_preferences(
	struct serial_number_data *prefs)
{
#if !defined(DEMO) && !defined(TRILOGY) && (defined(GAMMA) || defined(FINAL))
	boolean success= TRUE;
	byte short_serial_number[BYTES_PER_SHORT_SERIAL_NUMBER];
	byte inferred_pad[BYTES_PER_SHORT_SERIAL_NUMBER];
	
	long_serial_number_to_short_serial_number_and_pad(prefs->long_serial_number, short_serial_number, inferred_pad);

	if ((!PADS_ARE_EQUAL(actual_pad, inferred_pad) &&
		!(PADS_ARE_EQUAL(actual_pad_m2, inferred_pad) && ((char) short_serial_number[2])<0)) ||
		!VALID_INVERSE_SEQUENCE(short_serial_number))
	{
		success= FALSE;
	}
	

/*	dprintf("player #%d (%08x%08x%04x) %d %d;g;", i, *(long*)player1_long_serial_number,
		*(long*)(player1_long_serial_number+4), *(short*)(player1_long_serial_number+8),
		found_duplicate, found_illegal);
*/

	if (!success)
	{
		/* This has to be done here, because ask_for_serial number expects the global */
		/* variable to be valid. */
		serial_preferences= prefs;
		ask_for_serial_number();
		success= TRUE;
	}
	else
	{
		success= FALSE;
	}
	
	return success;
#else
#pragma unused (prefs)
	return FALSE;
#endif
}

/* -------------- network preferences */
static boolean ethernet_active(void);

static void default_network_preferences(
	struct network_preferences_data *preferences)
{
	preferences->type= _ethernet;

	preferences->allow_microphone = TRUE;
	preferences->game_is_untimed = FALSE;
	preferences->difficulty_level = 2;
	preferences->game_options =	_multiplayer_game | _ammo_replenishes | _weapons_replenish
		| _specials_replenish |	_monsters_replenish | _burn_items_on_death | _suicide_is_penalized 
		| _force_unique_teams | _live_network_stats;
	preferences->time_limit = 10 * TICKS_PER_SECOND * 60;
	preferences->kill_limit = 10;
	preferences->entry_point= 0;
	preferences->game_type= _game_of_kill_monsters;
	
	return;
}

static boolean validate_network_preferences(
	struct network_preferences_data *prefs)
{
	boolean changed= FALSE;

	if(prefs->type<0||prefs->type>_ethernet)
	{
		if(ethernet_active())
		{
			prefs->type= _ethernet;
		} else {
			prefs->type= _localtalk;
		}
		changed= TRUE;
	}
	
	if(prefs->game_is_untimed != TRUE && prefs->game_is_untimed != FALSE)
	{
		prefs->game_is_untimed= FALSE;
		changed= TRUE;
	}

	if(prefs->allow_microphone != TRUE && prefs->allow_microphone != FALSE)
	{
		prefs->allow_microphone= TRUE;
		changed= TRUE;
	}

	if(prefs->game_type<0 || prefs->game_type >= NUMBER_OF_GAME_TYPES)
	{
		prefs->game_type= _game_of_kill_monsters;
		changed= TRUE;
	}
	
	return changed;
}

/* ------------- player preferences */
static void get_name_from_system(char *name);

static void default_player_preferences(
	struct player_preferences_data *prefs)
{
	memset(prefs, 0, sizeof(struct player_preferences_data));

	GetDateTime(&prefs->last_time_ran);
	prefs->difficulty_level= 2;
	get_name_from_system(prefs->name);
	
	return;
}

static boolean validate_player_preferences(
	struct player_preferences_data *prefs)
{
#pragma unused (prefs)
	return FALSE;
}

#define strUSER_NAME -16096

static void get_name_from_system(
	char *name)
{
	Handle name_handle;
	char old_state;

	name_handle= (Handle)GetString(strUSER_NAME);
	if (name_handle)
	{
		old_state= HGetState(name_handle);
		HLock(name_handle);
		
		pstrcpy(name, *name_handle);
		HSetState(name_handle, old_state);
	}
	else
	{
		name[0]= 0;
	}

	return;
}

/* ------------ input preferences */
static void default_input_preferences(
	struct input_preferences_data *preferences)
{
	boolean	is_powerbook_keyboard= FALSE;
	long	kbd_type;
	
	if (Gestalt(gestaltKeyboardType, &kbd_type)==noErr)
	{
		switch (kbd_type)
		{
			case gestaltPwrBookADBKbd:
			case gestaltPwrBookISOADBKbd:
			case gestaltPwrBkExtISOKbd:
			case gestaltPwrBkExtJISKbd:
			case gestaltPwrBkExtADBKbd:
				is_powerbook_keyboard= TRUE;
				break;
		}	
	}
	preferences->input_device= _keyboard_or_game_pad;
	set_default_keys(preferences->keycodes, (is_powerbook_keyboard ? _powerbook_keyboard_setup : _standard_keyboard_setup));
}

static boolean validate_input_preferences(
	struct input_preferences_data *prefs)
{
#pragma unused (prefs)
	return FALSE;
}

static boolean ethernet_active(
	void)
{
	short  refnum;
	OSErr  error;

	error= OpenDriver("\p.ENET", &refnum);
	
	return error==noErr ? TRUE : FALSE;
}

/* ------------- dialog functions */

/* --------- graphics */
static void *get_graphics_pref_data(
	void)
{
	return w_get_data_from_preferences(prefGRAPHICS_TAG,
		sizeof(struct graphics_preferences_data), default_graphics_preferences,
		validate_graphics_preferences);
}

enum {
	_hundreds_colors_menu_item= 1,
	_thousands_colors_menu_item,
	_millions_colors_menu_item
};

static void setup_graphics_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	struct graphics_preferences_data *preferences= (struct graphics_preferences_data *) prefs;
	short value, active;

	if(machine_supports_32bit(&preferences->device_spec))
	{
		active= TRUE;
	} else {
		if(preferences->screen_mode.bit_depth==32) preferences->screen_mode.bit_depth= 16;
		active= FALSE;
	}
	set_popup_enabled_state(dialog, LOCAL_TO_GLOBAL_DITL(iNUMBER_OF_COLORS, first_item), _millions_colors_menu_item, active);

	if(machine_supports_16bit(&preferences->device_spec))
	{
		active= TRUE;
	} else {
		if(preferences->screen_mode.bit_depth==16) preferences->screen_mode.bit_depth= 8;
		active= FALSE;
	}
	set_popup_enabled_state(dialog, LOCAL_TO_GLOBAL_DITL(iNUMBER_OF_COLORS, first_item), _thousands_colors_menu_item, active);

	/* Force the stuff for the valkyrie board.. */
	if(preferences->screen_mode.acceleration==_valkyrie_acceleration)
	{
		preferences->screen_mode.high_resolution= FALSE;
		preferences->screen_mode.bit_depth= 16;
		preferences->screen_mode.draw_every_other_line= FALSE;
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iNUMBER_OF_COLORS, first_item), 
			CONTROL_INACTIVE, _thousands_colors_menu_item);
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iDETAIL, first_item), 
			CONTROL_INACTIVE, NONE);

		/* You can't choose a monitor with hardware acceleration enabled.. */
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iCHOOSE_MONITOR, first_item), 
			CONTROL_INACTIVE, NONE);
	} else {
		/* Make sure it is enabled. */
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iNUMBER_OF_COLORS, first_item), 
			CONTROL_ACTIVE, NONE);
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iDETAIL, first_item), 
			CONTROL_ACTIVE, NONE);
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iCHOOSE_MONITOR, first_item), 
			CONTROL_ACTIVE, NONE);
	}
	
	switch(preferences->screen_mode.bit_depth)
	{
		case 32: value= _millions_colors_menu_item; break;
		case 16: value= _thousands_colors_menu_item; break;
		case 8:	 value= _hundreds_colors_menu_item; break;
		default: value= _hundreds_colors_menu_item; halt(); break;
	}
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iNUMBER_OF_COLORS, first_item), NONE, value);

#ifdef env68k
	if(preferences->screen_mode.bit_depth != 8 || preferences->screen_mode.high_resolution)
	{
		preferences->screen_mode.draw_every_other_line= FALSE;
		active= CONTROL_INACTIVE;
	} else {
		active= CONTROL_ACTIVE;
	}
#else
	preferences->screen_mode.draw_every_other_line= FALSE;
	active= CONTROL_INACTIVE;
#endif
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iDRAW_EVERY_OTHER_LINE, first_item), 
		active, preferences->screen_mode.draw_every_other_line);

	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iWINDOW_SIZE, first_item), NONE, 
		preferences->screen_mode.size+1);
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iDETAIL, first_item), NONE, 
		!preferences->screen_mode.high_resolution+1); 
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iBRIGHTNESS, first_item), NONE, 
		preferences->screen_mode.gamma_level+1);

#ifdef envppc
	if (machine_has_display_manager())
	{
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iRESOLUTION_SWITCHING, first_item), CONTROL_ACTIVE, preferences->do_resolution_switching);
	}
	else
	{
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iRESOLUTION_SWITCHING, first_item), CONTROL_INACTIVE, 0);
	}
#else
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iRESOLUTION_SWITCHING, first_item), CONTROL_INACTIVE, 0);
#endif

	active = (hardware_acceleration_code(&preferences->device_spec) == _valkyrie_acceleration) ? CONTROL_ACTIVE : CONTROL_INACTIVE;
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iHARDWARE_ACCELERATION, first_item), 
		active, (preferences->screen_mode.acceleration == _valkyrie_acceleration));

	return;
}

static void hit_graphics_item(
	DialogPtr dialog,
	short first_item,
	void *prefs,
	short item_hit)
{
	struct graphics_preferences_data *preferences= (struct graphics_preferences_data *) prefs;
	ControlHandle control;
	short item_type;
	Rect bounds;
	boolean resetup= TRUE;

	switch(GLOBAL_TO_LOCAL_DITL(item_hit, first_item))
	{
		case iCHOOSE_MONITOR:
			display_device_dialog(&preferences->device_spec);
			/* We resetup because the new device might not support millions, etc.. */
			break;
			
		case iDRAW_EVERY_OTHER_LINE:
			preferences->screen_mode.draw_every_other_line= !preferences->screen_mode.draw_every_other_line;
			break;
			
		case iHARDWARE_ACCELERATION:
			preferences->screen_mode.draw_every_other_line= !preferences->screen_mode.draw_every_other_line;
			if(preferences->screen_mode.acceleration == _valkyrie_acceleration)
			{
				preferences->screen_mode.acceleration= _no_acceleration;
			} else {
				preferences->screen_mode.acceleration= _valkyrie_acceleration;
			}
			break;

#ifdef envppc			
		case iRESOLUTION_SWITCHING:
			preferences->do_resolution_switching= !preferences->do_resolution_switching;
			break;
#endif

		case iNUMBER_OF_COLORS:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			switch(GetCtlValue(control))
			{
				case _hundreds_colors_menu_item: preferences->screen_mode.bit_depth= 8; break;
				case _thousands_colors_menu_item: preferences->screen_mode.bit_depth= 16; break;
				case _millions_colors_menu_item: preferences->screen_mode.bit_depth= 32; break;
				default: preferences->screen_mode.bit_depth= 8; halt(); break;
			}
			break;

		case iWINDOW_SIZE:		
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->screen_mode.size= GetCtlValue(control)-1;
			break;
			
		case iDETAIL:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->screen_mode.high_resolution= !(GetCtlValue(control)-1);
			break;
		
		case iBRIGHTNESS:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->screen_mode.gamma_level= GetCtlValue(control)-1;
			break;
			
		default:
			halt();
			break;
	}

	if(resetup)
	{
		setup_graphics_dialog(dialog, first_item, prefs);
	}
}
	
static boolean teardown_graphics_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	#pragma unused (dialog, first_item, prefs)
	return TRUE;
}

/* --------- player */
static void *get_player_pref_data(
	void)
{
	return w_get_data_from_preferences(prefPLAYER_TAG,
		sizeof(struct player_preferences_data), default_player_preferences,
		validate_player_preferences);
}

static void setup_player_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	struct player_preferences_data *preferences= (struct player_preferences_data *)prefs;
	Handle item;
	short item_type;
	Rect bounds;

	/* Setup the difficulty level */
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iDIFFICULTY_LEVEL, first_item), NONE, 
		preferences->difficulty_level+1);

	/* Setup the name. */
	GetDItem(dialog, LOCAL_TO_GLOBAL_DITL(iNAME, first_item), &item_type, &item, &bounds);
	SetDialogItemText(item, (StringPtr)preferences->name);
	SelectDialogItemText(dialog, LOCAL_TO_GLOBAL_DITL(iNAME, first_item), 0, SHORT_MAX);

	/* Setup the color */
	GetDItem(dialog, LOCAL_TO_GLOBAL_DITL(iCOLOR, first_item), &item_type, &item, &bounds);
	SetCtlValue((ControlHandle) item, preferences->color+1);

	/* Setup the team */
	GetDItem(dialog, LOCAL_TO_GLOBAL_DITL(iTEAM, first_item), &item_type, &item, &bounds);
	SetCtlValue((ControlHandle) item, preferences->team+1);
}

static void hit_player_item(
	DialogPtr dialog,
	short first_item,
	void *prefs,
	short item_hit)
{
	ControlHandle control;
	short item_type;
	Rect bounds;
	struct player_preferences_data *preferences= (struct player_preferences_data *)prefs;

	switch(GLOBAL_TO_LOCAL_DITL(item_hit, first_item))
	{
		case iDIFFICULTY_LEVEL:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->difficulty_level= GetCtlValue(control)-1;
			break;

		case iCOLOR:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->color= GetCtlValue(control)-1;
			break;

		case iTEAM:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->team= GetCtlValue(control)-1;
			break;
	}
}
	
static boolean teardown_player_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	Handle control;
	short item_type;
	Rect bounds;
	struct player_preferences_data *preferences= (struct player_preferences_data *)prefs;
	Str255 buffer;
	
	/* Get the player name */
	GetDItem(dialog, LOCAL_TO_GLOBAL_DITL(iNAME, first_item), &item_type, &control, &bounds);
	GetIText(control, buffer);
	if(buffer[0]>PREFERENCES_NAME_LENGTH) buffer[0]= PREFERENCES_NAME_LENGTH;
	memcpy(preferences->name, buffer, buffer[0]+1);

	return TRUE;
}

/* --------- sound */
static void *get_sound_pref_data(
	void)
{
	return w_get_data_from_preferences(prefSOUND_TAG,
		sizeof(struct sound_manager_parameters), default_sound_manager_parameters,
		NULL);
}

static void setup_sound_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	struct sound_manager_parameters *preferences= (struct sound_manager_parameters *) prefs;
	short active;
	word available_flags;

	available_flags= available_sound_manager_flags(preferences->flags);

	/* First setup the popups */
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iVOLUME, first_item), NONE, 
		preferences->volume+1);
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iCHANNELS, first_item), NONE, 
		preferences->channel_count);

	active= (available_flags & _stereo_flag) ? CONTROL_ACTIVE : CONTROL_INACTIVE;
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iSTEREO, first_item), active, 
		(preferences->flags & _stereo_flag) ? TRUE : FALSE);

	/* Don't do dynamic tracking if you aren't in stereo. */
	if(!(preferences->flags & _stereo_flag))
	{
		preferences->flags &= ~_dynamic_tracking_flag;
	}

	active= (available_flags & _dynamic_tracking_flag) ? CONTROL_ACTIVE : CONTROL_INACTIVE;
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iACTIVE_PANNING, first_item), active, 
		(preferences->flags & _dynamic_tracking_flag) ? TRUE : FALSE);

	active= (available_flags & _16bit_sound_flag) ? CONTROL_ACTIVE : CONTROL_INACTIVE;
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iHIGH_QUALITY, first_item), active, 
		(preferences->flags & _16bit_sound_flag) ? TRUE : FALSE);

	active= (available_flags & _ambient_sound_flag) ? CONTROL_ACTIVE : CONTROL_INACTIVE;
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iAMBIENT_SOUND, first_item), active, 
		(preferences->flags & _ambient_sound_flag) ? TRUE : FALSE);

	active= (available_flags & _more_sounds_flag) ? CONTROL_ACTIVE : CONTROL_INACTIVE;
	modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iMORE_SOUNDS, first_item), active, 
		(preferences->flags & _more_sounds_flag) ? TRUE : FALSE);

	return;
}

static void hit_sound_item(
	DialogPtr dialog,
	short first_item,
	void *prefs,
	short item_hit)
{
	ControlHandle control;
	short item_type;
	Rect bounds;
	struct sound_manager_parameters *preferences= (struct sound_manager_parameters *)prefs;

	switch(GLOBAL_TO_LOCAL_DITL(item_hit, first_item))
	{
		case iSTEREO:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			if(!GetCtlValue(control))
			{
				preferences->flags |= _stereo_flag;
			} else {
				preferences->flags &= ~_stereo_flag;
			}
			break;

		case iACTIVE_PANNING:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			if(!GetCtlValue(control))
			{
				preferences->flags |= _dynamic_tracking_flag;
			} else {
				preferences->flags &= ~_dynamic_tracking_flag;
			}
			break;
			
		case iHIGH_QUALITY:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			if(!GetCtlValue(control))
			{
				preferences->flags |= _16bit_sound_flag;
			} else {
				preferences->flags &= ~_16bit_sound_flag;
			}
			break;

		case iAMBIENT_SOUND:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			if(!GetCtlValue(control))
			{
				preferences->flags |= _ambient_sound_flag;
			} else {
				preferences->flags &= ~_ambient_sound_flag;
			}
			break;

		case iMORE_SOUNDS:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			if(!GetCtlValue(control))
			{
				preferences->flags |= _more_sounds_flag;
			} else {
				preferences->flags &= ~_more_sounds_flag;
			}
			break;
			
		case iVOLUME:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->volume= GetCtlValue(control)-1;
			test_sound_volume(preferences->volume, _snd_adjust_volume);
			break;

		case iCHANNELS:
			GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
			preferences->channel_count= GetCtlValue(control);	
			break;
			
		default:
			halt();
			break;
	}

	setup_sound_dialog(dialog, first_item, prefs);

	return;
}
	
static boolean teardown_sound_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
#pragma unused (dialog, first_item, prefs)
	return TRUE;
}

/* --------- input */

static void *get_input_pref_data(
	void)
{
	return w_get_data_from_preferences(prefINPUT_TAG,
		sizeof(struct input_preferences_data), default_input_preferences,
		validate_input_preferences);
}

static void setup_input_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	struct input_preferences_data *preferences= (struct input_preferences_data *)prefs;
	short which;
	
#ifdef SUPPORT_INPUT_SPROCKET
	switch(preferences->input_device)
	{
		case _mouse_yaw_pitch:
			which = iMOUSE_CONTROL;
			break;
		case _input_sprocket_yaw_pitch:
			if (system_information->has_input_sprocket)	
			{
				which = iINPUT_SPROCKET_CONTROL;
				break;
			}
			/* FALL-THRU */
		case _keyboard_or_game_pad:
		default:
			which = iKEYBOARD_CONTROL;
	}
	
	
	if (system_information->has_input_sprocket)
	{
		modify_radio_button_family(dialog, LOCAL_TO_GLOBAL_DITL(iMOUSE_CONTROL, first_item), 
			LOCAL_TO_GLOBAL_DITL(iINPUT_SPROCKET_CONTROL, first_item), 
			LOCAL_TO_GLOBAL_DITL(which, first_item));
	}
	else
	{
		modify_radio_button_family(dialog, LOCAL_TO_GLOBAL_DITL(iMOUSE_CONTROL, first_item), 
			LOCAL_TO_GLOBAL_DITL(iKEYBOARD_CONTROL, first_item), 
			LOCAL_TO_GLOBAL_DITL(which, first_item));
			
		// disable the input sprocket radio button	
		modify_control(dialog, LOCAL_TO_GLOBAL_DITL(iINPUT_SPROCKET_CONTROL, first_item),
				CONTROL_INACTIVE, NONE);
	}
#else	
	which = (preferences->input_device == _mouse_yaw_pitch) ? iMOUSE_CONTROL : iKEYBOARD_CONTROL;
	modify_radio_button_family(dialog, LOCAL_TO_GLOBAL_DITL(iMOUSE_CONTROL, first_item), 
		LOCAL_TO_GLOBAL_DITL(iKEYBOARD_CONTROL, first_item), 
		LOCAL_TO_GLOBAL_DITL(which, first_item));
#endif
}

static void hit_input_item(
	DialogPtr dialog,
	short first_item,
	void *prefs,
	short item_hit)
{
	struct input_preferences_data *preferences= (struct input_preferences_data *)prefs;

	switch(GLOBAL_TO_LOCAL_DITL(item_hit, first_item))
	{
		case iMOUSE_CONTROL:
		case iKEYBOARD_CONTROL:
#ifdef SUPPORT_INPUT_SPROCKET
			use_input_sprocket= FALSE;
			modify_radio_button_family(dialog, LOCAL_TO_GLOBAL_DITL(iMOUSE_CONTROL, first_item), 
				LOCAL_TO_GLOBAL_DITL(iINPUT_SPROCKET_CONTROL, first_item), item_hit);
#else
			modify_radio_button_family(dialog, LOCAL_TO_GLOBAL_DITL(iMOUSE_CONTROL, first_item), 
				LOCAL_TO_GLOBAL_DITL(iKEYBOARD_CONTROL, first_item), item_hit);
#endif
			preferences->input_device= GLOBAL_TO_LOCAL_DITL(item_hit, first_item)==iMOUSE_CONTROL ?
				_mouse_yaw_pitch : _keyboard_or_game_pad;
			break;
		
#ifdef SUPPORT_INPUT_SPROCKET
		case iINPUT_SPROCKET_CONTROL:
			modify_radio_button_family(dialog, LOCAL_TO_GLOBAL_DITL(iMOUSE_CONTROL, first_item), 
				LOCAL_TO_GLOBAL_DITL(iINPUT_SPROCKET_CONTROL, first_item), item_hit);
			if (system_information->has_input_sprocket)	
			{
				use_input_sprocket= TRUE;
				preferences->input_device = _input_sprocket_yaw_pitch;
			}
			break;
#endif			
		case iSET_KEYS:
#ifdef SUPPORT_INPUT_SPROCKET
			if (use_input_sprocket)
			{
#ifndef INPUT_SPROCKET_BUG_FIX
				short	cur_res_file= CurResFile();
#endif
				GrafPtr old_port;	
				OSStatus err;
				GDHandle main_device = GetMainDevice();
				PixMapHandle main_device_pixMapHandle = (*main_device)->gdPMap;
				short main_device_depth = (*main_device_pixMapHandle)->pixelSize;
					
				GetPort(&old_port);
				
				/* if we are in 8 bit switch to system colors */
				if (main_device_depth == 8)
				{
					HideWindow(dialog);	
					paint_window_black();
					force_system_colors();
				}
				
#ifndef INPUT_SPROCKET_BUG_FIX
				UseResFile(HomeResFile(GetResource('SOCK', 128)));
#endif
				err= ISpConfigure(NULL);
#ifndef INPUT_SPROCKET_BUG_FIX
				UseResFile(cur_res_file);
#endif

				/* if we are in 8 bit switch back to our colors */
				if (main_device_depth == 8)
				{
					display_screen(MAIN_MENU_BASE);
					
					ShowWindow(dialog);
					SelectWindow(dialog);
					SetPort(dialog);
					DrawDialog(dialog);
					ShowCursor();
					
					stop_fade();
				}
				
				SetPort(old_port);
			}
			else
#endif
			{
				short key_codes[NUMBER_OF_KEYS];
				
				memcpy(key_codes, preferences->keycodes, NUMBER_OF_KEYS*sizeof(short));
				if(configure_key_setup(key_codes))
				{
					memcpy(preferences->keycodes, key_codes, NUMBER_OF_KEYS*sizeof(short));
					set_keys(key_codes);
				}
			}
			break;
			
		default:
			break;
	}
}
	
static boolean teardown_input_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	#pragma unused(dialog, first_item, prefs)
	return TRUE;
}

/* ------------------ environment preferences */
#define MAXIMUM_FIND_FILES (32)

struct file_description {
	OSType file_type;
	unsigned long checksum;
	unsigned long parent_checksum;
};

static FSSpec *accessory_files= NULL;
static struct file_description *file_descriptions= NULL;
static short accessory_file_count= 0;
static boolean physics_valid= TRUE;

static int scenario_file_count= 0;
static int physics_file_count= 0;
static int shapes_file_count= 0;
static int sounds_file_count= 0;
static int patch_file_count= 0;

static void default_environment_preferences(
	struct environment_preferences_data *prefs)
{
	memset(prefs, NONE, sizeof(struct environment_preferences_data));

	get_default_map_spec((FileDesc *) &prefs->map_file);
	get_default_physics_spec((FileDesc *) &prefs->physics_file);
	get_file_spec(&prefs->shapes_file, strFILENAMES, filenameSHAPES8, strPATHS);
	get_file_spec(&prefs->sounds_file, strFILENAMES, filenameSOUNDS8, strPATHS);

	/* Calculate their checksums.. */
	prefs->map_checksum= read_wad_file_checksum((FileDesc *) &prefs->map_file);
	prefs->physics_checksum= read_wad_file_checksum((FileDesc *) &prefs->physics_file);
	
	/* Calculate the modification dates. */
	prefs->shapes_mod_date= get_file_modification_date(&prefs->shapes_file);
	prefs->sounds_mod_date= get_file_modification_date(&prefs->sounds_file);

	return;
}

static boolean file_exists(
	FSSpec *file)
{
	OSErr err;
	FSSpec spec;
	
	err= FSMakeFSSpec(file->vRefNum, file->parID, file->name, &spec);

	return (err==noErr);
}

static boolean validate_environment_preferences(
	struct environment_preferences_data *prefs)
{
	boolean changed= FALSE;
#if 0
	if(!file_exists(&prefs->map_file))
	{
		get_default_map_spec((FileDesc *) &prefs->map_file);
		prefs->map_checksum= read_wad_file_checksum((FileDesc *) &prefs->map_file);
		changed= TRUE;
	}
	
	if(!file_exists(&prefs->physics_file))
	{
		get_default_physics_spec((FileDesc *) &prefs->physics_file);
		prefs->physics_checksum= read_wad_file_checksum((FileDesc *) &prefs->physics_file);
		changed= TRUE;
	}
	
	if(!file_exists(&prefs->shapes_file))
	{
		get_file_spec(&prefs->shapes_file, strFILENAMES, filenameSHAPES8, strPATHS);
		prefs->shapes_mod_date= get_file_modification_date(&prefs->shapes_file);
		changed= TRUE;
	}
	
	if(!file_exists(&prefs->sounds_file))
	{
		get_file_spec(&prefs->sounds_file, strFILENAMES, filenameSOUNDS8, strPATHS);
		prefs->sounds_mod_date= get_file_modification_date(&prefs->sounds_file);
		changed= TRUE;
	}
#endif	
	return changed;
}

static void *get_environment_pref_data(
	void)
{
	return w_get_data_from_preferences(prefENVIRONMENT_TAG,
		sizeof(struct environment_preferences_data), 
		default_environment_preferences, validate_environment_preferences);
}

static void setup_environment_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
	struct environment_preferences_data *preferences= (struct environment_preferences_data *)prefs;

	if(allocate_extensions_memory())
	{
		SetCursor(*GetCursor(watchCursor));

		build_extensions_list();

		/* Fill in the extensions.. */
		fill_in_popup_with_filetype(dialog, LOCAL_TO_GLOBAL_DITL(iMAP, first_item),
			SCENARIO_FILE_TYPE, preferences->map_checksum);
		fill_in_popup_with_filetype(dialog, LOCAL_TO_GLOBAL_DITL(iPHYSICS, first_item),
			PHYSICS_FILE_TYPE, preferences->physics_checksum);
		fill_in_popup_with_filetype(dialog, LOCAL_TO_GLOBAL_DITL(iSHAPES, first_item),
			SHAPES_FILE_TYPE, preferences->shapes_mod_date);
		fill_in_popup_with_filetype(dialog, LOCAL_TO_GLOBAL_DITL(iSOUNDS, first_item),
			SOUNDS_FILE_TYPE, preferences->sounds_mod_date);

		SetCursor(&qd.arrow);
	} else {
		halt();
	}
	
	return;
}

static void hit_environment_item(
	DialogPtr dialog,
	short first_item,
	void *prefs,
	short item_hit)
{
	struct environment_preferences_data *preferences= (struct environment_preferences_data *)prefs;

#pragma unused(dialog)
	switch(GLOBAL_TO_LOCAL_DITL(item_hit, first_item))
	{
		case iMAP:
			preferences->map_checksum= find_checksum_and_file_spec_from_dialog(dialog, item_hit, 
				SCENARIO_FILE_TYPE,	&preferences->map_file);
			break;
			
		case iPHYSICS:
			preferences->physics_checksum= find_checksum_and_file_spec_from_dialog(dialog, item_hit, 
				PHYSICS_FILE_TYPE,	&preferences->physics_file);
			break;
			
		case iSHAPES:
			preferences->shapes_mod_date= find_checksum_and_file_spec_from_dialog(dialog, item_hit,
				SHAPES_FILE_TYPE, &preferences->shapes_file);
			break;

		case iSOUNDS:
			preferences->sounds_mod_date= find_checksum_and_file_spec_from_dialog(dialog, item_hit,
				SOUNDS_FILE_TYPE, &preferences->sounds_file);
			break;
			
		case iPATCHES_LIST:
			break;
			
		default:
			break;
	}

	return;
}

/* Load the environment.. */
void load_environment_from_preferences(
	void)
{
	FileDesc file;
	OSErr error;
	struct environment_preferences_data *prefs= environment_preferences;

	error= FSMakeFSSpec(prefs->map_file.vRefNum, prefs->map_file.parID, prefs->map_file.name,
		(FSSpec *) &file);
	if(!error)
	{
		set_map_file(&file);
	} else {
		/* Try to find the checksum */
		if(find_wad_file_that_has_checksum(&file,
			SCENARIO_FILE_TYPE, strPATHS, prefs->map_checksum))
		{
			set_map_file(&file);
		} else {
			set_to_default_map();
		}
	}

	error= FSMakeFSSpec(prefs->physics_file.vRefNum, prefs->physics_file.parID, prefs->physics_file.name,
		(FSSpec *) &file);
	if(!error)
	{
		set_physics_file(&file);
		import_definition_structures();
	} else {
		if(find_wad_file_that_has_checksum(&file,
			PHYSICS_FILE_TYPE, strPATHS, prefs->physics_checksum))
		{
			set_physics_file(&file);
			import_definition_structures();
		} else {
			/* Didn't find it.  Don't change them.. */
		}
	}

	error= FSMakeFSSpec(prefs->shapes_file.vRefNum, prefs->shapes_file.parID, prefs->shapes_file.name,
		(FSSpec *) &file);
	if(!error)
	{
		open_shapes_file((FSSpec *) &file);
	} else {
		if(find_file_with_modification_date(&file,
			SHAPES_FILE_TYPE, strPATHS, prefs->shapes_mod_date))
		{
			open_shapes_file((FSSpec *) &file);
		} else {
			/* What should I do? */
		}
	}

	error= FSMakeFSSpec(prefs->sounds_file.vRefNum, prefs->sounds_file.parID, prefs->sounds_file.name,
		(FSSpec *) &file);
	if(!error)
	{
		open_sound_file((FSSpec *) &file);
	} else {
		if(find_file_with_modification_date(&file,
			SOUNDS_FILE_TYPE, strPATHS, prefs->sounds_mod_date))
		{
			open_sound_file((FSSpec *) &file);
		} else {
			/* What should I do? */
		}
	}
	
	return;
}

static boolean teardown_environment_dialog(
	DialogPtr dialog,
	short first_item,
	void *prefs)
{
#pragma unused (dialog, first_item)
	struct environment_preferences_data *preferences= (struct environment_preferences_data *)prefs;

	/* Proceses the entire physics file.. */
	free_extensions_memory();
	
	return TRUE;
}

static unsigned long get_file_modification_date(
	FSSpec *file)
{
	CInfoPBRec pb;
	OSErr error;
	unsigned long modification_date= 0l;
	
	memset(&pb, 0, sizeof(pb));
	pb.hFileInfo.ioVRefNum= file->vRefNum;
	pb.hFileInfo.ioNamePtr= file->name;
	pb.hFileInfo.ioDirID= file->parID;
	pb.hFileInfo.ioFDirIndex= 0;
	error= PBGetCatInfoSync(&pb);
	if(!error)
	{
		modification_date= pb.hFileInfo.ioFlMdDat;
	}

	return modification_date;
}

/* ---------------- miscellaneous */
static void set_popup_enabled_state(
	DialogPtr dialog,
	short item_number,
	short item_to_affect,
	boolean enabled)
{
	MenuHandle menu;
	struct PopupPrivateData **privateHndl;
	ControlHandle control;
	short item_type;
	Rect bounds;
	
	/* Get the menu handle */
	GetDItem(dialog, item_number, &item_type, (Handle *) &control, &bounds);
	assert(item_type&ctrlItem);

	/* I don't know how to assert that it is a popup control... <sigh> */
	privateHndl= (PopupPrivateData **) ((*control)->contrlData);
	assert(privateHndl);
	
	menu= (*privateHndl)->mHandle;
	assert(menu);
	
	if(enabled)
	{
		EnableItem(menu, item_to_affect);
	} else {
		DisableItem(menu, item_to_affect);
	}
}

static boolean allocate_extensions_memory(
	void)
{
	boolean success;

	assert(!accessory_files);
	assert(!file_descriptions);
/*
	accessory_file_count= 0;
	accessory_files= (FSSpec *) malloc(MAXIMUM_FIND_FILES*sizeof(FSSpec));
	file_descriptions= (struct file_description *) malloc(MAXIMUM_FIND_FILES*sizeof(struct file_description));
	if(file_descriptions && accessory_files)
	{
		success= TRUE;
	} else {
		if(file_descriptions) free(file_descriptions);
		if(accessory_files) free(accessory_files);
		accessory_files= NULL;
		file_descriptions= NULL;
		success= FALSE;
	}
*/
	// dynamic to allow for up to MAXIMUM_FIND_FILES per popup.
	scenario_file_count= 0;
	physics_file_count= 0;
	shapes_file_count= 0;
	sounds_file_count= 0;
	patch_file_count= 0;

	accessory_file_count= 0;
	accessory_files= NULL;
	file_descriptions= NULL;
	success= TRUE;
	
	return success;
}

static void free_extensions_memory(
	void)
{
//	assert(accessory_files);
//	assert(file_descriptions);

	if (file_descriptions) free(file_descriptions);
	if (accessory_files) free(accessory_files);
	accessory_files= NULL;
	file_descriptions= NULL;
	accessory_file_count= 0;

	scenario_file_count= 0;
	physics_file_count= 0;
	shapes_file_count= 0;
	sounds_file_count= 0;
	patch_file_count= 0;
	return;
}

static boolean increase_extensions_memory(
	void)
{
	FSSpec *new_accessory_files= NULL;
	struct file_description *new_file_descriptions= NULL;
	boolean success= FALSE;
	
	new_accessory_files= (FSSpec *)realloc(accessory_files,sizeof(FSSpec) * (accessory_file_count + 1));
	if (new_accessory_files!=NULL)
	{
		accessory_files= new_accessory_files;
		new_file_descriptions= (struct file_description *)realloc(file_descriptions,sizeof(struct file_description) * (accessory_file_count + 1));
		if (new_file_descriptions!=NULL)
		{
			file_descriptions= new_file_descriptions;

			success= TRUE;
		}
	}
	
	return success;
}

static Boolean file_is_extension_and_add_callback(
	FSSpec *file,
	void *data)
{
	unsigned long checksum;
	CInfoPBRec *pb= (CInfoPBRec *) data;
	
//	assert(accessory_files);
//	assert(file_descriptions);

//	if(accessory_file_count<MAXIMUM_FIND_FILES)
	{
		switch(pb->hFileInfo.ioFlFndrInfo.fdType)
		{
			case SCENARIO_FILE_TYPE:
			case PHYSICS_FILE_TYPE:
				checksum= read_wad_file_checksum((FileDesc *) file);
				if(checksum != NONE) /* error. */
				{
					if (((pb->hFileInfo.ioFlFndrInfo.fdType==PHYSICS_FILE_TYPE) ? physics_file_count : scenario_file_count)<MAXIMUM_FIND_FILES)
					{
						if (increase_extensions_memory())
						{
							if (pb->hFileInfo.ioFlFndrInfo.fdType==PHYSICS_FILE_TYPE)
							{
								physics_file_count++;
							}
							else
							{
								scenario_file_count++;
							}
							accessory_files[accessory_file_count]= *file;
							file_descriptions[accessory_file_count].file_type= pb->hFileInfo.ioFlFndrInfo.fdType;
							file_descriptions[accessory_file_count++].checksum= checksum;
						}
					}
				}
				break;

			case PATCH_FILE_TYPE:
				checksum= read_wad_file_checksum((FileDesc *) file);
				if(checksum != NONE) /* error. */
				{
					unsigned long parent_checksum;
					
					if ((patch_file_count<MAXIMUM_FIND_FILES) && increase_extensions_memory())
					{
						patch_file_count++;
						parent_checksum= read_wad_file_parent_checksum((FileDesc *) file);
						accessory_files[accessory_file_count]= *file;
						file_descriptions[accessory_file_count].file_type= pb->hFileInfo.ioFlFndrInfo.fdType;
						file_descriptions[accessory_file_count++].checksum= checksum;
						file_descriptions[accessory_file_count++].parent_checksum= parent_checksum;
					}
				}
				break;
				
			case SHAPES_FILE_TYPE:
			case SOUNDS_FILE_TYPE:
				if (((pb->hFileInfo.ioFlFndrInfo.fdType==SHAPES_FILE_TYPE) ? shapes_file_count : sounds_file_count)<MAXIMUM_FIND_FILES)
				{
					if (increase_extensions_memory())
					{
						if (pb->hFileInfo.ioFlFndrInfo.fdType==SHAPES_FILE_TYPE)
						{
							shapes_file_count++;
						}
						else
						{
							sounds_file_count++;
						}
						accessory_files[accessory_file_count]= *file;
						file_descriptions[accessory_file_count].file_type= pb->hFileInfo.ioFlFndrInfo.fdType;
						file_descriptions[accessory_file_count++].checksum= pb->hFileInfo.ioFlMdDat;
					}
				}
				break;
				
			default:
				break;
		}
	}
	
	return FALSE;
}

static void build_extensions_list(
	void)
{
	FSSpec my_spec;
	short path_count, ii;

	get_my_fsspec(&my_spec);
	search_from_directory(&my_spec);

	/* Add the paths.. */
	path_count= countstr(strPATHS);
	for(ii= 0; ii<path_count; ++ii)
	{
		OSErr err;
		FSSpec file;
	
		getpstr(temporary, strPATHS, ii);
		
		/* Hmm... check FSMakeFSSpec... */
		/* Relative pathname.. */
		err= FSMakeFSSpec(my_spec.vRefNum, my_spec.parID, (StringPtr)temporary, &file);
		
		if(!err) 
		{
			long parID;
			
			err= get_directories_parID(&file, &parID);
			if(!err)
			{
				file.parID= parID;
				search_from_directory(&file);
			} else {
dprintf("Error: %d", err);
			}
		}
	}

	return;
}

static void search_from_directory(
	FSSpec *file)
{
	struct find_file_pb pb;
	OSErr error;
	
	memset(&pb, 0, sizeof(struct find_file_pb));
	pb.version= 0;
#ifdef FINAL
	pb.flags= _ff_recurse | _ff_callback_with_catinfo;
#else
	pb.flags= _ff_callback_with_catinfo;
#endif
	pb.search_type= _callback_only;
	pb.vRefNum= file->vRefNum;
	pb.directory_id= file->parID;
	pb.type_to_find= WILDCARD_TYPE;
	pb.buffer= NULL;
	pb.max= MAXIMUM_FIND_FILES;
	pb.callback= file_is_extension_and_add_callback;
	pb.user_data= NULL;
	pb.count= 0;

	error= find_files(&pb);
	vassert(!error, csprintf(temporary, "Err: %d", error));

	return;
}

/* Note that we are going to assume that things don't change while they are in this */
/*  dialog- ie no one is copying files to their machine, etc. */
static void fill_in_popup_with_filetype(
	DialogPtr dialog, 
	short item,
	OSType type,
	unsigned long checksum)
{
	MenuHandle menu;
	short index, value= NONE;
	ControlHandle control;
	short item_type, count;
	Rect bounds;

	/* Get the menu */
	menu= get_popup_menu_handle(dialog, item);
	
	/* Remove whatever it had */
	while(CountMItems(menu)) DelMenuItem(menu, 1);

//	assert(file_descriptions);
	for(index= 0; index<accessory_file_count; ++index)
	{
		if(file_descriptions[index].file_type==type)
		{
			AppendMenu(menu, "\p ");
			SetMenuItemText(menu, CountMItems(menu), accessory_files[index].name);

			if(file_descriptions[index].checksum==checksum)
			{
				value= CountMItems(menu);
			}
		}
	} 

	/* Set the max value */
	GetDItem(dialog, item, &item_type, (Handle *) &control, &bounds);
	count= CountMItems(menu);

	if(count==0)
	{
		switch(type)
		{
			case PHYSICS_FILE_TYPE:
				set_to_default_physics_file();
				AppendMenu(menu, (StringPtr)getpstr(temporary, strPROMPTS, _default_prompt));
				value= 1;
				physics_valid= FALSE;
				count++;
				break;
				
			case SCENARIO_FILE_TYPE:
			case SHAPES_FILE_TYPE:
			case SOUNDS_FILE_TYPE:
				break;
				
			default:
				break;
		}
	} 
	
	SetCtlMax(control, count);
	
	if(value != NONE)
	{
		SetCtlValue(control, value);
	} else {
		/* Select the default one, somehow.. */
		SetCtlValue(control, 1);
	}

	return;
}

static unsigned long find_checksum_and_file_spec_from_dialog(
	DialogPtr dialog, 
	short item_hit, 
	OSType type,
	FSSpec *file)
{
	short index;
	ControlHandle control;
	short item_type, value;
	Rect bounds;
	unsigned long checksum;
	
	/* Get the dialog item hit */
	GetDItem(dialog, item_hit, &item_type, (Handle *) &control, &bounds);
	value= GetCtlValue(control);
	
	for(index= 0; index<accessory_file_count; ++index)
	{
		if(file_descriptions[index].file_type==type)
		{
			if(!--value)
			{
				/* This is it */
				*file= accessory_files[index];
				checksum= file_descriptions[index].checksum;
			}
		}
	}

	return checksum;
}

















#ifndef VULCAN
static MenuHandle get_popup_menu_handle(
	DialogPtr dialog,
	short item)
{
	struct PopupPrivateData **privateHndl;
	MenuHandle menu;
	short item_type;
	ControlHandle control;
	Rect bounds;

	/* Add the maps.. */
	GetDItem(dialog, item, &item_type, (Handle *) &control, &bounds);

	/* I don't know how to assert that it is a popup control... <sigh> */
	privateHndl= (PopupPrivateData **) ((*control)->contrlData);
	assert(privateHndl);

	menu= (*privateHndl)->mHandle;
	assert(menu);

	return menu;
}
#endif

#if 0
static boolean control_strip_installed(
	void)
{
	boolean installed= FALSE;
	long control_strip_version;
	OSErr error;
	
	error= Gestalt(gestaltControlStripVersion, &control_strip_version);
	if(!error)
	{
		installed= TRUE;
	}
	
	return installed;
}

static void hide_control_strip(
	void)
{
	assert(control_strip_installed());
	if(SBIsControlStripVisible())
	{
		SBShowHideControlStrip(FALSE);
	}
}

static void show_control_strip(
	void)
{
	assert(control_strip_installed());
	if(!SBIsControlStripVisible())
	{
		SBShowHideControlStrip(TRUE);
	}
}
#endif
/*
 * Monday, September 12, 1994 12:45:17 PM  (alain)
 *   This header file can only be included by one other file. right now that's vbl.c
 *
 */


/* Constants */
enum /* special flag types */
{
	_double_flag,
	_latched_flag
};

/* Structures */
struct blacklist_data
{
	short offset1, offset2; /* the combination of keys that should be blacklisted */
	short mask1, mask2;     /* help for finding them in the keymap */
};

struct special_flag_data
{
	short type;
	long flag, alternate_flag;
	short persistence;
};

struct key_definition
{
	short offset;
	long action_flag;
	short mask;
};

/*
 * various key setups that the user can get.
 * NOTE that these arrays must all be in the same order, and they must
 * be in the same order as the text edit boxes in the "setup keys" dialog
 *
 */

#define NUMBER_OF_STANDARD_KEY_DEFINITIONS (sizeof(standard_key_definitions)/sizeof(struct key_definition))
static struct key_definition standard_key_definitions[]=
{
	/* keypad */
	{0x5b, _moving_forward},
	{0x57, _moving_backward},
	{0x56, _turning_left},
	{0x58, _turning_right},
	
	/* zx translation */
	{0x06, _sidestepping_left},
	{0x07, _sidestepping_right},

	/* as looking */
	{0x00, _looking_left},
	{0x01, _looking_right},

	/* dcv vertical looking */
	{0x02, _looking_up},
	{0x08, _looking_down},
	{0x09, _looking_center},
	
	/* KP7/KP9 for weapon cycling */
	{0x59, _cycle_weapons_backward},
	{0x5c, _cycle_weapons_forward},
	
	/* space for primary trigger, option for alternate trigger */
	{0x31, _left_trigger_state},
	{0x3a, _right_trigger_state},
	
	/* shift, control and command modifiers */
	{0x37, _sidestep_dont_turn},
	{0x3b, _run_dont_walk},
	{0x38, _look_dont_turn},
	
	/* tab for action */
	{0x30, _action_trigger_state},

	/* m for toggle between normal and overhead map view */
	{0x2e, _toggle_map},
	
	/* ` for using the microphone */
	{0x32, _microphone_button}
};

#define NUMBER_OF_LEFT_HANDED_KEY_DEFINITIONS (sizeof(left_handed_key_definitions)/sizeof(struct key_definition))
static struct key_definition left_handed_key_definitions[]=
{
	/* arrows */
	{0x7e, _moving_forward},
	{0x7d, _moving_backward},
	{0x7b, _turning_left},
	{0x7c, _turning_right},
	
	/* zx translation */
	{0x06, _sidestepping_left},
	{0x07, _sidestepping_right},

	/* as looking */
	{0x00, _looking_left},
	{0x01, _looking_right},

	/* dcv vertical looking */
	{0x02, _looking_up},
	{0x08, _looking_down},
	{0x09, _looking_center},
	
	/* ;' for weapon cycling */
	{0x0c, _cycle_weapons_backward},
	{0x0d, _cycle_weapons_forward},
	
	/* space for primary trigger, option for alternate trigger */
	{0x31, _left_trigger_state},
	{0x3a, _right_trigger_state},
	
	/* shift, control and command modifiers */
	{0x37, _sidestep_dont_turn},
	{0x3b, _run_dont_walk},
	{0x38, _look_dont_turn},
	
	/* tab for action */
	{0x30, _action_trigger_state},

	/* m for toggle between normal and overhead map view */
	{0x2e, _toggle_map},
	
	/* ` for using the microphone */
	{0x32, _microphone_button}
};

#define NUMBER_OF_POWERBOOK_KEY_DEFINITIONS (sizeof(powerbook_key_definitions)/sizeof(struct key_definition))
static struct key_definition powerbook_key_definitions[]=
{
	/* olk; */
	{0x1f, _moving_forward},
	{0x25, _moving_backward},
	{0x28, _turning_left},
	{0x29, _turning_right},
	
	/* zx translation */
	{0x06, _sidestepping_left},
	{0x07, _sidestepping_right},

	/* as looking */
	{0x00, _looking_left},
	{0x01, _looking_right},

	/* dcv vertical looking */
	{0x02, _looking_up},
	{0x08, _looking_down},
	{0x09, _looking_center},
	
	/* ip for weapon cycling */
	{0x22, _cycle_weapons_backward},
	{0x23, _cycle_weapons_forward},
	
	/* space for primary trigger, option for alternate trigger */
	{0x31, _left_trigger_state},
	{0x3a, _right_trigger_state},
	
	/* shift, control and command modifiers */
	{0x37, _sidestep_dont_turn},
	{0x3b, _run_dont_walk},
	{0x38, _look_dont_turn},
	
	/* tab for action */
	{0x30, _action_trigger_state},

	/* m for toggle between normal and overhead map view */
	{0x2e, _toggle_map},
	
	/* ` for using the microphone */
	{0x32, _microphone_button}
};

static struct key_definition *all_key_definitions[NUMBER_OF_KEY_SETUPS]=
{
	standard_key_definitions,
	left_handed_key_definitions,
	powerbook_key_definitions
};

/* Externed because both vbl.c and vbl_macintosh.c use this. */
extern struct key_definition current_key_definitions[NUMBER_OF_STANDARD_KEY_DEFINITIONS];

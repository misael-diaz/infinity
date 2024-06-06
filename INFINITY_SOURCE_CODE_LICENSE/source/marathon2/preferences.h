/*

	preferences.h
	Tuesday, June 13, 1995 10:07:04 AM- rdm created.

*/

/* New preferences junk */
struct graphics_preferences_data
{
	struct screen_mode_data screen_mode;
	boolean do_resolution_switching;
	byte unused[3];
	GDSpec device_spec;
};

struct serial_number_data
{
	boolean network_only;
	byte long_serial_number[10];
	Str255 user_name;
	Str255 tokenized_serial_number;
};

struct network_preferences_data
{
	boolean allow_microphone;
	boolean  game_is_untimed;
	short type; // look in network_dialogs.c for _ethernet, etc...
	short game_type;
	short difficulty_level;
	short game_options; // Penalize suicide, etc... see map.h for constants
	long time_limit;
	short kill_limit;
	short entry_point;
};

struct player_preferences_data
{
	char name[PREFERENCES_NAME_LENGTH+1];
	short color;
	short team;
	unsigned long last_time_ran;
	short difficulty_level;
	boolean background_music_on;
};

struct input_preferences_data
{
	short input_device;
	short keycodes[NUMBER_OF_KEYS];
};

#define MAXIMUM_PATCHES_PER_ENVIRONMENT (32)

struct environment_preferences_data
{
	FSSpec map_file;
	FSSpec physics_file;
	FSSpec shapes_file;
	FSSpec sounds_file;
	unsigned long map_checksum;
	unsigned long physics_checksum;
	unsigned long shapes_mod_date;
	unsigned long sounds_mod_date;
	unsigned long patches[MAXIMUM_PATCHES_PER_ENVIRONMENT];
};

/* New preferences.. (this sorta defeats the purpose of this system, but not really) */
extern struct graphics_preferences_data *graphics_preferences;
extern struct serial_number_data *serial_preferences;
extern struct network_preferences_data *network_preferences;
extern struct player_preferences_data *player_preferences;
extern struct input_preferences_data *input_preferences;
extern struct sound_manager_parameters *sound_preferences;
extern struct environment_preferences_data *environment_preferences;

/* --------- functions */
void initialize_preferences(void);
void handle_preferences(void);
void write_preferences(void);
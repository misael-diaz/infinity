#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "util.h"

#define TICKS_PER_SECOND 30
#define INPUT_NUM_KEYS 21
#define GAMMA_NUM_LEVELS 8
#define GAMMA_DEFAULT_LEVEL 2
#define DEVICE_NONE 0xffff
#define DEVICE_COLOR_FLAG 0x0001
#define MAX_WADFILE_NAME_LEN 63
#define MAX_WADFILE_NAME_SIZE (MAX_WADFILE_NAME_LEN + 1)
#define MAX_PLAYER_NAME_LEN MAX_WADFILE_NAME_LEN
#define MAX_PLAYER_NAME_SIZE (MAX_WADFILE_NAME_LEN + 1)
#define MAX_PRECALCULATION_TABLE_ENTRY_SIZE 64
#define PREFERENCES_NAME_LEN 255
#define PREFERENCES_NAME_SIZE (PREFERENCES_NAME_LEN + 1)
#define FD_NAME_LEN MAX_WADFILE_NAME_LEN
#define FD_NAME_SIZE (FD_NAME_LEN + 1)
#define LEVEL_NAME_LEN MAX_WADFILE_NAME_LEN
#define LEVEL_NAME_SIZE (LEVEL_NAME_LEN + 1)
#define RENDER_FLAGS_BUFFER_SIZE (8 * 1024)
#define MAX_NUM_ENTRIES_SCRATCH_TABLE 1024
#define POLYGON_QUEUE_SIZE 256
#define MAX_NUM_OBJECT_TYPES 64
#define MAX_NUM_ENEMIES_PER_MAP 256
#define MAX_NUM_PROJECTILES_PER_MAP 32
#define MAX_NUM_PLATFORMS_PER_MAP 64
#define MAX_NUM_OBJECTS_PER_MAP 512
#define MAX_NUM_LINES_PER_MAP (4 * 1024)
#define MAX_NUM_ENDPOINTS_PER_MAP (8 * 1024)
#define MAX_NUM_POINTS_PER_PATH 63
#define MAX_NUM_NODES 512
#define MAX_NUM_FLOOD_NODES 255
#define MAX_NUM_SORTED_NODES 128
#define MAX_NUM_RENDER_OBJECTS 64
#define MAX_NUM_ENDPOINT_CLIPS 64
#define MAX_NUM_CLIPPING_WINDOWS 256
#define MAX_NUM_LINE_CLIPS 256
#define MAX_NUM_VERTICES_PER_POLYGON 8
#define MAX_NUM_POLYGONS_PER_MAP 1024
#define MAX_NUM_CLIPPING_LINES_PER_NODE (MAX_NUM_VERTICES_PER_POLYGON - 2)
#define MAX_NUM_PLAYERS 8
#define MAX_NUM_ITEMS 64
#define MAX_NUM_PATHS 20

#define NUMBER_OF_STANDARD_KEY_DEFINITIONS (\
	sizeof(standard_key_definitions) / sizeof(struct key_definition)\
)

enum {
	_full_screen,
	_100_percent,
	_75_percent,
	_50_percent
} ScreenSize;

enum {
	_no_acceleration,
	_valkyrie_acceleration
} HardwareAcceleration;

enum {
	_keyboard_or_game_pad,
	_mouse_yaw_pitch,
	_mouse_yaw_velocity,
	_cybermaxx_input,
	_input_sprocket_yaw_pitch,
} InputDevices;

enum {
        _appletalk_remote,
        _localtalk,
        _tokentalk,
        _ethernet,
        NUMBER_OF_NETWORK_TYPES
} NetworkTypes;

enum {
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

	NUMBER_OF_ACTION_FLAG_BITS
} ActionFlagBitOffsets;

enum {
	_absolute_yaw_mode = (1 << _absolute_yaw_mode_bit),
	_turning_left = (1 << _turning_left_bit),
	_turning_right = (1 << _turning_right_bit),
	_sidestep_dont_turn = (1 << _sidestep_dont_turn_bit),
	_looking_left = (1 << _looking_left_bit),
	_looking_right = (1 << _looking_right_bit),

	_absolute_pitch_mode = (1 << _absolute_pitch_mode_bit),
	_looking_up = (1 << _looking_up_bit),
	_looking_down = (1 << _looking_down_bit),
	_looking_center = (1 << _looking_center_bit),
	_look_dont_turn = (1 << _look_dont_turn_bit),

	_absolute_position_mode = (1 << _absolute_position_mode_bit),
	_moving_forward = (1 << _moving_forward_bit),
	_moving_backward = (1 << _moving_backward_bit),
	_run_dont_walk = (1 << _run_dont_walk_bit),

	_sidestepping_left = (1 << _sidestepping_left_bit),
	_sidestepping_right = (1 << _sidestepping_right_bit),
	_left_trigger_state = (1 << _left_trigger_state_bit),
	_right_trigger_state = (1 << _right_trigger_state_bit),
	_action_trigger_state = (1 << _action_trigger_state_bit),
	_cycle_weapons_forward = (1 << _cycle_weapons_forward_bit),
	_cycle_weapons_backward = (1 << _cycle_weapons_backward_bit),
	_toggle_map = (1 << _toggle_map_bit),
	_microphone_button = (1 << _microphone_button_bit),
	_swim = (1 << _swim_bit),

	_turning = _turning_left | _turning_right,
	_looking = _looking_left | _looking_right,
	_moving = _moving_forward | _moving_backward,
	_sidestepping = _sidestepping_left | _sidestepping_right,
	_looking_vertically = _looking_up | _looking_down | _looking_center
} ActionFlags;

enum {
	_game_of_kill_monsters,
	_game_of_cooperative_play,
	_game_of_capture_the_flag,
	_game_of_king_of_the_hill,
	_game_of_kill_man_with_ball,
	_game_of_defense,
	_game_of_rugby,
	_game_of_tag,
	NUMBER_OF_GAME_TYPES
} GameTypes;

enum {
	_multiplayer_game = 0x0001,
	_ammo_replenishes = 0x0002,
	_weapons_replenish = 0x0004,
	_specials_replenish = 0x0008,
	_monsters_replenish = 0x0010,
	_motion_sensor_does_not_work = 0x00020,
	_overhead_map_is_omniscient = 0x0040,
	_burn_items_on_death = 0x0080,
	_live_network_stats = 0x0100,
	_game_has_kill_limit = 0x0200,
	_force_unique_teams = 0x0400,
	_dying_is_penalized = 0x0800,
	_suicide_is_penalized = 0x1000,
	_overhead_map_shows_items = 0x2000,
	_overhead_map_shows_monsters = 0x4000,
	_overhead_map_shows_projectiles = 0x8000
} GameOptions;

enum TAG {
	NETWORK_TAG,
	ENVIRONMENT_TAG,
	GRAPHICS_TAG,
	PLAYER_TAG,
	INPUT_TAG,
	SOUND_TAG
};

struct tag {
	uint64_t tag;
	uint64_t length;
	uint64_t offset;
	char *data;
};

struct wad {
	struct tag *tag;
	char *read_only_data;
	int16_t num_tags;
	int16_t pad;
};

struct wad_header {
	uint64_t checksum;
	uint64_t parent_checksum;
	int16_t num_wads;
	char filename[MAX_WADFILE_NAME_SIZE];
	char explicit_padding[46];
};

struct entry_header {
	uint64_t tag;
	uint64_t length;
	uint64_t offset;
	uint64_t next_offset;
};

struct data_node {
        struct data_node *parent;
        struct data_node *siblings;
        struct data_node *children;
        struct data_node **reference;
        int16_t clipping_endpoints[MAX_NUM_CLIPPING_LINES_PER_NODE];
        int16_t clipping_lines[MAX_NUM_CLIPPING_LINES_PER_NODE];
        int16_t polygon_index;
        int16_t clipping_endpoint_count;
        int16_t clipping_line_count;
        int16_t flags;
};

struct FileDescriptor {
	uint64_t id;
	FILE *handle;
	int16_t reference_number;
	char name[FD_NAME_SIZE];
};

struct preferences {
	struct FileDescriptor fd;
	struct wad *wad;
};

struct key_definition {
	int16_t offset;
	int64_t action_flag;
	int16_t: 16;
	int32_t: 32;
	int32_t: 32;
};

struct data_preferences_input {
        int16_t keycodes[INPUT_NUM_KEYS];
        int16_t input_device;
};

struct data_screen_mode {
	int16_t size;
	int16_t acceleration;
	int16_t bit_depth;
	int16_t gamma_level;
	bool high_resolution;
	bool texture_floor;
	bool texture_ceiling;
	bool draw_every_other_line;
	char explicit_padding_char[4];
};

struct GraphicsDeviceSpecification {
        int16_t slot;
        int16_t flags;
        int16_t bit_depth;
        int16_t width;
	int16_t height;
	char explicit_padding[6];
} GDSpec;

struct data_preferences_graphics {
	struct data_screen_mode screen_mode;
	struct GraphicsDeviceSpecification device_spec;
	bool do_resolution_switching;
	char explicit_padding[31];
};

struct data_preferences_network {
        int64_t time_limit;
        int16_t type;
        int16_t game_type;
        int16_t difficulty_level;
        int16_t game_options;
        int16_t kill_limit;
        int16_t entry_point;
        bool allow_microphone;
        bool game_is_untimed;
	int64_t: 64;
};

struct data_preferences_player {
	time_t last_time_exec;
	int16_t difficulty_level;
	int16_t color;
	int16_t team;
	bool background_music_enabled;
	char name[PREFERENCES_NAME_SIZE];
	char explicit_padding[240];
};

struct data_static {
        uint64_t entry_point_flags;
        int16_t environment_code;
        int16_t physics_model;
        int16_t song_index;
        int16_t mission_flags;
        int16_t environment_flags;
        int16_t explicit_padding_int16_t[3];
        char level_name[LEVEL_NAME_SIZE];
        char explicit_padding_char[40];
};

struct point2d {
	int16_t x;
	int16_t y;
};

struct point3d {
	int16_t x;
	int16_t y;
	int16_t z;
};

struct path {
	struct point2d points[MAX_NUM_POINTS_PER_PATH];
	int16_t current_step;
	int16_t step_count;
};

struct data_enemy {
	uint64_t ticks_since_last_activation;
	struct point3d sound_location;
	int16_t type;
	int16_t vitality;
	int16_t flags;
	int16_t path;
	int16_t path_segment_length;
	int16_t desired_height;
	int16_t mode;
	int16_t action;
	int16_t target_index;
	int16_t external_velocity;
	int16_t vertical_velocity;
	int16_t ticks_since_attack;
	int16_t attack_repetitions;
	int16_t changes_until_lock_lost;
	int16_t elevation;
	int16_t object_index;
	int16_t activation_bias;
	int16_t goal_polygon_index;
	int16_t sound_polygon_index;
	int16_t random_desired_height;
	char explicit_padding_char[8];
};

struct data_projectile {
	uint64_t damage_scale;
	int16_t type;
	int16_t object_index;
	int16_t target_index;
	int16_t elevation;
	int16_t owner_index;
	int16_t owner_type;
	int16_t flags;
	int16_t ticks_since_last_contrail;
	int16_t contrail_count;
	int16_t distance_travelled;
	int16_t gravity;
	int16_t permutation;
};

struct data_object {
	uint32_t sound_pitch;
	struct point3d location;
	int16_t shape;
	int16_t polygon;
	int16_t facing;
	int16_t sequence;
	int16_t flags;
	int16_t transfer_mode;
	int16_t transfer_period;
	int16_t transfer_phase;
	int16_t permutation;
	int16_t next_object;
	int16_t parasitic_object;
};

struct data_endpoint_owner {
	int16_t first_polygon_index;
	int16_t polygon_index_count;
	int16_t first_line_index;
	int16_t line_index_count;
};

struct data_platform {
	struct data_endpoint_owner endpoint_owners[MAX_NUM_VERTICES_PER_POLYGON];
	uint64_t static_flags;
	int16_t type;
	int16_t speed;
	int16_t delay;
	int16_t minimum_floor_height;
	int16_t maximum_floor_height;
	int16_t minimum_ceiling_height;
	int16_t maximum_ceiling_height;
	int16_t polygon_index;
	int16_t dynamic_flags;
	int16_t floor_height;
	int16_t ceiling_height;
	int16_t ticks_until_restart;
	int16_t parent_platform_index;
	int16_t tag;
	char explicit_padding[28];
};

struct action_queue {
        int64_t *buffer;
        int16_t read_index;
	int16_t write_index;
	char explicit_padding[4];
};

struct damage_record {
	uint64_t damage;
	int16_t kills;
};

struct vector2d {
	int16_t x;
	int16_t y;
};

struct vector3d {
	int16_t x;
	int16_t y;
	int16_t z;
};

struct data_endpoint_clip {
	struct vector2d vector;
	int16_t flags;
	int16_t x;
};

struct data_line_clip {
	struct vector2d top_vector;
	struct vector2d bottom_vector;
	int16_t top_y;
	int16_t bottom_y;
	int16_t flags;
	int16_t x0;
	int16_t x1;
};

struct data_clipping_window {
	struct data_clipping_window *next_window;
	struct vector2d left;
	struct vector2d right;
	struct vector2d top;
	struct vector2d bottom;
	int16_t x0;
	int16_t x1;
	int16_t y0;
	int16_t y1;
};

struct data_sorted_node {
        struct data_render_object *interior_objects;
        struct data_render_object *exterior_objects;
        struct data_clipping_window *clipping_windows;
        int16_t polygon_index;
};

struct rectangle_definition {
	struct bitmap_definition *texture;
	void *shading_tables;
	int16_t flags;
	int16_t x0;
	int16_t y0;
	int16_t x1;
	int16_t y1;
	int16_t clip_left;
	int16_t clip_right;
	int16_t clip_top;
	int16_t clip_bottom;
	int16_t depth;
	int16_t ambient_shade;
	int16_t transfer_mode;
	int16_t transfer_data;
	bool flip_vertical;
	bool flip_horizontal;
};

struct data_render_object {
        struct data_sorted_node *node;
        struct data_clipping_window *clipping_windows;
        struct data_render_object *next_object;
        struct rectangle_definition rectangle;
        int16_t ymedia;
};

struct physics_variables {
	struct point3d last_position;
	struct point3d position;
	struct vector3d external_velocity;
	int16_t head_direction;
	int16_t last_direction;
	int16_t direction;
	int16_t elevation;
	int16_t angular_velocity;
	int16_t vertical_angular_velocity;
	int16_t velocity;
	int16_t perpendicular_velocity;
	int16_t actual_height;
	int16_t adjusted_pitch;
	int16_t adjusted_yaw;
	int16_t external_angular_velocity;
	int16_t step_phase;
	int16_t step_amplitude;
	int16_t floor_height;
	int16_t ceiling_height;
	int16_t media_height;
	int16_t action;
	int16_t old_flags;
	int16_t flags;
};

struct data_player {
	struct damage_record damage_taken[MAX_NUM_PLAYERS];
	struct physics_variables variables;
	struct damage_record total_damage_given;
	struct damage_record monster_damage_taken;
	struct damage_record monster_damage_given;
	struct point3d location;
	struct point3d camera_location;
	int16_t items[MAX_NUM_ITEMS];
	uint64_t ticks_at_last_successful_save;
	uint64_t netgame_parameters[2];
	int16_t identifier;
	int16_t flags;
	int16_t color;
	int16_t team;
	int16_t camera_polygon_index;
	int16_t facing, elevation;
	int16_t supporting_polygon_index;
	int16_t last_supporting_polygon_index;
	int16_t suit_energy, suit_oxygen;
	int16_t monster_index;
	int16_t object_index;
	int16_t weapon_intensity_decay;
	int16_t weapon_intensity;
	int16_t invisibility_duration;
	int16_t invincibility_duration;
	int16_t infravision_duration;
	int16_t extravision_duration;
	int16_t delay_before_teleport;
	int16_t teleporting_phase;
	int16_t teleporting_destination;
	int16_t interlevel_teleport_phase;
	int16_t interface_flags;
	int16_t interface_decay;
	int16_t reincarnation_delay;
	int16_t control_panel_side_index;
	char name[MAX_PLAYER_NAME_SIZE];
	char explicit_padding[498];
};

struct data_game {
        uint64_t game_time_remaining;
        int32_t initial_random_seed;
        int16_t game_type;
        int16_t game_options;
        int16_t kill_limit;
        int16_t difficulty_level;
        int16_t parameters[2];
};

struct data_dynamic {
	struct data_game game_information;
	struct point2d game_beacon;
	uint64_t tick_count;
	uint32_t random_seed;
	int16_t player_count;
	int16_t speaking_player_index;
	int16_t explicit_padding;
	int16_t platform_count;
	int16_t endpoint_count;
	int16_t line_count;
	int16_t side_count;
	int16_t polygon_count;
	int16_t lightsource_count;
	int16_t map_index_count;
	int16_t ambient_sound_image_count;
	int16_t random_sound_image_count;
	int16_t object_count;
	int16_t enemy_count;
	int16_t projectile_count;
	int16_t effect_count;
	int16_t light_count;
	int16_t default_annotation_count;
	int16_t personal_annotation_count;
	int16_t initial_objects_count;
	int16_t garbage_object_count;
	int16_t last_enemy_index_to_get_time;
	int16_t last_enemy_index_to_build_path;
	int16_t new_enemy_mangler_cookie;
	int16_t new_enemy_vanishing_cookie;
	int16_t civilians_killed_by_players;
	int16_t random_enemys_left[MAX_NUM_OBJECT_TYPES];
	int16_t current_enemy_count[MAX_NUM_OBJECT_TYPES];
	int16_t random_items_left[MAX_NUM_OBJECT_TYPES];
	int16_t current_item_count[MAX_NUM_OBJECT_TYPES];
	int16_t current_level_number;
	int16_t current_civilian_causalties;
	int16_t current_civilian_count;
	int16_t total_civilian_causalties;
	int16_t total_civilian_count;
	int16_t game_player_index;
};

static void* precalculation_table = NULL;
static int16_t *scratch_table1 = NULL;
static int16_t *scratch_table2 = NULL;
static int16_t *render_flags = NULL;
static int16_t *line_clip_ids = NULL;
static int16_t *endpoint_coords = NULL;
static int16_t *polygon_queue = NULL;
static int16_t *visited_polygons = NULL;
static struct data_sorted_node **node_polygon_mapper = NULL;
static struct data_render_object *render_objects = NULL;
static struct data_clipping_window *clipping_windows = NULL;
static struct data_endpoint_clip *endpoint_clips = NULL;
static struct data_line_clip *line_clips = NULL;
static struct data_node *nodes = NULL;
static struct data_node *flood_nodes = NULL;
static struct data_sorted_node *sorted_nodes = NULL;
static struct path *paths = NULL;
static struct data_static *world_static = NULL;
static struct data_dynamic *world_dynamic = NULL;
static struct data_enemy *enemies = NULL;
static struct data_projectile *projectiles = NULL;
static struct data_object *objects = NULL;
static struct data_platform *platforms = NULL;
static struct data_player *players = NULL;
static struct preferences *preferences = NULL;
static struct data_preferences_network *preferences_network = NULL;
static struct data_preferences_graphics *preferences_graphics = NULL;
static struct data_preferences_player *preferences_player = NULL;
static struct data_preferences_input *preferences_input = NULL;

static struct key_definition standard_key_definitions[] = {

	/* keypad */
	{.offset = 0x5b, .action_flag = _moving_forward},
	{.offset = 0x57, .action_flag = _moving_backward},
	{.offset = 0x56, .action_flag = _turning_left},
	{.offset = 0x58, .action_flag = _turning_right},

	/* zx translation */
	{.offset = 0x06, .action_flag = _sidestepping_left},
	{.offset = 0x07, .action_flag = _sidestepping_right},

	/* as looking */
	{.offset = 0x00, .action_flag = _looking_left},
	{.offset = 0x01, .action_flag = _looking_right},

	/* dcv vertical looking */
	{.offset = 0x02, .action_flag = _looking_up},
	{.offset = 0x08, .action_flag = _looking_down},
	{.offset = 0x09, .action_flag = _looking_center},

	/* KP7/KP9 for weapon cycling */
	{.offset = 0x59, .action_flag = _cycle_weapons_backward},
	{.offset = 0x5c, .action_flag = _cycle_weapons_forward},

	/* space for primary trigger, option for alternate trigger */
	{.offset = 0x31, .action_flag = _left_trigger_state},
	{.offset = 0x3a, .action_flag = _right_trigger_state},

	/* shift, control and command modifiers */
	{.offset = 0x37, .action_flag = _sidestep_dont_turn},
	{.offset = 0x3b, .action_flag = _run_dont_walk},
	{.offset = 0x38, .action_flag = _look_dont_turn},

	/* tab for action */
	{.offset = 0x30, .action_flag = _action_trigger_state},

	/* m for toggle between normal and overhead map view */
	{.offset = 0x2e, .action_flag = _toggle_map},

	/* ` for using the microphone */
	{.offset = 0x32, .action_flag = _microphone_button}
};

void *wad_extractTypeFromWad(uint64_t *length,
			     struct wad const *wad,
			     uint64_t wadDataType);
void *wad_getDataFromPreferences(uint64_t preferences,
				 uint64_t expected_size,
				 void (*initialize) (void *prefs),
				 bool (*validate) (void *prefs));

void wad_fillDefaultWadHeader(struct FileDescriptor *fd,
			      int16_t num_wads,
			      struct wad_header *header);

void wad_writeWadHeader(struct FileDescriptor *fd, struct wad_header *header);
bool wad_writeWad(struct FileDescriptor *fd,
		  struct wad_header *file_header,
		  struct wad *wad,
		  uint64_t offset);
void wad_createEmptyWad(struct wad *wad);
void allocate_memory_map(void);
void allocate_memory_render(void);
void allocate_memory_path(void);
void allocate_memory_flood_map(void);
void allocate_texture_table(void);
void allocate_memory_preferences(void);
void initialize_preferences(void);

#define WAD_FILENAME "wadfile.dat"

int main (void)
{
	printf("number of stdkey defs: %zu\n", NUMBER_OF_STANDARD_KEY_DEFINITIONS);
	printf("sizeof(struct path): %zu\n", sizeof(struct path));
	printf("sizeof(struct key_definition): %zu\n", sizeof(struct key_definition));
	printf("sizeof(struct action_queue): %zu\n", sizeof(struct action_queue));
	printf("sizeof(struct data_player): %zu\n", sizeof(struct data_player));
	printf("sizeof(struct data_enemy): %zu\n", sizeof(struct data_enemy));
	printf("sizeof(struct data_projectile): %zu\n", sizeof(struct data_projectile));
	printf("sizeof(struct data_platform): %zu\n", sizeof(struct data_platform));
	printf("sizeof(struct data_object): %zu\n", sizeof(struct data_object));
	printf("sizeof(struct data_static): %zu\n", sizeof(struct data_static));
	printf("sizeof(struct wad_header): %zu\n", sizeof(struct wad_header));
	printf("sizeof(struct data_screen_mode): %zu\n", sizeof(struct data_screen_mode));
	printf("sizeof(struct GraphicsDeviceSpecification): %zu\n",
	sizeof(struct GraphicsDeviceSpecification));
	printf("sizeof(struct data_preferences_graphics): %zu\n",
	sizeof(struct data_preferences_graphics));
	printf("sizeof(struct data_preferences_player): %zu\n",
	sizeof(struct data_preferences_player));
	printf("sizeof(struct data_preferences_network): %zu\n",
	sizeof(struct data_preferences_network));

	if (INPUT_NUM_KEYS != NUMBER_OF_STANDARD_KEY_DEFINITIONS) {
		fprintf(stderr, "main: NumInputKeysError");
		exit(EXIT_FAILURE);
	}

	char wadfile[FD_NAME_SIZE] = WAD_FILENAME;
	FILE *file = fopen(wadfile, "w");
	if (!file) {
		printf("main: IO ERROR\n");
		Util_Clear();
		exit(EXIT_FAILURE);
	}

	struct FileDescriptor fd = {
		.id = 0,
		.handle = file,
		.reference_number = 0,
		.name = WAD_FILENAME
	};

	struct wad_header header;
	struct wad wad;
	int16_t num_wads = 1;
	uint64_t offset = 0;
	wad_createEmptyWad(&wad);
	wad_fillDefaultWadHeader(&fd, num_wads, &header);
	wad_writeWadHeader(&fd, &header);
	wad_writeWad(&fd, &header, &wad, offset);
	fclose(file);
	allocate_memory_map();
	allocate_memory_render();
	allocate_memory_path();
	allocate_memory_flood_map();
	allocate_texture_table();
	allocate_memory_preferences();
	initialize_preferences();
	Util_Clear();
	return 0;
}

void *wad_extractTypeFromWad (uint64_t *length,
			      struct wad const *wad,
			      uint64_t wadDataType)
{
	*length = ((uint64_t) 0);
	void *data = NULL;
	for (int16_t i = 0; i != wad->num_tags; ++i) {
		if (wad->tag[i].tag == wadDataType) {
			data = wad->tag[i].data;
			*length = wad->tag[i].length;
			break;
		}
	}
	return data;
}

/*
void *wad_getDataFromPreferences (uint64_t tag,
				  uint64_t expected_size,
				  void (*initialize) (void *prefs),
				  bool (*validate) (void *prefs))
{

	return NULL;
}
*/

void wad_fillDefaultWadHeader (struct FileDescriptor *fd,
			       int16_t num_wads,
			       struct wad_header *header)
{
	memset(header, 0, sizeof(*header));
	header->num_wads = num_wads;
	strcpy(header->filename, fd->name);
}

bool wad_writeWad (struct FileDescriptor *fd,
		   struct wad_header *file_header,
		   struct wad *wad,
		   uint64_t offset)
{
	size_t bytes = 0;
	uint64_t running_offset = 0;
	struct entry_header header;
	size_t sz_h = fwrite(file_header, 1, sizeof(*file_header), fd->handle);
	if (sz_h != sizeof(*file_header)) {
		return false;
	}
	for (int16_t i = 0; i != wad->num_tags; ++i) {
		header.tag = wad->tag[i].tag;
		header.length = wad->tag[i].length;
		header.offset = wad->tag[i].offset;
		if (i == wad->num_tags - 1) {
			header.next_offset = 0;
		} else {
			running_offset += header.length + sizeof(struct entry_header);
			header.next_offset = running_offset;
		}

		size_t sz_h = fwrite(&header, 1, sizeof(header), fd->handle);
		if (sz_h != sizeof(header)) {
			return false;
		}
		size_t sz_d = fwrite(wad->tag[i].data, 1, wad->tag[i].length, fd->handle);
		if (sz_d != wad->tag[i].length) {
			return false;
		}
		size_t sz_header = sz_h;
		size_t sz_data = sz_d;
		bytes += (sz_header + sz_data);
	}

	if (bytes != running_offset) {
		return false;
	} else {
		return true;
	}
}

void wad_writeWadHeader (struct FileDescriptor *fd, struct wad_header *header)
{

	size_t bytes_written = fwrite(header, 1, sizeof(*header), fd->handle);
	if (bytes_written != sizeof(*header)) {
		printf("wad_writeWadHeader: unexpected IO Error!");
		fclose(fd->handle);
		Util_Clear();
		exit(EXIT_FAILURE);
	}
	printf("wad_writeWadHeader: bytes-written: %zu\n", bytes_written);
}

void wad_createEmptyWad (struct wad *wad)
{
	memset(wad, 0, sizeof(*wad));
}

void allocate_memory_map (void)
{
	world_static = (struct data_static*) Util_Malloc(sizeof(struct data_static));
	world_dynamic = (struct data_dynamic*) Util_Malloc(sizeof(struct data_dynamic));
	size_t sz_enemies = MAX_NUM_ENEMIES_PER_MAP * sizeof(struct data_enemy);
	enemies = (struct data_enemy*) Util_Malloc(sz_enemies);
	size_t szof_projectile = sizeof(struct data_projectile);
	size_t sz_projectiles = MAX_NUM_PROJECTILES_PER_MAP * szof_projectile;
	projectiles = (struct data_projectile*) Util_Malloc(sz_projectiles);
	size_t sz_objects = MAX_NUM_OBJECTS_PER_MAP * sizeof(struct data_object);
	objects = (struct data_object*) Util_Malloc(sz_objects);
	size_t sz_platforms = MAX_NUM_PLATFORMS_PER_MAP * sizeof(struct data_platform);
	platforms = (struct data_platform*) Util_Malloc(sz_platforms);
	size_t sz_players = MAX_NUM_PLAYERS * sizeof(struct data_player);
	players = (struct data_player*) Util_Malloc(sz_players);
}

void allocate_memory_render (void)
{
	size_t sz_render_flags = RENDER_FLAGS_BUFFER_SIZE * sizeof(int16_t);
	render_flags = (int16_t*) Util_Malloc(sz_render_flags);

	size_t sz_line_clip_ids = MAX_NUM_LINES_PER_MAP * sizeof(int16_t);
	line_clip_ids = (int16_t*) Util_Malloc(sz_line_clip_ids);

	size_t sz_polygon_queue = POLYGON_QUEUE_SIZE * sizeof(int16_t);
	polygon_queue = (int16_t*) Util_Malloc(sz_polygon_queue);

	size_t sz_endpoint_coords = MAX_NUM_ENDPOINTS_PER_MAP * sizeof(int16_t);
	endpoint_coords = (int16_t*) Util_Malloc(sz_endpoint_coords);

	size_t sz_nodes = MAX_NUM_NODES * sizeof(struct data_node);
	nodes = (struct data_node*) Util_Malloc(sz_nodes);
	size_t sz_sorted_nodes = MAX_NUM_SORTED_NODES * sizeof(struct data_sorted_node);
	sorted_nodes = (struct data_sorted_node*) Util_Malloc(sz_sorted_nodes);

	size_t szof_render_objects = sizeof(struct data_render_object);
	size_t sz_render_objects = MAX_NUM_RENDER_OBJECTS * szof_render_objects;
	render_objects = (struct data_render_object*) Util_Malloc(sz_render_objects);

	size_t szof_endpoint_clip = sizeof(struct data_endpoint_clip);
	size_t sz_endpoint_clips = MAX_NUM_ENDPOINT_CLIPS * szof_endpoint_clip;
	endpoint_clips = (struct data_endpoint_clip*) Util_Malloc(sz_endpoint_clips);

	size_t szof_line_clip = sizeof(struct data_line_clip);
	size_t sz_line_clips = MAX_NUM_LINE_CLIPS * szof_line_clip;
	line_clips = (struct data_line_clip*) Util_Malloc(sz_line_clips);

	size_t szof_clipping_window = sizeof(struct data_clipping_window);
	size_t sz_clip_windows = MAX_NUM_CLIPPING_WINDOWS * szof_clipping_window;
	clipping_windows = (struct data_clipping_window*) Util_Malloc(sz_clip_windows);

	size_t sz_np_mapper = MAX_NUM_POLYGONS_PER_MAP * sizeof(struct data_sorted_node*);
	node_polygon_mapper = (struct data_sorted_node**) Util_Malloc(sz_np_mapper);
}

void allocate_memory_path (void)
{
	size_t sz_paths = MAX_NUM_PATHS * sizeof(struct path);
	paths = (struct path*) Util_Malloc(sz_paths);
}

void allocate_memory_flood_map (void)
{
	size_t sz_flood_nodes = MAX_NUM_FLOOD_NODES * sizeof(struct data_node);
	flood_nodes = (struct data_node*) Util_Malloc(sz_flood_nodes);
	size_t sz_visited_polygons = MAX_NUM_POLYGONS_PER_MAP * sizeof(int16_t);
	visited_polygons = (int16_t*) Util_Malloc(sz_visited_polygons);
}

void allocate_texture_table (void)
{
	size_t sz_scratch_table = MAX_NUM_ENTRIES_SCRATCH_TABLE * sizeof(int16_t);
	scratch_table2 = (int16_t*) Util_Malloc(sz_scratch_table);
	scratch_table1 = (int16_t*) Util_Malloc(sz_scratch_table);
	size_t sz = (MAX_PRECALCULATION_TABLE_ENTRY_SIZE * MAX_NUM_ENTRIES_SCRATCH_TABLE);
	precalculation_table = Util_Malloc(sz);

}

void allocate_memory_preferences (void)
{
	size_t sz = sizeof(struct data_preferences_graphics);
	preferences_graphics = (struct data_preferences_graphics*) Util_Malloc(sz);
	size_t sz_player = sizeof(struct data_preferences_player);
	preferences_player = (struct data_preferences_player*) Util_Malloc(sz_player);
	size_t sz_input = sizeof(struct data_preferences_input);
	preferences_input = (struct data_preferences_input*) Util_Malloc(sz_input);
	size_t sz_network = sizeof(struct data_preferences_network);
	preferences_network = (struct data_preferences_network*) Util_Malloc(sz_network);
}

void default_preferences_graphics (struct data_preferences_graphics *preferences_graphics)
{
	preferences_graphics->device_spec.slot = DEVICE_NONE;
	preferences_graphics->device_spec.flags = DEVICE_COLOR_FLAG;
	preferences_graphics->device_spec.bit_depth = 8;
	preferences_graphics->device_spec.width = 640;
	preferences_graphics->device_spec.height = 480;

	preferences_graphics->screen_mode.gamma_level = GAMMA_DEFAULT_LEVEL;
	preferences_graphics->screen_mode.size = _100_percent;
	preferences_graphics->screen_mode.high_resolution = true;
	preferences_graphics->screen_mode.acceleration = _no_acceleration;
	preferences_graphics->screen_mode.bit_depth = 8;
	preferences_graphics->do_resolution_switching = false;
	preferences_graphics->screen_mode.draw_every_other_line = false;
}

void default_preferences_player (struct data_preferences_player *preferences_player)
{
	memset(preferences_player, 0, sizeof(*preferences_player));
	preferences_player->last_time_exec = time(NULL);
	preferences_player->difficulty_level = 2;
	strncpy(preferences_player->name, "infinity", PREFERENCES_NAME_SIZE);
	preferences_player->name[PREFERENCES_NAME_LEN] = '\0';
}

void input_set_default_keys (struct data_preferences_input *preferences_input)
{
	int16_t *keycodes = preferences_input->keycodes;
	for (int16_t i = 0; i != NUMBER_OF_STANDARD_KEY_DEFINITIONS; ++i) {
		keycodes[i] = standard_key_definitions[i].offset;
	}
}

void default_preferences_input (struct data_preferences_input *preferences_input)
{
	preferences_input->input_device = _keyboard_or_game_pad;
	input_set_default_keys(preferences_input);
}

void default_preferences_network (struct data_preferences_network *preferences)
{
	preferences->type = _ethernet;
	preferences->allow_microphone = true;
	preferences->game_is_untimed = false;
	preferences->difficulty_level = 2;
	preferences->game_options = _multiplayer_game     |
				    _ammo_replenishes     |
				    _weapons_replenish    |
				    _specials_replenish   |
				    _monsters_replenish   |
				    _burn_items_on_death  |
				    _suicide_is_penalized |
				    _force_unique_teams   |
				    _live_network_stats;
	preferences->time_limit = 10 * TICKS_PER_SECOND * 60;
	preferences->kill_limit = 10;
	preferences->entry_point = 0;
	preferences->game_type = _game_of_kill_monsters;
}

void initialize_preferences (void)
{
	default_preferences_network(preferences_network);
	default_preferences_graphics(preferences_graphics);
	default_preferences_player(preferences_player);
	default_preferences_input(preferences_input);
}


/*

Infinity                                             June 07, 2024

author: @misael-diaz
source: src/main/main.c

Copyright (C) 2024 Misael Díaz-Maldonado

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

*/

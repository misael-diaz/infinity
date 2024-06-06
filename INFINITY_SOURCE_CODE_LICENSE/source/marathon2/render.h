/*
RENDER.H
Thursday, September 8, 1994 5:40:59 PM  (Jason)
*/

#include "world.h"	
#include "textures.h"

/* ---------- constants */

/* the distance behind which we are not required to draw objects */
#define MINIMUM_OBJECT_DISTANCE ((short)(WORLD_ONE/20))

#define MINIMUM_VERTICES_PER_SCREEN_POLYGON ((short)3)
#define MAXIMUM_VERTICES_PER_SCREEN_POLYGON ((short)16)

enum /* render effects */
{
	_render_effect_fold_in,
	_render_effect_fold_out,
	_render_effect_going_fisheye,
	_render_effect_leaving_fisheye,
	_render_effect_explosion
};

enum /* shading tables */
{
	_shading_normal, /* to black */
	_shading_infravision /* false color */
};

#define NORMAL_FIELD_OF_VIEW 80
#define EXTRAVISION_FIELD_OF_VIEW 130

/* ---------- structures */

struct point2d
{
	short x, y;
};
typedef struct point2d point2d;

struct definition_header
{
	short tag;
	short clip_left, clip_right;
};

struct view_data
{
	short field_of_view; /* width of the view cone, in degrees (!) */
	short standard_screen_width; /* this is *not* the width of the projected image (see initialize_view_data() in RENDER.C */
	short screen_width, screen_height; /* dimensions of the projected image */
	short horizontal_scale, vertical_scale;
	
	short half_screen_width, half_screen_height;
	short world_to_screen_x, world_to_screen_y;
	short dtanpitch; /* world_to_screen*tan(pitch) */
	angle half_cone; /* often ==field_of_view/2 (when screen_width==standard_screen_width) */
	angle half_vertical_cone;

	world_vector2d untransformed_left_edge, untransformed_right_edge;
	world_vector2d left_edge, right_edge, top_edge, bottom_edge;

	short ticks_elapsed;
	long tick_count; /* for effects and transfer modes */
	short origin_polygon_index;
	angle yaw, pitch, roll;
	world_point3d origin;
	fixed maximum_depth_intensity; /* in fixed units */

	short shading_mode;

	short effect, effect_phase;
	short real_world_to_screen_x, real_world_to_screen_y;
	
	boolean overhead_map_active;
	short overhead_map_scale;

	boolean under_media_boundary;
	short under_media_index;
	
	boolean terminal_mode_active;
};

/* ---------- render flags */

#define TEST_STATE_FLAG(i,f) TEST_RENDER_FLAG(i,f)
#define SET_STATE_FLAG(i,f,v) SET_RENDER_FLAG(i,f)

#define TEST_RENDER_FLAG(index, flag) (render_flags[index]&(flag))
#define SET_RENDER_FLAG(index, flag) render_flags[index]|= (flag)
#define RENDER_FLAGS_BUFFER_SIZE (8*KILO)
enum /* render flags */
{
	_polygon_is_visible_bit, /* some part of this polygon is horizontally in the view cone */
	_endpoint_has_been_visited_bit, /* we�ve already tried to cast a ray out at this endpoint */
	_endpoint_is_visible_bit, /* this endpoint is horizontally in the view cone */
	_side_is_visible_bit, /* this side was crossed while building the tree and should be drawn */
	_line_has_clip_data_bit, /* this line has a valid clip entry */
	_endpoint_has_clip_data_bit, /* this endpoint has a valid clip entry */
	_endpoint_has_been_transformed_bit, /* this endpoint has been transformed into screen-space */
	NUMBER_OF_RENDER_FLAGS, /* should be <=16 */

	_polygon_is_visible= 1<<_polygon_is_visible_bit,
	_endpoint_has_been_visited= 1<<_endpoint_has_been_visited_bit,
	_endpoint_is_visible= 1<<_endpoint_is_visible_bit,
	_side_is_visible= 1<<_side_is_visible_bit,
	_line_has_clip_data= 1<<_line_has_clip_data_bit,
	_endpoint_has_clip_data= 1<<_endpoint_has_clip_data_bit,
	_endpoint_has_been_transformed= 1<<_endpoint_has_been_transformed_bit
};

/* ---------- globals */

extern word *render_flags;

/* ---------- prototypes/RENDER.C */

void allocate_render_memory(void);

void initialize_view_data(struct view_data *view);
void render_view(struct view_data *view, struct bitmap_definition *destination);

void start_render_effect(struct view_data *view, short effect);

/* ----------- prototypes/SCREEN.C */
void render_overhead_map(struct view_data *view);
void render_computer_interface(struct view_data *view);

#include "scottish_textures.h"

extern boolean has_ambiguous_flags;
extern boolean exceeded_max_node_aliases;
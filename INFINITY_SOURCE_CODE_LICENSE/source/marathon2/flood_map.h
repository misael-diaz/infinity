/*
FLOOD_MAP.H
Saturday, June 18, 1994 8:07:35 PM
*/

/* ---------- constants */

enum /* flood modes */
{
	_depth_first, /* unsupported */
	_breadth_first, /* significantly faster than _best_first for large domains */
	_flagged_breadth_first, /* user data is interpreted as a long * to 4 bytes of flags */
	_best_first
};

/* ---------- typedefs */

typedef long (*cost_proc_ptr)(short source_polygon_index, short line_index, short destination_polygon_index, void *caller_data);

/* ---------- prototypes/PATHFINDING.C */

void allocate_pathfinding_memory(void);
void reset_paths(void);

short new_path(world_point2d *source_point, short source_polygon_index,
	world_point2d *destination_point, short destination_polygon_index,
	world_distance minimum_separation, cost_proc_ptr cost, void *data);
boolean move_along_path(short path_index, world_point2d *p);
void delete_path(short path_index);

/* ---------- prototypes/FLOOD_MAP.C */

void allocate_flood_map_memory(void);

/* default cost_proc, NULL, is the area of the destination polygon and is significantly faster
	than supplying a user procedure */
short flood_map(short first_polygon_index, long maximum_cost, cost_proc_ptr cost_proc, short flood_mode, void *caller_data);
short reverse_flood_map(void);
short flood_depth(void);

void choose_random_flood_node(world_vector2d *bias);

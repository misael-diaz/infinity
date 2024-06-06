/*
SCREEN.H
Thursday, August 24, 1995 5:36:27 PM  (Jason)
*/

/* ---------- constants */

enum /* screen sizes */
{
	_full_screen,
	_100_percent,
	_75_percent,
	_50_percent
};

enum /* hardware acceleration codes */
{
	_no_acceleration,
	_valkyrie_acceleration
};

/* ---------- structures */

// screen_mode_data in SHELL.H for PREFERENCES.H

/* ---------- globals */

extern struct color_table *world_color_table, *visible_color_table, *interface_color_table;

#ifdef mac
extern GDHandle world_device;
extern WindowPtr screen_window;
extern GWorldPtr world_pixels;

#if SUPPORT_DRAW_SPROCKET
#include "DrawSprocket.h"
extern DSpContextAttributes		gDrawContextAttributes;
extern DSpContextReference		gDrawContext;
#endif

#endif

/* ---------- prototypes/SCREEN.C */

void change_screen_clut(struct color_table *color_table);
void change_interface_clut(struct color_table *color_table);
void animate_screen_clut(struct color_table *color_table, boolean full_screen);

void build_direct_color_table(struct color_table *color_table, short bit_depth);

void start_teleporting_effect(boolean out);
void start_extravision_effect(boolean out);

void render_screen(short ticks_elapsed);

void toggle_overhead_map_display_status(void);
void zoom_overhead_map_out(void);
void zoom_overhead_map_in(void);

void enter_screen(void);
void exit_screen(void);

void darken_world_window(void);
void validate_world_window(void);

void change_gamma_level(short gamma_level);

void assert_world_color_table(struct color_table *world_color_table, struct color_table *interface_color_table);

void start_teleporting_effect(boolean out);
void start_extravision_effect(boolean out);

#ifdef mac
void initialize_screen(struct screen_mode_data *mode);
void change_screen_mode(struct screen_mode_data *mode, boolean redraw);

void process_screen_key(EventRecord *event, short key);
void process_screen_click(EventRecord *event);

boolean machine_supports_16bit(GDSpecPtr spec);
boolean machine_supports_32bit(GDSpecPtr spec);
short hardware_acceleration_code(GDSpecPtr spec);

void activate_screen_window(WindowPtr window, EventRecord *event, boolean active);
void update_screen_window(WindowPtr window, EventRecord *event);

void calculate_destination_frame(short size, boolean high_resolution, Rect *frame);
#endif

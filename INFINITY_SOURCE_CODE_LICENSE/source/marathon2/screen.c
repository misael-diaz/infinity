/*
SCREEN.C
Saturday, August 21, 1993 11:55:26 AM

Saturday, December 18, 1993 2:17:05 PM
	removed visible_color_table and cinematic_fade_test, halfway through implementing the new
	startup CLUT munger.
Friday, August 12, 1994 6:59:29 PM
	more changes for 16-bit.
Friday, September 9, 1994 12:13:04 PM (alain)
	modified to get window from a resource. set fore and back color before using copybits.
*/

/*
//we expect Entries to leave the device color table untouched, and it doesn�t
*/

#include "macintosh_cseries.h"
#include "my32bqd.h"
#include "textures.h"
#include "valkyrie.h"

#include <math.h>

#include "world.h"
#include "map.h"
#include "render.h"
#include "shell.h"
#include "interface.h"
#include "player.h"
#include "overhead_map.h"
#include "fades.h"
#include "game_window.h"
#include "screen.h"
#include "preferences.h"
#include "computer_interface.h"

#include <GestaltEqu.h>

#include <ctype.h>

/*	CJD --*/
#if SUPPORT_DRAW_SPROCKET
#include "DrawSprocket.h"
#endif

#if 1
#include <Displays.h>
extern pascal OSErr DMSuspendConfigure(Handle displayState, unsigned long reserved1)
 THREEWORDINLINE(0x303C, 0x04E9, 0xABEB);
extern pascal OSErr DMResumeConfigure(Handle displayState, unsigned long reserved1)
 THREEWORDINLINE(0x303C, 0x04E8, 0xABEB);
#endif

#ifdef mpwc
#pragma segment screen
#endif

#ifdef DEBUG
//#define CYBERMAXX
//#define WHITE_SCREEN_BETWEEN_FRAMES
//#define DIRECT_SCREEN_TEST
#endif

#ifdef env68k
#define EXTERNAL
#endif

#ifdef envppc
#define EXTERNAL
#endif

/* ---------- constants */

#ifdef DIRECT_SCREEN_TEST
#define WORLD_H 160
#define WORLD_V 80
#else
#define WORLD_H 0
#define WORLD_V 0
#endif

#define DESIRED_SCREEN_WIDTH 640
#define DESIRED_SCREEN_HEIGHT 480

/* standard screen width is twice height */
#define MAXIMUM_WORLD_WIDTH 640
#define MAXIMUM_WORLD_HEIGHT 480

#define DEFAULT_WORLD_WIDTH 640
#define DEFAULT_WORLD_HEIGHT 320

#define windGAME_WINDOW 128
#define windBACKDROP_WINDOW 129

#ifdef envppc
#define EXTRA_MEMORY (100*KILO)
#else
#define EXTRA_MEMORY 0
#endif

#define FREE_MEMORY_FOR_16BIT (4*MEG+EXTRA_MEMORY)
#define FREE_MEMORY_FOR_32BIT (9*MEG+EXTRA_MEMORY)
#define FREE_MEMORY_FOR_FULL_SCREEN (3*MEG+EXTRA_MEMORY)

#define clutMARATHON8_CLUT_ID 5002

/* ---------- missing from QUICKDRAW.H */

#define deviceIsGrayscale 0x0000
#define deviceIsColor 0x0001

typedef ReqListRec *ReqListPtr;

/* ---------- structures */

#define _EVERY_OTHER_LINE_BIT 0x8000
#define _SIXTEEN_BIT_BIT 0x4000
#define _THIRTYTWO_BIT_BIT 0x2000

struct copy_screen_data
{
	pixel8 *source, *destination;
	short width, height, bytes_per_row;
	short source_slop, destination_slop;
	short flags;
};

/* ---------- globals */

/*	CJD -- */
#if SUPPORT_DRAW_SPROCKET
DSpContextAttributes	gDrawContextAttributes;
DSpContextReference		gDrawContext;
#endif

GDHandle world_device = nil; /* the device we�re running on */
WindowPtr screen_window; /* a window covering our entire device */
WindowPtr backdrop_window; /* a window covering all devices */
struct color_table *uncorrected_color_table; /* the pristine color environment of the game (can be 16bit) */
struct color_table *world_color_table; /* the gamma-corrected color environment of the game (can be 16bit) */
struct color_table *interface_color_table; /* always 8bit, for mixed-mode (i.e., valkyrie) fades */
struct color_table *visible_color_table; /* the color environment the player sees (can be 16bit) */

struct view_data *world_view; /* should be static */

GWorldPtr world_pixels;
struct bitmap_definition *world_pixels_structure;

static boolean enough_memory_for_16bit, enough_memory_for_32bit, enough_memory_for_full_screen;

static struct screen_mode_data screen_mode;
static boolean overhead_map_status= FALSE;

//static boolean restore_depth_on_exit; /* otherwise restore CLUT */
//static short restore_depth, restore_flags; /* for SetDepth on exit */
static GDSpec restore_spec, resolution_restore_spec;

static VDSwitchInfoRec switchInfo;	// save the real mode that the user had

#define FRAME_SAMPLE_SIZE 20
boolean displaying_fps= FALSE;
short frame_count, frame_index;
long frame_ticks[64];

static boolean screen_initialized= FALSE;

short bit_depth= NONE;
short interface_bit_depth= NONE;

#if 1
Handle	display_manager_state;
#endif

#ifdef envppc
boolean did_switch_the_resolution_on_last_enter_screen= FALSE;
boolean did_we_ever_switched_resolution= FALSE;
#endif

/* ---------- private prototypes */

static void update_screen(void);

void calculate_destination_frame(short size, boolean high_resolution, Rect *frame);
static void calculate_source_frame(short size, boolean high_resolution, Rect *frame);
static void calculate_adjusted_source_frame(struct screen_mode_data *mode, Rect *frame);

static GDHandle find_and_initialize_world_device(long area, short depth);
static void restore_world_device(void);
static boolean parse_device(GDHandle device, short depth, boolean *color, boolean *needs_to_change);

static void restore_world_device(void);

static void set_overhead_map_status(boolean status);
static void set_terminal_status(boolean status);

void quadruple_screen(struct copy_screen_data *data);

static void update_fps_display(GrafPtr port);

static void calculate_screen_options(void);

/*	CJD -- just a function to clear the attributes to a known init state	*/
#if SUPPORT_DRAW_SPROCKET
static void InitDrawSprocketAttributes(DSpContextAttributes *inAttributes);
#endif

/* ---------- code */
void initialize_screen(
	struct screen_mode_data *mode)
{
	OSErr error;
	Rect bounds;
	GrafPtr old_port;
	RgnHandle gray_region= GetGrayRgn();

	if (!screen_initialized)
	{
		/* Calculate the screen options-> 16, 32, full? */
		calculate_screen_options();
#ifdef OBSOLETE
		enough_memory_for_16bit= (FreeMem()>FREE_MEMORY_FOR_16BIT) ? TRUE : FALSE;
		enough_memory_for_32bit= (FreeMem()>FREE_MEMORY_FOR_32BIT) ? TRUE : FALSE;
		enough_memory_for_full_screen= (FreeMem()>FREE_MEMORY_FOR_FULL_SCREEN) ? TRUE : FALSE;
#endif
	}
	
	if (mode->bit_depth==32 && !enough_memory_for_32bit) mode->bit_depth= 16;
	if (mode->bit_depth==16 && !enough_memory_for_16bit) mode->bit_depth= 8;
	interface_bit_depth= bit_depth= mode->bit_depth;
	
	/*	CJD -- Set up the DSp attributes before we ask it to request a context	*/
#if SUPPORT_DRAW_SPROCKET
	InitDrawSprocketAttributes(&gDrawContextAttributes);

	gDrawContextAttributes.displayWidth			= 640;
	gDrawContextAttributes.displayHeight		= 480;
	gDrawContextAttributes.colorNeeds			= kDSpColorNeeds_Require;

	switch (mode->bit_depth)
	{
		case 32:
			gDrawContextAttributes.backBufferDepthMask	= kDSpDepthMask_32;
			gDrawContextAttributes.displayDepthMask		= kDSpDepthMask_32;
			gDrawContextAttributes.backBufferBestDepth	= 32;
			gDrawContextAttributes.displayBestDepth		= 32;
			break;

		case 16:
			gDrawContextAttributes.backBufferDepthMask	= kDSpDepthMask_16;
			gDrawContextAttributes.displayDepthMask		= kDSpDepthMask_16;
			gDrawContextAttributes.backBufferBestDepth	= 16;
			gDrawContextAttributes.displayBestDepth		= 16;
			break;
			
		default:
			gDrawContextAttributes.backBufferDepthMask	= kDSpDepthMask_8;
			gDrawContextAttributes.displayDepthMask		= kDSpDepthMask_8;
			gDrawContextAttributes.backBufferBestDepth	= 8;
			gDrawContextAttributes.displayBestDepth		= 8;
	}

	gDrawContextAttributes.pageCount			= 2;
	
#else
	switch (mode->acceleration)
	{
		case _valkyrie_acceleration:
			mode->bit_depth= bit_depth= 16;
			mode->high_resolution= FALSE;
			interface_bit_depth= 8;
			break;
	}
#endif

	/*	CJD	--	Find a reserve a drawing context	*/
#if SUPPORT_DRAW_SPROCKET
	{
	OSStatus	theError;
	
		theError = DSpFindBestContext(&gDrawContextAttributes, &gDrawContext);
		if (theError != noErr)
			alert_user(fatalError, strERRORS, badMonitor, -1);

		gDrawContextAttributes.contextOptions |= kDSpContextOption_DontSyncVBL;

		theError = DSpContext_Reserve(gDrawContext, &gDrawContextAttributes);
		if (theError)
			alert_user(fatalError, strERRORS, badMonitor, -1);
	}
	
#else
	/* beg, borrow or steal an n-bit device */
	graphics_preferences->device_spec.bit_depth= interface_bit_depth;
	world_device= BestDevice(&graphics_preferences->device_spec);
	if (!world_device&&interface_bit_depth!=8)
	{
		graphics_preferences->device_spec.bit_depth= 8;
		world_device= BestDevice(&graphics_preferences->device_spec);
		if (world_device) mode->bit_depth= bit_depth= interface_bit_depth= 8;
	}
	if (!world_device) alert_user(fatalError, strERRORS, badMonitor, -1);
#endif

#ifdef OBSOLETE
	/* beg, borrow or steal an n-bit device */
	world_device= find_and_initialize_world_device(DESIRED_SCREEN_WIDTH*DESIRED_SCREEN_HEIGHT, interface_bit_depth);
	if (!world_device&&interface_bit_depth!=8)
	{
		world_device= find_and_initialize_world_device(DESIRED_SCREEN_WIDTH*DESIRED_SCREEN_HEIGHT, 8);
		if (world_device) mode->bit_depth= bit_depth= interface_bit_depth= 8;
	}
	if (!world_device) alert_user(fatalError, strERRORS, badMonitor, -1);
#endif

	if (!screen_initialized)
	{
		graphics_preferences->device_spec.width= DESIRED_SCREEN_WIDTH;
		graphics_preferences->device_spec.height= DESIRED_SCREEN_HEIGHT;
		
		uncorrected_color_table= (struct color_table *)malloc(sizeof(struct color_table));
		world_color_table= (struct color_table *)malloc(sizeof(struct color_table));
		visible_color_table= (struct color_table *)malloc(sizeof(struct color_table));
		interface_color_table= (struct color_table *)malloc(sizeof(struct color_table));
		assert(uncorrected_color_table && world_color_table && visible_color_table && interface_color_table);
		memset(world_color_table,0,sizeof(struct color_table));
		memset(interface_color_table,0,sizeof(struct color_table));

		/*	CJD --	This is where we clear the screen to black and put up the window
					in which the game is played	*/
#if !SUPPORT_DRAW_SPROCKET
		backdrop_window= (WindowPtr) NewPtr(sizeof(CWindowRecord));
		assert(backdrop_window);
		backdrop_window= GetNewCWindow(windBACKDROP_WINDOW, (Ptr) backdrop_window, (WindowPtr) -1);
		assert(backdrop_window);
		MoveWindow(backdrop_window, (**gray_region).rgnBBox.left, (**gray_region).rgnBBox.top, FALSE);
		SizeWindow(backdrop_window, RECTANGLE_WIDTH(&(**gray_region).rgnBBox), RECTANGLE_HEIGHT(&(**gray_region).rgnBBox), TRUE);
		ShowWindow(backdrop_window);

		screen_window= (WindowPtr) NewPtr(sizeof(CWindowRecord));
		assert(screen_window);
		screen_window= GetNewCWindow(windGAME_WINDOW, (Ptr) screen_window, (WindowPtr) -1);
		assert(screen_window);
		SetWRefCon(screen_window, refSCREEN_WINDOW);
		ShowWindow(screen_window);
#else
		{
		OSStatus	theError;
			DisplayIDType theID;
			
			DSpContext_FadeGammaOut(NULL, NULL);
			theError = DSpContext_SetState(gDrawContext, kDSpContextState_Active);
			DSpContext_FadeGammaIn(NULL, NULL);
			
			if (theError)
			{
				DebugStr("\pCould not set context state to active");
			}

			/* CAF --	get the gdevice associated with the context */
			theError = DSpContext_GetBackBuffer(gDrawContext, kDSpBufferKind_Normal, (CGrafPtr *) &world_pixels);
			if( theError )
				DebugStr("\p sh-t happened ");

			theError = DSpContext_GetDisplayID( gDrawContext, &theID );
			if( theError )
				DebugStr("\p sh-t happened again ");
				
			DMGetGDeviceByDisplayID( theID, &world_device, false );
			screen_window = (WindowPtr)world_pixels;
		}
#endif
		/* allocate the bitmap_definition structure for our GWorld (it is reinitialized every frame */
		world_pixels_structure= (struct bitmap_definition *) NewPtr(sizeof(struct bitmap_definition)+sizeof(pixel8 *)*MAXIMUM_WORLD_HEIGHT);
		assert(world_pixels_structure);
		
		/* allocate and initialize our view_data structure; we don�t call initialize_view_data
			anymore (change_screen_mode does that) */
		world_view= (struct view_data *) NewPtr(sizeof(struct view_data));
		assert(world_view);
		world_view->field_of_view= NORMAL_FIELD_OF_VIEW; /* degrees (was 74 for a long, long time) */
		world_view->overhead_map_scale= DEFAULT_OVERHEAD_MAP_SCALE;
		world_view->overhead_map_active= FALSE;
		world_view->terminal_mode_active= FALSE;
		world_view->horizontal_scale= 1, world_view->vertical_scale= 1;

		/* make sure everything gets cleaned up after we leave */
		atexit(restore_world_device);
	
#if !SUPPORT_DRAW_SPROCKET
		world_pixels= (GWorldPtr) NULL;
#endif
	}
	else
	{
		unload_all_collections();
	}
	
	if (!screen_initialized || MatchGDSpec(&restore_spec)!=MatchGDSpec(&graphics_preferences->device_spec))
	{
		if (screen_initialized)
		{
	
			/* get rid of the menu bar */
			HideMenuBar(GetMainDevice());

			GetPort(&old_port);
			SetPort(screen_window);
			PaintRect(&screen_window->portRect);
			SetPort(old_port);
			
			SetDepthGDSpec(&restore_spec);
	
			/* get rid of the menu bar */
			HideMenuBar(GetMainDevice());

		}
		BuildGDSpec(&restore_spec, world_device);
#ifdef envppc
		// did we weak link in DisplayMgr?
		if ((DMDisposeList!=NULL)) //(mode->texture_floor)
		{
			BuildGDSpec(&resolution_restore_spec, world_device);
			resolution_restore_spec.bit_depth= graphics_preferences->device_spec.bit_depth;
			if (!screen_initialized)
			{
				(void) DMGetDisplayMode(world_device, &switchInfo);
				DMBeginConfigureDisplays(&display_manager_state);
				DMResumeConfigure(display_manager_state, 0);
			}
		}
#endif
	}
	
	/* CAF -- just commenting out stuff that would make us crash */
#if !SUPPORT_DRAW_SPROCKET
	SetDepthGDSpec(&graphics_preferences->device_spec);

	/* get rid of the menu bar */
	HideMenuBar(GetMainDevice());

	MoveWindow(screen_window, (*world_device)->gdRect.left, (*world_device)->gdRect.top, FALSE);
	SizeWindow(screen_window, RECTANGLE_WIDTH(&(*world_device)->gdRect), RECTANGLE_HEIGHT(&(*world_device)->gdRect), TRUE);

#endif
	{
		Point origin;
		
		origin.h= - (RECTANGLE_WIDTH(&(*world_device)->gdRect)-DESIRED_SCREEN_WIDTH)/2;
		origin.v= - (RECTANGLE_HEIGHT(&(*world_device)->gdRect)-DESIRED_SCREEN_HEIGHT)/2;
		if (origin.v>0) origin.v= 0;
		
		GetPort(&old_port);
		SetPort(screen_window);
		SetOrigin(origin.h, origin.v);
		SetPort(old_port);
	}
	
	/* allocate and initialize our GWorld */
	calculate_destination_frame(mode->size, mode->high_resolution, &bounds);

#if ! SUPPORT_DRAW_SPROCKET
	error= screen_initialized ? myUpdateGWorld(&world_pixels, 0, &bounds, (CTabHandle) NULL, (GDHandle) NULL, 0) :
		myNewGWorld(&world_pixels, 0, &bounds, (CTabHandle) NULL, (GDHandle) NULL, 0);
	if (error!=noErr) alert_user(fatalError, strERRORS, outOfMemory, error);
#endif

	change_screen_mode(mode, FALSE);

	screen_initialized= TRUE;
	
	return;
}

void enter_screen(
	void)
{
	GrafPtr old_port;
	
	if (world_view->overhead_map_active) set_overhead_map_status(FALSE);
	if (world_view->terminal_mode_active) set_terminal_status(FALSE);

#if defined(envppc) && ! SUPPORT_DRAW_SPROCKET
	// did we weak link in DisplayMgr?
	if ((DMDisposeList!=NULL) && graphics_preferences->do_resolution_switching) //(mode->texture_floor)
	{
		SetResolutionGDSpec(&graphics_preferences->device_spec, NULL);
		did_switch_the_resolution_on_last_enter_screen= TRUE;
		did_we_ever_switched_resolution= TRUE;
	}
	else
	{
		memset(&resolution_restore_spec, 0, sizeof(GDSpec));
	}

	MoveWindow(screen_window, (*world_device)->gdRect.left, (*world_device)->gdRect.top, FALSE);
	SizeWindow(screen_window, RECTANGLE_WIDTH(&(*world_device)->gdRect), RECTANGLE_HEIGHT(&(*world_device)->gdRect), TRUE);

	{
		Point origin;
		
		origin.h= - (RECTANGLE_WIDTH(&(*world_device)->gdRect)-DESIRED_SCREEN_WIDTH)/2;
		origin.v= - (RECTANGLE_HEIGHT(&(*world_device)->gdRect)-DESIRED_SCREEN_HEIGHT)/2;
		if (origin.v>0) origin.v= 0;
		
		GetPort(&old_port);
		SetPort(screen_window);
		SetOrigin(origin.h, origin.v);
		SetPort(old_port);
	}
#endif

	GetPort(&old_port);
	SetPort(screen_window);

#if SUPPORT_DRAW_SPROCKET
	ForeColor( blackColor );
#endif

	PaintRect(&screen_window->portRect);
	SetPort(old_port);

#ifdef envppc
	assert_world_color_table(interface_color_table, world_color_table);
#endif

	change_screen_mode(&screen_mode, TRUE);

	switch (screen_mode.acceleration)
	{
		case _valkyrie_acceleration:
			valkyrie_begin();
			break;
	}

	return;
}

void exit_screen(
	void)
{

#if ! SUPPORT_DRAW_SPROCKET
#ifdef envppc
	if (did_switch_the_resolution_on_last_enter_screen)
	{
		SetResolutionGDSpec(&resolution_restore_spec, NULL);
//		DMSuspendConfigure(display_manager_state, 0);
//		DMEndConfigureDisplays(display_manager_state);
		did_switch_the_resolution_on_last_enter_screen= FALSE;
	}
	assert_world_color_table(interface_color_table, world_color_table);

	MoveWindow(screen_window, (*world_device)->gdRect.left, (*world_device)->gdRect.top, FALSE);
	SizeWindow(screen_window, RECTANGLE_WIDTH(&(*world_device)->gdRect), RECTANGLE_HEIGHT(&(*world_device)->gdRect), FALSE);

	{
		GrafPtr	old_port;
		Point origin;
		
		origin.h= - (RECTANGLE_WIDTH(&(*world_device)->gdRect)-DESIRED_SCREEN_WIDTH)/2;
		origin.v= - (RECTANGLE_HEIGHT(&(*world_device)->gdRect)-DESIRED_SCREEN_HEIGHT)/2;
		if (origin.v>0) origin.v= 0;
		
		GetPort(&old_port);
		SetPort(screen_window);
		SetOrigin(origin.h, origin.v);
		PaintRect(&screen_window->portRect);
		SetPort(old_port);
	}
#endif

#endif

	switch (screen_mode.acceleration)
	{
		case _valkyrie_acceleration:
			change_screen_mode(&screen_mode, FALSE);
			valkyrie_end();
			break;
	}
	
	return;
}

void change_screen_mode(
	struct screen_mode_data *mode,
	boolean redraw)
{
	short width, height;
	Rect bounds;
	GrafPtr old_port;
	OSErr error;

//	if (mode->high_resolution && mode->size==_full_screen && !enough_memory_for_full_screen)
//	{
//		mode->high_resolution= FALSE;
//	}

	switch (mode->acceleration)
	{
		case _valkyrie_acceleration:
			if (!world_view->overhead_map_active && !world_view->terminal_mode_active)
			{
				mode->high_resolution= FALSE;
			}
			break;
	}
	
	/* This makes sure that the terminal and map use their own resolutions */
	world_view->overhead_map_active= FALSE;
	world_view->terminal_mode_active= FALSE;

	if (redraw)
	{
		GetPort(&old_port);
		SetPort(screen_window);
		calculate_destination_frame(mode->size==_full_screen ? _full_screen : _100_percent, TRUE, &bounds);
		PaintRect(&bounds);
		SetPort(old_port);
	}

	switch (mode->acceleration)
	{
		case _valkyrie_acceleration:
			calculate_destination_frame(mode->size, mode->high_resolution, &bounds);
			OffsetRect(&bounds, -screen_window->portRect.left, -screen_window->portRect.top);
			valkyrie_initialize(world_device, TRUE, &bounds, 0xfe);
			if (redraw) valkyrie_erase_graphic_key_frame(0xfe);
			break;
	}
	
	/* save parameters */
	screen_mode= *mode;

	free_and_unlock_memory(); /* do our best to give UpdateGWorld memory */

	/* adjust the size of our GWorld based on mode->size and mode->resolution */
	calculate_adjusted_source_frame(mode, &bounds);
#if ! SUPPORT_DRAW_SPROCKET
	error= myUpdateGWorld(&world_pixels, 0, &bounds, (CTabHandle) NULL, (GDHandle) NULL, 0);
	if (error!=noErr) alert_user(fatalError, strERRORS, outOfMemory, error);
#endif
	
	calculate_source_frame(mode->size, mode->high_resolution, &bounds);
	width= RECTANGLE_WIDTH(&bounds), height= RECTANGLE_HEIGHT(&bounds);

	/* adjust and initialize the world_view structure */
#ifdef CYBERMAXX
	world_view->screen_width= width;
	world_view->screen_height= height/2;
	world_view->standard_screen_width= 2*height;
#else
	world_view->screen_width= width;
	world_view->screen_height= height;
	world_view->standard_screen_width= 2*height;
#endif
	initialize_view_data(world_view);

	frame_count= frame_index= 0;

#ifdef envppc
	resolution_restore_spec.bit_depth= graphics_preferences->device_spec.bit_depth;
#endif
	return;
}

void render_screen(
	short ticks_elapsed)
{
	PixMapHandle pixels;

	/* make whatever changes are necessary to the world_view structure based on whichever player
		is frontmost */
	world_view->ticks_elapsed= ticks_elapsed;
	world_view->tick_count= dynamic_world->tick_count;
	world_view->yaw= current_player->facing;
	world_view->pitch= current_player->elevation;
	world_view->maximum_depth_intensity= current_player->weapon_intensity;
	world_view->shading_mode= current_player->infravision_duration ? _shading_infravision : _shading_normal;
	if (current_player->extravision_duration)
	{
		if (world_view->field_of_view!=EXTRAVISION_FIELD_OF_VIEW && world_view->effect!=_render_effect_going_fisheye)
		{
			world_view->field_of_view= EXTRAVISION_FIELD_OF_VIEW;
			initialize_view_data(world_view);
		}
	}
	else
	{
		if (world_view->field_of_view!=NORMAL_FIELD_OF_VIEW && world_view->effect!=_render_effect_leaving_fisheye)
		{
			world_view->field_of_view= NORMAL_FIELD_OF_VIEW;
			initialize_view_data(world_view);
		}
	}
	
	if (PLAYER_HAS_MAP_OPEN(current_player))
	{
		if (!world_view->overhead_map_active) set_overhead_map_status(TRUE);
	}
	else
	{
		if (world_view->overhead_map_active) set_overhead_map_status(FALSE);
	}

	if(player_in_terminal_mode(current_player_index))
	{
		if (!world_view->terminal_mode_active) set_terminal_status(TRUE);
	} else {
		if (world_view->terminal_mode_active) set_terminal_status(FALSE);
	}

#if ! SUPPORT_DRAW_SPROCKET
	myLockPixels(world_pixels);
	pixels= myGetGWorldPixMap(world_pixels);
#else
	pixels=world_pixels->portPixMap;
#endif

	switch (screen_mode.acceleration)
	{
		case _valkyrie_acceleration:
			if (!world_view->overhead_map_active && !world_view->terminal_mode_active)
			{
				valkyrie_initialize_invisible_video_buffer(world_pixels_structure);
				break;
			}
			/* if we�re using the overhead map, fall through to no acceleration */
		case _no_acceleration:
			world_pixels_structure->width= world_view->screen_width;
			world_pixels_structure->height= world_view->screen_height;
			world_pixels_structure->flags= 0;
#ifdef DIRECT_SCREEN_TEST
			pixels= (*world_device)->gdPMap; //((CGrafPtr)screen_window)->portPixMap;
			world_pixels_structure->bytes_per_row= (*pixels)->rowBytes&0x3fff;
			world_pixels_structure->row_addresses[0]= ((pixel8 *)(*pixels)->baseAddr) + WORLD_H +
				WORLD_V*world_pixels_structure->bytes_per_row;
			/* blanking the screen is not supported in direct-to-screen mode */
#else /* !DIRECT_SCREEN_TEST */
			world_pixels_structure->bytes_per_row= (*pixels)->rowBytes&0x3fff;
			world_pixels_structure->row_addresses[0]= (pixel8 *) myGetPixBaseAddr(world_pixels);
#ifdef WHITE_SCREEN_BETWEEN_FRAMES
			{
				/* The terminal stuff only draws when necessary */
				if(!world_view->terminal_mode_active)
				{
					GDHandle old_device;
					CGrafPtr old_port;
					
					myGetGWorld(&old_port, &old_device);
					mySetGWorld(world_pixels, (GDHandle) NULL);
					EraseRect(&world_pixels->portRect);
					mySetGWorld(old_port, old_device);
				}
			}
#endif /* WHITE_SCREEN_BETWEEN_FRAMES */
#endif /* else DIRECT_SCREEN_TEST */
			precalculate_bitmap_row_addresses(world_pixels_structure);
			break;
		
		default:
			halt();
	}

	assert(world_view->screen_height<=MAXIMUM_WORLD_HEIGHT);

#ifdef CYBERMAXX
	/* render left and right images on alternating scan lines */
	{
		world_point3d left_origin, right_origin;
		short left_polygon_index, right_polygon_index;
		angle left_angle, right_angle;

		get_binocular_vision_origins(current_player_index, &left_origin, &left_polygon_index, &left_angle,
			&right_origin, &right_polygon_index, &right_angle);
		
		world_pixels_structure->bytes_per_row*= 2;
		precalculate_bitmap_row_addresses(world_pixels_structure);
		world_view->origin= left_origin;
		world_view->origin_polygon_index= left_polygon_index;
		world_view->yaw= left_angle;
		render_view(world_view, world_pixels_structure);
		
		world_pixels_structure->row_addresses[0]+= world_pixels_structure->bytes_per_row/2;
		precalculate_bitmap_row_addresses(world_pixels_structure);
		world_view->origin= right_origin;
		world_view->origin_polygon_index= right_polygon_index;
		world_view->yaw= right_angle;
		render_view(world_view, world_pixels_structure);
	}
#else /* !CYBERMAXX */
	world_view->origin= current_player->camera_location;
	world_view->origin_polygon_index= current_player->camera_polygon_index;
	render_view(world_view, world_pixels_structure);
#endif

	switch (screen_mode.acceleration)
	{
		case _valkyrie_acceleration:
			if (!world_view->overhead_map_active && !world_view->terminal_mode_active)
			{
				valkyrie_switch_to_invisible_video_buffer();
				break;
			}
			/* if we�re using the overhead map, fall through to no acceleration */
		case _no_acceleration:
			update_fps_display((GrafPtr)world_pixels);
#ifndef DIRECT_SCREEN_TEST
			update_screen();
#endif
			break;
		
		default:
			halt();
	}

#if ! SUPPORT_DRAW_SPROCKET
	myUnlockPixels(world_pixels);
#endif

	return;
}

void process_screen_click(
	EventRecord *event)
{
#pragma unused (event)
	return;
}

void change_interface_clut(
	struct color_table *color_table)
{
	memcpy(interface_color_table, color_table, sizeof(struct color_table));

	return;
}

/* builds color tables suitable for SetEntries (in either bit depth) and makes the screen look
	like we expect it to */
void change_screen_clut(
	struct color_table *color_table)
{
	if (interface_bit_depth==8 && bit_depth==8)
	{
		memcpy(uncorrected_color_table, color_table, sizeof(struct color_table));
		memcpy(interface_color_table, color_table, sizeof(struct color_table));
	}
	
	if (bit_depth==16 || bit_depth==32)
	{
		build_direct_color_table(uncorrected_color_table, bit_depth);
		if (interface_bit_depth!=8)
		{
			memcpy(interface_color_table, uncorrected_color_table, sizeof(struct color_table));
		}
	}

	gamma_correct_color_table(uncorrected_color_table, world_color_table, screen_mode.gamma_level);
	memcpy(visible_color_table, world_color_table, sizeof(struct color_table));

	/* switch to our device, stuff in our corrected color table */
	assert_world_color_table(interface_color_table, world_color_table);

	/* and finally, make sure the ctSeeds of our GWorld and our GDevice are synchronized */
	{
		Rect bounds;
		OSErr error;
	
		calculate_adjusted_source_frame(&screen_mode, &bounds);
#if ! SUPPORT_DRAW_SPROCKET
		error= myUpdateGWorld(&world_pixels, 0, &bounds, (CTabHandle) NULL, (GDHandle) NULL, 0);
		if (error!=noErr) alert_user(fatalError, strERRORS, outOfMemory, error);
#endif
	}
		
	return;
}

void build_direct_color_table(
	struct color_table *color_table,
	short bit_depth)
{
	struct rgb_color *color;
	short maximum_component;
	short i;
	
	switch (bit_depth)
	{
		case 16: maximum_component= PIXEL16_MAXIMUM_COMPONENT; break;
		case 32: maximum_component= PIXEL32_MAXIMUM_COMPONENT; break;
		default: halt();
	}
	
	color_table->color_count= maximum_component+1;
	for (i= 0, color= color_table->colors; i<=maximum_component; ++i, ++color)
	{
		color->red= color->green= color->blue= (i*0xffff)/maximum_component;
	}
	
	return;
}

void render_computer_interface(
	struct view_data *view)
{
	GWorldPtr old_gworld;
	GDHandle old_device;
	struct view_terminal_data data;

	GetGWorld(&old_gworld, &old_device);
	SetGWorld(world_pixels, (GDHandle) NULL);
	
	data.left= 0; data.right= view->screen_width;
	data.top= 0; data.bottom= view->screen_height;
	
	switch (screen_mode.size)
	{
			case _50_percent:
			case _75_percent:
				halt();
				break;
				
			case _100_percent:
				data.vertical_offset= 0;
				break;
			case _full_screen:
				data.vertical_offset= (view->screen_height-DEFAULT_WORLD_HEIGHT)/2;
				break;
	}
	_render_computer_interface(&data);

	RGBForeColor(&rgb_black);
	PenSize(1, 1);
	TextFont(0);
	TextFace(normal);
	TextSize(0);
	SetGWorld(old_gworld, old_device);
	
	return;
}

void render_overhead_map(
	struct view_data *view)
{
	GWorldPtr old_gworld;
	GDHandle old_device;
	struct overhead_map_data overhead_data;

	GetGWorld(&old_gworld, &old_device);
	SetGWorld(world_pixels, (GDHandle) NULL);
	
	PaintRect(&world_pixels->portRect);

	overhead_data.scale= view->overhead_map_scale;
	overhead_data.mode= _rendering_game_map;
	overhead_data.origin.x= view->origin.x;
	overhead_data.origin.y= view->origin.y;
	overhead_data.half_width= view->half_screen_width;
	overhead_data.half_height= view->half_screen_height;
	overhead_data.width= view->screen_width;
	overhead_data.height= view->screen_height;
	overhead_data.top= overhead_data.left= 0;
	
	_render_overhead_map(&overhead_data);

	RGBForeColor(&rgb_black);
	PenSize(1, 1);
	TextFont(0);
	TextFace(normal);
	TextSize(0);
	SetGWorld(old_gworld, old_device);
	
	return;
}

void zoom_overhead_map_out(
	void)
{
	world_view->overhead_map_scale= FLOOR(world_view->overhead_map_scale-1, OVERHEAD_MAP_MINIMUM_SCALE);
	
	return;
}

void zoom_overhead_map_in(
	void)
{
	world_view->overhead_map_scale= CEILING(world_view->overhead_map_scale+1, OVERHEAD_MAP_MAXIMUM_SCALE);
	
	return;
}

void start_teleporting_effect(
	boolean out)
{
	start_render_effect(world_view, out ? _render_effect_fold_out : _render_effect_fold_in);
}

void start_extravision_effect(
	boolean out)
{
	start_render_effect(world_view, out ? _render_effect_going_fisheye : _render_effect_leaving_fisheye);
}

/* These should be replaced with better preferences control functions */
boolean game_window_is_full_screen(
	void)
{
	return screen_mode.size==_full_screen;
}

boolean machine_supports_16bit(
	GDSpecPtr spec)
{
	GDSpec test_spec;
	boolean found_16bit_device= TRUE;

	/* Make sure that enough_memory_for_16bit is valid */
	calculate_screen_options();
	
	test_spec= *spec;
	test_spec.bit_depth= 16;
	if (!HasDepthGDSpec(&test_spec)) /* automatically searches for grayscale devices */
	{
		found_16bit_device= FALSE;
	}
	
	return found_16bit_device && enough_memory_for_16bit;
}

boolean machine_supports_32bit(
	GDSpecPtr spec)
{
	GDSpec test_spec;
	boolean found_32bit_device= TRUE;

	/* Make sure that enough_memory_for_16bit is valid */
	calculate_screen_options();

	test_spec= *spec;
	test_spec.bit_depth= 32;
	if (!HasDepthGDSpec(&test_spec)) /* automatically searches for grayscale devices */
	{
		found_32bit_device= FALSE;
	}
	
	return found_32bit_device && enough_memory_for_32bit;
}

short hardware_acceleration_code(
	GDSpecPtr spec)
{
	short acceleration_code= _no_acceleration;
	
	calculate_screen_options();

	if (machine_has_valkyrie(spec) && enough_memory_for_16bit)
	{
		acceleration_code= _valkyrie_acceleration;
	}
	
	return acceleration_code;
}

void update_screen_window(
	WindowPtr window,
	EventRecord *event)
{
	#pragma unused (window,event)
	
	draw_interface();
	change_screen_mode(&screen_mode, TRUE);
	assert_world_color_table(interface_color_table, world_color_table);
	
	return;
}

void activate_screen_window(
	WindowPtr window,
	EventRecord *event,
	boolean active)
{
	#pragma unused (window,event,active)
	
	return;
}

/* LowLevelSetEntries bypasses the Color Manager and goes directly to the hardware.  this means
	QuickDraw doesn�t fuck up during RGBForeColor and RGBBackColor. */
void animate_screen_clut(
	struct color_table *color_table,
	boolean full_screen)
{
	CTabHandle macintosh_color_table= build_macintosh_color_table(color_table);
	
	if (macintosh_color_table)
	{
		GDHandle old_device;

		HLock((Handle)macintosh_color_table);	
		old_device= GetGDevice();
		SetGDevice(world_device);
		switch (screen_mode.acceleration)
		{
			case _valkyrie_acceleration:
				if (!full_screen)
				{
					valkyrie_change_video_clut(macintosh_color_table);
					break;
				}
				// if we�re doing full-screen fall through to LowLevelSetEntries
	
			case _no_acceleration:
#if SUPPORT_DRAW_SPROCKET
				DSpContext_SetCLUTEntries( gDrawContext, (*macintosh_color_table)->ctTable, 0,
					(*macintosh_color_table)->ctSize );
#else
				LowLevelSetEntries(0, (*macintosh_color_table)->ctSize, (*macintosh_color_table)->ctTable);
#endif
				break;
		}
		
		DisposeHandle((Handle)macintosh_color_table);
		SetGDevice(old_device);
	}
	
	return;
}

void assert_world_color_table(
	struct color_table *interface_color_table,
	struct color_table *world_color_table)
{
	if (interface_bit_depth==8 && interface_color_table->color_count)
	{
		CTabHandle macintosh_color_table= build_macintosh_color_table(interface_color_table);
		
		if (macintosh_color_table)
		{
#if SUPPORT_DRAW_SPROCKET
			DSpContext_SetCLUTEntries( gDrawContext, (*macintosh_color_table)->ctTable, 0,
				(*macintosh_color_table)->ctSize );
#else
			GDHandle old_device;
			
			HLock((Handle)macintosh_color_table);
			old_device= GetGDevice();
			SetGDevice(world_device);
			SetEntries(0, (*macintosh_color_table)->ctSize, (*macintosh_color_table)->ctTable);
			SetGDevice(old_device);
			DisposeHandle((Handle)macintosh_color_table);
#endif
		}
	}
	
	if (world_color_table && world_color_table->color_count) animate_screen_clut(world_color_table, FALSE);
	
	return;
}

void darken_world_window(
	void)
{
	GrafPtr old_port;
	Rect bounds;
	
	GetPort(&old_port);
	SetPort(screen_window);
	PenPat(&qd.gray);
	PenMode(srcOr);
	calculate_destination_frame(screen_mode.size, screen_mode.high_resolution, &bounds);
	PaintRect(&bounds);
	PenMode(srcCopy);
	PenPat(&qd.black);
	SetPort(old_port);
	
	return;
}

void validate_world_window(
	void)
{
#if ! SUPPORT_DRAW_SPROCKET
	GrafPtr old_port;
	Rect bounds;
	
	GetPort(&old_port);
	SetPort(screen_window);
	calculate_destination_frame(screen_mode.size, screen_mode.high_resolution, &bounds);
	ValidRect(&bounds);
	SetPort(old_port);
#endif

	switch (screen_mode.acceleration)
	{
		case _valkyrie_acceleration:
			valkyrie_erase_graphic_key_frame(0xfe);
			break;
	}
	
	return;
}

void change_gamma_level(
	short gamma_level)
{
	screen_mode.gamma_level= gamma_level;
	gamma_correct_color_table(uncorrected_color_table, world_color_table, gamma_level);
	stop_fade();
	memcpy(visible_color_table, world_color_table, sizeof(struct color_table));
	assert_world_color_table(interface_color_table, world_color_table);
	change_screen_mode(&screen_mode, FALSE);
	set_fade_effect(NONE);
	
	return;
}

/* ---------- private code */

static void update_fps_display(
	GrafPtr port)
{
	if (displaying_fps)
	{
		long ticks= TickCount();
		GrafPtr old_port;
		char fps[100];
		
		frame_ticks[frame_index]= ticks;
		frame_index= (frame_index+1)%FRAME_SAMPLE_SIZE;
		if (frame_count<FRAME_SAMPLE_SIZE)
		{
			frame_count+= 1;
			strcpy(fps, (const char *)"\p--");
		}
		else
		{
			psprintf(fps, "%3.2ffps", (FRAME_SAMPLE_SIZE*60)/(float)(ticks-frame_ticks[frame_index]));
		}

		GetPort(&old_port);
		SetPort(port);
		TextSize(9);
		TextFont(monaco);
#if !SUPPORT_DRAW_SPROCKET
		MoveTo(5, port->portRect.bottom-5);
#else
		MoveTo(5, 20);
#endif
		RGBForeColor(&rgb_white);
		DrawString((StringPtr)fps);
		RGBForeColor(&rgb_black);
		SetPort(old_port);
	}
	else
	{
		frame_count= frame_index= 0;
	}
	
	return;
}

static void set_overhead_map_status( /* it has changed, this is the new status */
	boolean status)
{
	static struct screen_mode_data previous_screen_mode;
	
	if (!status)
	{
		screen_mode= previous_screen_mode;
	}
	else
	{
		previous_screen_mode= screen_mode;
		screen_mode.high_resolution= TRUE;
		switch (screen_mode.size)
		{
			case _50_percent:
			case _75_percent:
				screen_mode.size= _100_percent;
				break;
		}
	}
	world_view->overhead_map_active= status;
	change_screen_mode(&screen_mode, TRUE);
	world_view->overhead_map_active= status;
	
	return;
}

static void set_terminal_status( /* It has changed, this is the new state.. */
	boolean status)
{
	static struct screen_mode_data previous_screen_mode;
	boolean restore_effect= FALSE;
	short effect, phase;
	
	if(!status)
	{
		screen_mode= previous_screen_mode;
		if(world_view->effect==_render_effect_fold_out)
		{
			effect= world_view->effect;
			phase= world_view->effect_phase;
			restore_effect= TRUE;
		}
	}
	else
	{
		previous_screen_mode= screen_mode;
		screen_mode.high_resolution= TRUE;
		switch(screen_mode.size)
		{
			case _50_percent:
			case _75_percent:
				screen_mode.size= _100_percent;
				break;
		}
	}
	world_view->terminal_mode_active= status;
	change_screen_mode(&screen_mode, TRUE);
	world_view->terminal_mode_active= status;

	if(restore_effect)
	{
		world_view->effect= effect;
		world_view->effect_phase= phase;
	}

	/* Dirty the view.. */
	dirty_terminal_view(current_player_index);
	
	return;
}

static void restore_world_device(
	void)
{
	GrafPtr old_port;

	switch (screen_mode.acceleration)
	{
		case _valkyrie_acceleration:
			valkyrie_restore();
			break;
	}
	
	GetPort(&old_port);
	SetPort(screen_window);
	PaintRect(&screen_window->portRect);
	SetPort(old_port);
	
#if !SUPPORT_DRAW_SPROCKET

	ShowMenuBar();

#ifdef envppc
	if (did_switch_the_resolution_on_last_enter_screen || did_we_ever_switched_resolution)
	{
		SetResolutionGDSpec(&resolution_restore_spec, &switchInfo);
	}
	if ((DMDisposeList!=NULL))
	{
		DMSuspendConfigure(display_manager_state, 0);
		DMEndConfigureDisplays(display_manager_state);
	}
#endif

	/* put our device back the way we found it */
	SetDepthGDSpec(&restore_spec);
#endif

#if ! SUPPORT_DRAW_SPROCKET
	myDisposeGWorld(world_pixels);
	
	CloseWindow(screen_window);
	CloseWindow(backdrop_window);
#endif
	
	return;
}

/* pixels are already locked, etc. */
static void update_screen(
	void)
{
	Rect source, destination;
	
	calculate_source_frame(screen_mode.size, screen_mode.high_resolution, &source);
	calculate_destination_frame(screen_mode.size, screen_mode.high_resolution, &destination);

	if (screen_mode.high_resolution)
	{
		GrafPtr old_port;
		RGBColor old_forecolor, old_backcolor;
		
		GetPort(&old_port);
		SetPort(screen_window);

		GetForeColor(&old_forecolor);
		GetBackColor(&old_backcolor);
		RGBForeColor(&rgb_black);
		RGBBackColor(&rgb_white);
		
		OffsetRect(&source, -source.left, -source.top);
		
#if SUPPORT_DRAW_SPROCKET
		DSpContext_InvalBackBufferRect(gDrawContext, &source );
		DSpContext_SwapBuffers(gDrawContext, NULL, NULL);
		DSpContext_GetBackBuffer( gDrawContext, kDSpBufferKind_Normal, (CGrafPtr *)&world_pixels );
#else
		CopyBits((BitMapPtr)*world_pixels->portPixMap, &screen_window->portBits,
			&source, &destination, srcCopy, (RgnHandle) NULL);
#endif
		
		RGBForeColor(&old_forecolor);
		RGBBackColor(&old_backcolor);
		SetPort(old_port);
	}
	else
	{
		byte mode;
		short pelsize= bit_depth>>3;
		struct copy_screen_data data;
		short source_rowBytes, destination_rowBytes, source_width, destination_width;
		PixMapHandle screen_pixmap= (*world_device)->gdPMap;
		
#if ! SUPPORT_DRAW_SPROCKET
		source_rowBytes= (*myGetGWorldPixMap(world_pixels))->rowBytes&0x3fff;
#else
		source_rowBytes = (*world_pixels->portPixMap)->rowBytes & 0x3FFF;
#endif
		
		destination_rowBytes= (*screen_pixmap)->rowBytes&0x3fff;
		source_width= RECTANGLE_WIDTH(&source);
		destination_width= RECTANGLE_WIDTH(&destination);
		
#if ! SUPPORT_DRAW_SPROCKET
		data.source= (byte *)myGetPixBaseAddr(world_pixels);
#else
		data.source = (byte *)(*world_pixels->portPixMap)->baseAddr;
#endif		
		data.source_slop= source_rowBytes-source_width*pelsize;
		
		data.destination= (byte *)(*screen_pixmap)->baseAddr + (destination.top-screen_window->portRect.top)*destination_rowBytes +
			(destination.left-screen_window->portRect.left)*pelsize;
		data.destination_slop= destination_rowBytes-destination_width*pelsize;

		data.bytes_per_row= destination_rowBytes;
		data.height= RECTANGLE_HEIGHT(&source);

		assert(!((long)data.source&3));
		assert(!((long)data.destination&3));
		assert(!(data.destination_slop&3));
		assert(!(data.source_slop&3));
		assert(!(data.bytes_per_row&3));

		data.flags= 0;
		data.destination_slop+= destination_rowBytes;
#ifdef env68k
		if (screen_mode.draw_every_other_line) data.flags|= _EVERY_OTHER_LINE_BIT;
#endif
		switch (bit_depth)
		{
			case 8:
				data.width= source_width>>2;
				break;
			case 16:
				data.width= source_width>>1;
				data.flags|= _SIXTEEN_BIT_BIT;
				break;
			case 32:
				data.width= source_width;
				data.flags|= _THIRTYTWO_BIT_BIT;
				break;
			default:
				halt();
		}

		mode= true32b;
		SwapMMUMode((signed char *)&mode);
		quadruple_screen(&data);
		SwapMMUMode((signed char *)&mode);
	}

	return;
}

/* This function is NOT void because both the computer interface editor and vulcan use it */
/*  to determine the size of their gworlds.. */
void calculate_destination_frame(
	short size,
	boolean high_resolution,
	Rect *frame)
{
	#pragma unused (high_resolution)
	
	/* calculate destination frame */
	switch (size)
	{
		case _full_screen: /* ow */
			SetRect(frame, 0, 0, DESIRED_SCREEN_WIDTH, DESIRED_SCREEN_HEIGHT);
			break;
		case _100_percent:
			SetRect(frame, 0, 0, DEFAULT_WORLD_WIDTH, DEFAULT_WORLD_HEIGHT);
			break;
		case _75_percent:
			SetRect(frame, 0, 0, 3*DEFAULT_WORLD_WIDTH/4, 3*DEFAULT_WORLD_HEIGHT/4);
			break;
		case _50_percent:
			SetRect(frame, 0, 0, DEFAULT_WORLD_WIDTH/2, DEFAULT_WORLD_HEIGHT/2);
			break;
	}
	
	if (size!=_full_screen)
	{
		OffsetRect(frame, WORLD_H+((DEFAULT_WORLD_WIDTH-frame->right)>>1), 
			WORLD_V+((DEFAULT_WORLD_HEIGHT-frame->bottom)>>1));
	}
	
	return;
}

enum
{
	BITS_PER_CACHE_LINE= 6,
	CACHE_LINE_SIZE= (1<<BITS_PER_CACHE_LINE) // ppc601
};

static void calculate_adjusted_source_frame(
	struct screen_mode_data *mode,
	Rect *frame)
{
	calculate_source_frame(mode->size, mode->high_resolution, frame);

#ifdef OBSOLETE
	short width;

#ifdef envppc
#if 0
	// calculate width in bytes
	width= RECTANGLE_WIDTH(frame);
	switch (mode->bit_depth)
	{
		case 8: break;
		case 16: width<<= 1; break;
		case 32: width<<= 2; break;
		default: halt();
	}

	// assure that our width is an odd-multiple of our cache line size
	if (width&(CACHE_LINE_SIZE-1)) width= (width&~(CACHE_LINE_SIZE-1)) + CACHE_LINE_SIZE;
	if (!((width>>BITS_PER_CACHE_LINE)&1)) width+= CACHE_LINE_SIZE;
	
	// restore width in pixels
	switch (mode->bit_depth)
	{
		case 8: break;
		case 16: width>>= 1; break;
		case 32: width>>= 2; break;
		default: halt();
	}
#else
	if (mode->bit_depth==8) width= 704;
#endif
	frame->right= frame->left + width;
#endif
#endif

	return;
}

static void calculate_source_frame(
	short size,
	boolean high_resolution,
	Rect *frame)
{
	calculate_destination_frame(size, high_resolution, frame);
	if (!high_resolution)
	{
		frame->right-= (RECTANGLE_WIDTH(frame)>>1);
		frame->bottom-= (RECTANGLE_HEIGHT(frame)>>1);
	}
	OffsetRect(frame, (*world_device)->gdRect.left-frame->left,
		(*world_device)->gdRect.top-frame->top);
	
	return;
}

static void calculate_screen_options(
	void)
{
	static boolean screen_options_initialized= FALSE;
	
	if(!screen_options_initialized)
	{
		enough_memory_for_16bit= (FreeMem()>FREE_MEMORY_FOR_16BIT) ? TRUE : FALSE;
		enough_memory_for_32bit= (FreeMem()>FREE_MEMORY_FOR_32BIT) ? TRUE : FALSE;
		enough_memory_for_full_screen= (FreeMem()>FREE_MEMORY_FOR_FULL_SCREEN) ? TRUE : FALSE;
		screen_options_initialized= TRUE;
	}
}


#ifndef EXTERNAL
void quadruple_screen(
	struct copy_screen_data *data)
{
	register short bytes_per_row= data->bytes_per_row;
	register long *read= (long *) data->source;
	register long *write1= (long *) data->destination;
	register long *write2= (long *) ((byte *)data->destination + bytes_per_row);
	register unsigned long in, out1, out2;
	register short count;
	short rows_left;

	for (rows_left=data->height;rows_left>0;--rows_left)
	{
		if (data->flags&_THIRTYTWO_BIT_BIT)
		{
			for (count= data->width; count>0; --count)
			{
				in= *read++;
				*write1++= *write2++= *write1++= *write2++= in;
			}
		}
		else
		{
			if (data->flags&_SIXTEEN_BIT_BIT)
			{
				for (count=data->width;count>0;--count)
				{
					in= *read++;
					*write1++= *write2++= (in&0xffff0000) | (in>>16);
					*write1++= *write2++= (in&0x0000ffff) | (in<<16);
				}
			}
			else
			{
				if (data->flags&_EVERY_OTHER_LINE_BIT)
				{
					for (count=data->width;count>0;--count)
					{
						in= *read++;
						*write1++= ((unsigned long)(((word)(in&0xff)) | ((word)(in<<8)))) | (unsigned long)((((in>>8)&0xff) | (in&0xff00))<<16);
						in>>= 16;
						*write1++= ((unsigned long)(((word)(in&0xff)) | ((word)(in<<8)))) | (unsigned long)((((in>>8)&0xff) | (in&0xff00))<<16);
					}
				}
				else
				{
					for (count=data->width;count>0;--count)
					{
						in= *read++;
						out2= ((unsigned long)(((word)(in&0xff)) | ((word)(in<<8)))) | (unsigned long)((((in>>8)&0xff) | (in&0xff00))<<16);
						in>>= 16;
						out1= ((unsigned long)(((word)(in&0xff)) | ((word)(in<<8)))) | (unsigned long)((((in>>8)&0xff) | (in&0xff00))<<16);
						
						*write1++= out1, *write2++= out1;
						*write1++= out2, *write2++= out2;
					}
				}
			}
		}
		
		(byte*)read+= data->source_slop;
		(byte*)write1+= data->destination_slop;
		(byte*)write2+= data->destination_slop;
	}
	
	return;
}
#endif

//	CJD -- just a function to clear the attributes to a known init state
#if SUPPORT_DRAW_SPROCKET
static void
InitDrawSprocketAttributes(DSpContextAttributes *inAttributes)
{
	if( NULL == inAttributes )
		DebugStr("\pNULL context sent to InitDrawSprocketAttributes!");
		
	inAttributes->frequency					= 0;
	inAttributes->displayWidth				= 0;
	inAttributes->displayHeight				= 0;
	inAttributes->reserved1					= 0;
	inAttributes->reserved2					= 0;
	inAttributes->colorNeeds				= 0;
	inAttributes->colorTable				= NULL;
	inAttributes->contextOptions			= 0;
	inAttributes->backBufferDepthMask		= 0;
	inAttributes->displayDepthMask			= 0;
	inAttributes->backBufferBestDepth		= 0;
	inAttributes->displayBestDepth			= 0;
	inAttributes->pageCount					= 0;
	inAttributes->gameMustConfirmSwitch		= false;
	inAttributes->reserved3[0]				= 0;
	inAttributes->reserved3[1]				= 0;
	inAttributes->reserved3[2]				= 0;
	inAttributes->reserved3[3]				= 0;
}
#endif
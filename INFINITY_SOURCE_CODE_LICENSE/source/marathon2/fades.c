/*
FADES.C
Friday, June 10, 1994 6:59:04 PM

Saturday, June 11, 1994 12:49:19 AM
	not doing signed math on the color deltas resulted in some very cool static-ish effects
	resulting from integer overflow.
Saturday, July 9, 1994 1:06:20 PM
	fade_finished() was acting like fade_not_finished().
Sunday, September 25, 1994 12:50:34 PM  (Jason')
	cool new fades.
Monday, April 3, 1995 11:22:58 AM  (Jason')
	fade effects for underwater/lava/goo/sewage.
Monday, July 10, 1995 8:23:03 PM  (Jason)
	random transparencies
Thursday, August 24, 1995 6:20:06 PM  (Jason)
	removed macintosh dependencies
Monday, October 30, 1995 8:02:12 PM  (Jason)
	fade prioritites for juggernaut flash
*/

#include "cseries.h"
#include "fades.h"
#include "screen.h"
#include "textures.h"

#include <string.h>
#include <math.h>

#ifdef mpwc
#pragma segment shell
#endif

/* ---------- constants */

enum
{
	ADJUSTED_TRANSPARENCY_DOWNSHIFT= 8,

	MINIMUM_FADE_RESTART_TICKS= MACHINE_TICKS_PER_SECOND/2,
	MINIMUM_FADE_UPDATE_TICKS= MACHINE_TICKS_PER_SECOND/8
};

/* ---------- macros */

#define FADES_RANDOM() ((fades_random_seed&1) ? (fades_random_seed= (fades_random_seed>>1)^0xb400) : (fades_random_seed>>= 1))

/* ---------- types */

typedef void (*fade_proc)(struct color_table *original_color_table, struct color_table *animated_color_table, struct rgb_color *color, fixed transparency);

/* ---------- structures */

/* an effect is something which is applied to the original color table before a fade begins */
struct fade_effect_definition
{
	short fade_type;
	fixed transparency;
};

enum
{
	_full_screen_flag= 0x0001,
	_random_transparency_flag= 0x0002
};

struct fade_definition
{
	fade_proc proc;
	struct rgb_color color;
	fixed initial_transparency, final_transparency; /* in [0,FIXED_ONE] */

	short period;
	
	word flags;
	short priority; // higher is higher
};

#define FADE_IS_ACTIVE(f) ((f)->flags&(word)0x8000)
#define SET_FADE_ACTIVE_STATUS(f,s) ((s)?((f)->flags|=(word)0x8000):((f)->flags&=(word)~0x8000))

struct fade_data
{
	word flags; /* [active.1] [unused.15] */
	
	short type;
	short fade_effect_type;
	
	long start_tick;
	long last_update_tick;
	
	struct color_table *original_color_table;
	struct color_table *animated_color_table;
};

/* ---------- globals */

static struct fade_data *fade;

static word fades_random_seed= 0x1;

/* ---------- fade definitions */

static void tint_color_table(struct color_table *original_color_table, struct color_table *animated_color_table, struct rgb_color *color, fixed transparency);
static void randomize_color_table(struct color_table *original_color_table, struct color_table *animated_color_table, struct rgb_color *color, fixed transparency);
static void negate_color_table(struct color_table *original_color_table, struct color_table *animated_color_table, struct rgb_color *color, fixed transparency);
static void dodge_color_table(struct color_table *original_color_table, struct color_table *animated_color_table, struct rgb_color *color, fixed transparency);
static void burn_color_table(struct color_table *original_color_table, struct color_table *animated_color_table, struct rgb_color *color, fixed transparency);
static void soft_tint_color_table(struct color_table *original_color_table, struct color_table *animated_color_table, struct rgb_color *color, fixed transparency);

static struct fade_definition fade_definitions[NUMBER_OF_FADE_TYPES]=
{
	{tint_color_table, {0, 0, 0}, FIXED_ONE, FIXED_ONE, 0, _full_screen_flag, 0}, /* _start_cinematic_fade_in */
	{tint_color_table, {0, 0, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, _full_screen_flag, 0}, /* _cinematic_fade_in */
	{tint_color_table, {0, 0, 0}, FIXED_ONE, 0, 3*MACHINE_TICKS_PER_SECOND/2, _full_screen_flag, 0}, /* _long_cinematic_fade_in */
	{tint_color_table, {0, 0, 0}, 0, FIXED_ONE, MACHINE_TICKS_PER_SECOND/2, _full_screen_flag, 0}, /* _cinematic_fade_out */
	{tint_color_table, {0, 0, 0}, 0, 0, 0, _full_screen_flag, 0}, /* _end_cinematic_fade_out */
	
	{tint_color_table, {65535, 0, 0}, (3*FIXED_ONE)/4, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, /* _fade_red */
	{tint_color_table, {65535, 0, 0}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/4, 0, 25}, /* _fade_big_red */
	{tint_color_table, {0, 65535, 0}, FIXED_ONE_HALF, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, /* _fade_bonus */
	{tint_color_table, {65535, 65535, 50000}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/3, 0, 0}, /* _fade_bright */
	{tint_color_table, {65535, 65535, 50000}, FIXED_ONE, 0, 4*MACHINE_TICKS_PER_SECOND, 0, 100}, /* _fade_long_bright */
	{tint_color_table, {65535, 65535, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, 0, 50}, /* _fade_yellow */
	{tint_color_table, {65535, 65535, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND, 0, 75}, /* _fade_big_yellow */
	{tint_color_table, {215*256, 107*256, 65535}, (3*FIXED_ONE)/4, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, /* _fade_purple */
	{tint_color_table, {169*256, 65535, 224*256}, (3*FIXED_ONE)/4, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, /* _fade_cyan */
	{tint_color_table, {65535, 65535, 65535}, FIXED_ONE_HALF, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, /* _fade_white */
	{tint_color_table, {65535, 65535, 65535}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, 0, 25}, /* _fade_big_white */
	{tint_color_table, {65535, 32768, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/4, 0, 0}, /* _fade_orange */
	{tint_color_table, {65535, 32768, 0}, FIXED_ONE/4, 0, 3*MACHINE_TICKS_PER_SECOND, 0, 25}, /* _fade_long_orange */
	{tint_color_table, {0, 65535, 0}, 3*FIXED_ONE/4, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, /* _fade_green */
	{tint_color_table, {65535, 0, 65535}, FIXED_ONE/4, 0, 3*MACHINE_TICKS_PER_SECOND, 0, 25}, /* _fade_long_green */

	{randomize_color_table, {0, 0, 0}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/8, 0, 0}, /* _fade_static */
	{negate_color_table, {65535, 65535, 65535}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, 0, 0}, /* _fade_negative */
	{negate_color_table, {65535, 65535, 65535}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/2, 0, 25}, /* _fade_big_negative */
	{negate_color_table, {0, 65535, 0}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND/2, _random_transparency_flag, 0}, /* _fade_flicker_negative */
	{dodge_color_table, {0, 65535, 0}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/4, 0, 0}, /* _fade_dodge_purple */
	{burn_color_table, {0, 65535, 65535}, FIXED_ONE, 0, MACHINE_TICKS_PER_SECOND, 0, 0}, /* _fade_burn_cyan */
	{dodge_color_table, {0, 0, 65535}, FIXED_ONE, 0, (3*MACHINE_TICKS_PER_SECOND)/2, 0, 0}, /* _fade_dodge_yellow */
	{burn_color_table, {0, 65535, 0}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, /* _fade_burn_green */

	{soft_tint_color_table, {137*256, 0, 137*256}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, /* _fade_tint_purple */
	{soft_tint_color_table, {0, 0, 65535}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, /* _fade_tint_blue */
	{soft_tint_color_table, {65535, 16384, 0}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, /* _fade_tint_orange */
	{soft_tint_color_table, {32768, 65535, 0}, FIXED_ONE, 0, 2*MACHINE_TICKS_PER_SECOND, 0, 0}, /* _fade_tint_gross */
};

struct fade_effect_definition fade_effect_definitions[NUMBER_OF_FADE_EFFECT_TYPES]=
{
	{_fade_tint_blue, 3*FIXED_ONE/4}, /* _effect_under_water */
	{_fade_tint_orange, 3*FIXED_ONE/4}, /* _effect_under_lava */
	{_fade_tint_gross, 3*FIXED_ONE/4}, /* _effect_under_sewage */
	{_fade_tint_green, 3*FIXED_ONE/4}, /* _effect_under_goo */
};

static float actual_gamma_values[NUMBER_OF_GAMMA_LEVELS]=
{
	1.3,
	1.15,
	1.0, // default
	0.95,
	0.90,
	0.85,
	0.77,
	0.70
};

/* ---------- private prototypes */

static struct fade_definition *get_fade_definition(short index);
static struct fade_effect_definition *get_fade_effect_definition(short index);

static void recalculate_and_display_color_table(short type, fixed transparency,
	struct color_table *original_color_table, struct color_table *animated_color_table);

/* ---------- code */

void initialize_fades(
	void)
{
	/* allocate and initialize space for our fade_data structure */
	fade= (struct fade_data *) malloc(sizeof(struct fade_data));
	assert(fade);
	
	SET_FADE_ACTIVE_STATUS(fade, FALSE);
	fade->fade_effect_type= NONE;
	
	return;
}

boolean update_fades(
	void)
{
	if (FADE_IS_ACTIVE(fade))
	{
		struct fade_definition *definition= get_fade_definition(fade->type);
		long tick_count= machine_tick_count();
		boolean update= FALSE;
		fixed transparency;
		short phase;
		
		if ((phase= tick_count-fade->start_tick)>=definition->period)
		{
			transparency= definition->final_transparency;
			SET_FADE_ACTIVE_STATUS(fade, FALSE);
			
			update= TRUE;
		}
		else
		{
			if (tick_count-fade->last_update_tick>=MINIMUM_FADE_UPDATE_TICKS)
			{
				transparency= definition->initial_transparency + (phase*(definition->final_transparency-definition->initial_transparency))/definition->period;
				if (definition->flags&_random_transparency_flag) transparency+= FADES_RANDOM()%(definition->final_transparency-transparency);
				
				update= TRUE;
			}
		}
		
		if (update) recalculate_and_display_color_table(fade->type, transparency, fade->original_color_table, fade->animated_color_table);
	}
	
	return FADE_IS_ACTIVE(fade) ? TRUE : FALSE;
}

void set_fade_effect(
	short type)
{
	if (fade->fade_effect_type!=type)
	{
		fade->fade_effect_type= type;
		
		if (!FADE_IS_ACTIVE(fade))
		{
			if (type==NONE)
			{
				animate_screen_clut(world_color_table, FALSE);
			}
			else
			{
				recalculate_and_display_color_table(NONE, 0, world_color_table, visible_color_table);
			}
		}
	}
	
	return;
}

void start_fade(
	short type)
{
	explicit_start_fade(type, world_color_table, visible_color_table);
	
	return;
}

void explicit_start_fade(
	short type,
	struct color_table *original_color_table,
	struct color_table *animated_color_table)
{
	struct fade_definition *definition= get_fade_definition(type);
	long tick_count= machine_tick_count();
	boolean do_fade= TRUE;

	if (FADE_IS_ACTIVE(fade))
	{
		struct fade_definition *old_definition= get_fade_definition(fade->type);
		
		if (old_definition->priority>definition->priority) do_fade= FALSE;
		if (tick_count-fade->start_tick<MINIMUM_FADE_RESTART_TICKS && fade->type==type) do_fade= FALSE;
	}

	if (do_fade)
	{
		SET_FADE_ACTIVE_STATUS(fade, FALSE);
	
		recalculate_and_display_color_table(type, definition->initial_transparency, original_color_table, animated_color_table);
		if (definition->period)
		{
			fade->type= type;
			fade->start_tick= fade->last_update_tick= tick_count;
			fade->original_color_table= original_color_table;
			fade->animated_color_table= animated_color_table;
			SET_FADE_ACTIVE_STATUS(fade, TRUE);
		}
	}
	
	return;
}

void stop_fade(
	void)
{
	if (FADE_IS_ACTIVE(fade))
	{
		struct fade_definition *definition= get_fade_definition(fade->type);
		
		recalculate_and_display_color_table(fade->type, definition->final_transparency,
			fade->original_color_table, fade->animated_color_table);
		
		SET_FADE_ACTIVE_STATUS(fade, FALSE);
	}
	
	return;
}

boolean fade_finished(
	void)
{
	return FADE_IS_ACTIVE(fade) ? FALSE : TRUE;
}

void full_fade(
	short type,
	struct color_table *original_color_table)
{
	struct color_table animated_color_table;
	
	memcpy(&animated_color_table, original_color_table, sizeof(struct color_table));
	
#if !DRAW_SPROCKET_SUPPORT
	explicit_start_fade(type, original_color_table, &animated_color_table);
	while (update_fades())
		; /* empty loop body */
#endif
	return;
}

short get_fade_period(
	short type)
{
	struct fade_definition *definition= get_fade_definition(type);
	
	return definition->period;
}

void gamma_correct_color_table(
	struct color_table *uncorrected_color_table,
	struct color_table *corrected_color_table,
	short gamma_level)
{
	short i;
	float gamma;
	struct rgb_color *uncorrected= uncorrected_color_table->colors;
	struct rgb_color *corrected= corrected_color_table->colors;
	
	assert(gamma_level>=0 && gamma_level<NUMBER_OF_GAMMA_LEVELS);
	gamma= actual_gamma_values[gamma_level];
	
	corrected_color_table->color_count= uncorrected_color_table->color_count;
	for (i= 0; i<uncorrected_color_table->color_count; ++i, ++corrected, ++uncorrected)
	{
		corrected->red= pow(uncorrected->red/65535.0, gamma)*65535.0;
		corrected->green= pow(uncorrected->green/65535.0, gamma)*65535.0;
		corrected->blue= pow(uncorrected->blue/65535.0, gamma)*65535.0;
	}
	
	return;
}

/* ---------- private code */

static struct fade_definition *get_fade_definition(
	short index)
{
	assert(index>=0 && index<NUMBER_OF_FADE_TYPES);
	
	return fade_definitions + index;
}

static struct fade_effect_definition *get_fade_effect_definition(
	short index)
{
	assert(index>=0 && index<NUMBER_OF_FADE_EFFECT_TYPES);
	
	return fade_effect_definitions + index;
}

static void recalculate_and_display_color_table(
	short type,
	fixed transparency,
	struct color_table *original_color_table,
	struct color_table *animated_color_table)
{
	boolean full_screen= FALSE;
	
	/* if a fade effect is active, apply it first */
	if (fade->fade_effect_type!=NONE)
	{
		struct fade_effect_definition *effect_definition= get_fade_effect_definition(fade->fade_effect_type);
		struct fade_definition *definition= get_fade_definition(effect_definition->fade_type);
		
		definition->proc(original_color_table, animated_color_table, &definition->color, effect_definition->transparency);
		original_color_table= animated_color_table;
	}

	if (type!=NONE)
	{
		struct fade_definition *definition= get_fade_definition(type);

		definition->proc(original_color_table, animated_color_table, &definition->color, transparency);	
		full_screen= (definition->flags&_full_screen_flag) ? TRUE : FALSE;
	}
	
	animate_screen_clut(animated_color_table, full_screen);
	
	return;
}

/* ---------- fade functions */

static void tint_color_table(
	struct color_table *original_color_table,
	struct color_table *animated_color_table,
	struct rgb_color *color,
	fixed transparency)
{
	short i;
	struct rgb_color *unadjusted= original_color_table->colors;
	struct rgb_color *adjusted= animated_color_table->colors;
	short adjusted_transparency= transparency>>ADJUSTED_TRANSPARENCY_DOWNSHIFT;

	animated_color_table->color_count= original_color_table->color_count;
	for (i= 0; i<original_color_table->color_count; ++i, ++adjusted, ++unadjusted)
	{
		adjusted->red= unadjusted->red + (((color->red-unadjusted->red)*adjusted_transparency)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT));
		adjusted->green= unadjusted->green + (((color->green-unadjusted->green)*adjusted_transparency)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT));
		adjusted->blue= unadjusted->blue + (((color->blue-unadjusted->blue)*adjusted_transparency)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT));
	}
	
	return;
}

static void randomize_color_table(
	struct color_table *original_color_table,
	struct color_table *animated_color_table,
	struct rgb_color *color,
	fixed transparency)
{
	short i;
	struct rgb_color *unadjusted= original_color_table->colors;
	struct rgb_color *adjusted= animated_color_table->colors;
	word mask, adjusted_transparency= PIN(transparency, 0, 0xffff);

	#pragma unused (color)

	/* calculate a mask which has all bits including and lower than the high-bit in the
		transparency set */
	for (mask= 0;~mask & adjusted_transparency;mask= (mask<<1)|1)
		; /* empty loop body */
	
	animated_color_table->color_count= original_color_table->color_count;
	for (i= 0; i<original_color_table->color_count; ++i, ++adjusted, ++unadjusted)
	{
		adjusted->red= unadjusted->red + (FADES_RANDOM()&mask);
		adjusted->green= unadjusted->green + (FADES_RANDOM()&mask);
		adjusted->blue= unadjusted->blue + (FADES_RANDOM()&mask);
	}
	
	return;
}

/* unlike pathways, all colors won�t pass through 50% gray at the same time */
static void negate_color_table(
	struct color_table *original_color_table,
	struct color_table *animated_color_table,
	struct rgb_color *color,
	fixed transparency)
{
	short i;
	struct rgb_color *unadjusted= original_color_table->colors;
	struct rgb_color *adjusted= animated_color_table->colors;
	
	transparency= FIXED_ONE-transparency;
	animated_color_table->color_count= original_color_table->color_count;
	for (i= 0; i<original_color_table->color_count; ++i, ++adjusted, ++unadjusted)
	{
		adjusted->red= (unadjusted->red>0x8000) ?
			CEILING((unadjusted->red^color->red)+transparency, (long)unadjusted->red) :
			FLOOR((unadjusted->red^color->red)-transparency, (long)unadjusted->red);
		adjusted->green= (unadjusted->green>0x8000) ?
			CEILING((unadjusted->green^color->green)+transparency, (long)unadjusted->green) :
			FLOOR((unadjusted->green^color->green)-transparency, (long)unadjusted->green);
		adjusted->blue= (unadjusted->blue>0x8000) ?
			CEILING((unadjusted->blue^color->blue)+transparency, (long)unadjusted->blue) :
			FLOOR((unadjusted->blue^color->blue)-transparency, (long)unadjusted->blue);
	}
	
	return;
}

static void dodge_color_table(
	struct color_table *original_color_table,
	struct color_table *animated_color_table,
	struct rgb_color *color,
	fixed transparency)
{
	short i;
	struct rgb_color *unadjusted= original_color_table->colors;
	struct rgb_color *adjusted= animated_color_table->colors;
	
	animated_color_table->color_count= original_color_table->color_count;
	for (i= 0; i<original_color_table->color_count; ++i, ++adjusted, ++unadjusted)
	{
		long component;
		
		component= 0xffff - (((color->red^0xffff)*unadjusted->red)>>FIXED_FRACTIONAL_BITS) - transparency, adjusted->red= CEILING(component, unadjusted->red);
		component= 0xffff - (((color->green^0xffff)*unadjusted->green)>>FIXED_FRACTIONAL_BITS) - transparency, adjusted->green= CEILING(component, unadjusted->green);
		component= 0xffff - (((color->blue^0xffff)*unadjusted->blue)>>FIXED_FRACTIONAL_BITS) - transparency, adjusted->blue= CEILING(component, unadjusted->blue);
	}
	
	return;
}

static void burn_color_table(
	struct color_table *original_color_table,
	struct color_table *animated_color_table,
	struct rgb_color *color,
	fixed transparency)
{
	short i;
	struct rgb_color *unadjusted= original_color_table->colors;
	struct rgb_color *adjusted= animated_color_table->colors;
	
	transparency= FIXED_ONE-transparency;
	animated_color_table->color_count= original_color_table->color_count;
	for (i= 0; i<original_color_table->color_count; ++i, ++adjusted, ++unadjusted)
	{
		long component;
		
		component= ((color->red*unadjusted->red)>>FIXED_FRACTIONAL_BITS) + transparency, adjusted->red= CEILING(component, unadjusted->red);
		component= ((color->green*unadjusted->green)>>FIXED_FRACTIONAL_BITS) + transparency, adjusted->green= CEILING(component, unadjusted->green);
		component= ((color->blue*unadjusted->blue)>>FIXED_FRACTIONAL_BITS) + transparency, adjusted->blue= CEILING(component, unadjusted->blue);
	}

	return;
}

static void soft_tint_color_table(
	struct color_table *original_color_table,
	struct color_table *animated_color_table,
	struct rgb_color *color,
	fixed transparency)
{
	short i;
	struct rgb_color *unadjusted= original_color_table->colors;
	struct rgb_color *adjusted= animated_color_table->colors;
	word adjusted_transparency= transparency>>ADJUSTED_TRANSPARENCY_DOWNSHIFT;
	
	animated_color_table->color_count= original_color_table->color_count;
	for (i= 0; i<original_color_table->color_count; ++i, ++adjusted, ++unadjusted)
	{
		word intensity;
		
		intensity= MAX(unadjusted->red, unadjusted->green);
		intensity= MAX(intensity, unadjusted->blue)>>ADJUSTED_TRANSPARENCY_DOWNSHIFT;
		
		adjusted->red= unadjusted->red + (((((color->red*intensity)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT))-unadjusted->red)*adjusted_transparency)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT));
		adjusted->green= unadjusted->green + (((((color->green*intensity)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT))-unadjusted->green)*adjusted_transparency)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT));
		adjusted->blue= unadjusted->blue + (((((color->blue*intensity)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT))-unadjusted->blue)*adjusted_transparency)>>(FIXED_FRACTIONAL_BITS-ADJUSTED_TRANSPARENCY_DOWNSHIFT));
	}

	return;
}

/*
SHAPES.C
Saturday, September 4, 1993 9:26:41 AM

Thursday, May 19, 1994 9:06:28 AM
	unification of wall and object shapes complete, new shading table builder.
Wednesday, June 22, 1994 11:55:07 PM
	we now read data from alain�s shape extractor.
Saturday, July 9, 1994 3:22:11 PM
	lightening_table removed; we now build darkening tables on a collection-by-collection basis
	(one 8k darkening table per clut permutation of the given collection)
Monday, October 3, 1994 4:17:15 PM (Jason)
	compressed or uncompressed collection resources
Friday, June 16, 1995 11:34:08 AM  (Jason)
	self-luminescent colors
*/

/*
//gracefully handle out-of-memory conditions when loading shapes.  it will happen.
//get_shape_descriptors() needs to look at high-level instead of low-level shapes when fetching scenery instead of walls/ceilings/floors
//get_shape_information() is called often, and is quite slow
//it is possible to have more than 255 low-level shapes in a collection, which means the existing shape_descriptor is too small
//must build different shading tables for each collection (even in 8-bit, for alternate color tables)
*/

#include "macintosh_cseries.h"
#include "byte_swapping.h"

#include "shell.h"
#include "render.h"
#include "interface.h"
#include "rle.h"
#include "collection_definition.h"
#include "screen.h"

#include "map.h"

#ifdef mpwc
#pragma segment shell
#endif

//#define SCREAMING_METAL

/* ---------- constants */

#define iWHITE 1
#ifdef SCREAMING_METAL
#define iBLACK 255
#else
#define iBLACK 18
#endif

/* each collection has a tint table which (fully) tints the clut of that collection to whatever it
	looks like through the light enhancement goggles */
#define NUMBER_OF_TINT_TABLES 1

/* ---------- macros */

/* ---------- structures */

/* ---------- globals */

#include "shape_definitions.h"

static pixel16 *global_shading_table16= (pixel16 *) NULL;
static pixel32 *global_shading_table32= (pixel32 *) NULL;

short number_of_shading_tables, shading_table_fractional_bits, shading_table_size;

/* ---------- private prototypes */

static void update_color_environment(void);
static short find_or_add_color(struct rgb_color_value *color, struct rgb_color_value *colors, short *color_count);
static void build_shading_tables(struct rgb_color_value *colors, short count);
static pixel8 find_closest_match(struct rgb_color *match, struct rgb_color_value *colors, short count);
static void _change_clut(void (*change_clut_proc)(struct color_table *color_table), struct rgb_color_value *colors, short color_count);

static void build_shading_tables8(struct rgb_color_value *colors, short color_count, pixel8 *shading_tables);
static void build_shading_tables16(struct rgb_color_value *colors, short color_count, pixel16 *shading_tables, byte *remapping_table);
static void build_shading_tables32(struct rgb_color_value *colors, short color_count, pixel32 *shading_tables, byte *remapping_table);
static void build_global_shading_table16(void);
static void build_global_shading_table32(void);

static boolean get_next_color_run(struct rgb_color_value *colors, short color_count, short *start, short *count);
static boolean new_color_run(struct rgb_color_value *new_run, struct rgb_color_value *last);

static long get_shading_table_size(short collection_code);

static void build_collection_tinting_table(struct rgb_color_value *colors, short color_count, short collection_index);
static void build_tinting_table8(struct rgb_color_value *colors, short color_count, pixel8 *tint_table, short tint_start, short tint_count);
static void build_tinting_table16(struct rgb_color_value *colors, short color_count, pixel16 *tint_table, struct rgb_color *tint_color);
static void build_tinting_table32(struct rgb_color_value *colors, short color_count, pixel32 *tint_table, struct rgb_color *tint_color);

static void precalculate_bit_depth_constants(void);

static void byte_swap_collection(short collection_index);

static boolean collection_loaded(struct collection_header *header);
static void unload_collection(struct collection_header *header);
static void unlock_collection(struct collection_header *header);
static void lock_collection(struct collection_header *header);
static boolean load_collection(short collection_index, boolean strip);

static void debug_shapes_memory(void);

/* --------- collection accessor prototypes */

static struct collection_header *get_collection_header(short collection_index);
static struct collection_definition *get_collection_definition(short collection_index);
static struct collection_definition *_get_collection_definition(short collection_index);
static void *get_collection_shading_tables(short collection_index, short clut_index);
static void *get_collection_tint_tables(short collection_index, short tint_index);
static void *collection_offset(struct collection_definition *definition, long offset);
static struct rgb_color_value *get_collection_colors(short collection_index, short clut_number);
static struct high_level_shape_definition *get_high_level_shape_definition(short collection_index, short high_level_shape_index);
static struct low_level_shape_definition *get_low_level_shape_definition(short collection_index, short low_level_shape_index);
static struct bitmap_definition *get_bitmap_definition(short collection_index, short bitmap_index);

/* ---------- machine-specific code */

#ifdef mac
#include "shapes_macintosh.c"
#endif

/* ---------- code */

void unload_all_collections(
	void)
{
	struct collection_header *header;
	short collection_index;
	
	for (collection_index= 0, header= collection_headers; collection_index<MAXIMUM_COLLECTIONS; ++collection_index, ++header)
	{
		if (collection_loaded(header))
		{
			unload_collection(header);
		}
	}
	
	return;
}

void mark_collection(
	short collection_code,
	boolean loading)
{
	if (collection_code!=NONE)
	{
		short clut_index= GET_COLLECTION_CLUT(collection_code);
		short collection_index= GET_COLLECTION(collection_code);
	
		assert(collection_index>=0&&collection_index<MAXIMUM_COLLECTIONS);
		collection_headers[collection_index].status|= loading ? markLOAD : markUNLOAD;
	}
	
	return;
}

void strip_collection(
	short collection_code)
{
	if (collection_code!=NONE)
	{
		short collection_index= GET_COLLECTION(collection_code);
	
		assert(collection_index>=0&&collection_index<MAXIMUM_COLLECTIONS);
		collection_headers[collection_index].status|= markSTRIP;
	}
	
	return;
}

/* returns count, doesn�t fill NULL buffer */
short get_shape_descriptors(
	short shape_type,
	shape_descriptor *buffer)
{
	short collection_index, low_level_shape_index;
	short appropriate_type;
	short count;
	
	switch (shape_type)
	{
		case _wall_shape: appropriate_type= _wall_collection; break;
		case _floor_or_ceiling_shape: appropriate_type= _wall_collection; break;
		default: halt();
	}

	count= 0;
	for (collection_index=0;collection_index<MAXIMUM_COLLECTIONS;++collection_index)
	{
		struct collection_definition *collection= _get_collection_definition(collection_index);
		
		if (collection&&collection->type==appropriate_type)
		{
			for (low_level_shape_index=0;low_level_shape_index<collection->low_level_shape_count;++low_level_shape_index)
			{
				struct low_level_shape_definition *low_level_shape= get_low_level_shape_definition(collection_index, low_level_shape_index);
				struct bitmap_definition *bitmap= get_bitmap_definition(collection_index, low_level_shape->bitmap_index);
				
				count+= collection->clut_count;
				if (buffer)
				{
					short clut;
				
					for (clut=0;clut<collection->clut_count;++clut)
					{
						*buffer++= BUILD_DESCRIPTOR(BUILD_COLLECTION(collection_index, clut), low_level_shape_index);
					}
				}
			}
		}
	}
	
	return count;
}

void extended_get_shape_bitmap_and_shading_table(
	short collection_code,
	short low_level_shape_index,
	struct bitmap_definition **bitmap,
	void **shading_tables,
	short shading_mode)
{
//	if (collection_code==_collection_marathon_control_panels) collection_code= 30, low_level_shape_index= 0;
	short collection_index= GET_COLLECTION(collection_code);
	short clut_index= GET_COLLECTION_CLUT(collection_code);
	struct low_level_shape_definition *low_level_shape= get_low_level_shape_definition(collection_index, low_level_shape_index);
	
	if (bitmap) *bitmap= get_bitmap_definition(collection_index, low_level_shape->bitmap_index);
	if (shading_tables)
	{
		switch (shading_mode)
		{
			case _shading_normal:
				*shading_tables= get_collection_shading_tables(collection_index, clut_index);
				break;
			case _shading_infravision:
				*shading_tables= get_collection_tint_tables(collection_index, 0);
				break;
			
			default:
				halt();
		}
	}

	return;
}

struct shape_information_data *extended_get_shape_information(
	short collection_code,
	short low_level_shape_index)
{
	short collection_index= GET_COLLECTION(collection_code);
	struct low_level_shape_definition *low_level_shape;
	struct collection_definition *collection;

	collection= get_collection_definition(collection_index);
	low_level_shape= get_low_level_shape_definition(collection_index, low_level_shape_index);

	return (struct shape_information_data *) low_level_shape;
}

void process_collection_sounds(
	short collection_code,
	void (*process_sound)(short sound_index))
{
	short collection_index= GET_COLLECTION(collection_code);
	struct collection_definition *collection= get_collection_definition(collection_index);
	short high_level_shape_index;
	
	for (high_level_shape_index= 0; high_level_shape_index<collection->high_level_shape_count; ++high_level_shape_index)
	{
		struct high_level_shape_definition *high_level_shape= get_high_level_shape_definition(collection_index, high_level_shape_index);
		
		process_sound(high_level_shape->first_frame_sound);
		process_sound(high_level_shape->key_frame_sound);
		process_sound(high_level_shape->last_frame_sound);
	}
	
	return;
}

struct shape_animation_data *get_shape_animation_data(
	shape_descriptor shape)
{
	short collection_index, high_level_shape_index;
	struct high_level_shape_definition *high_level_shape;

	collection_index= GET_COLLECTION(GET_DESCRIPTOR_COLLECTION(shape));
	high_level_shape_index= GET_DESCRIPTOR_SHAPE(shape);
	high_level_shape= get_high_level_shape_definition(collection_index, high_level_shape_index);
	
	return (struct shape_animation_data *) &high_level_shape->number_of_views;
}

void *get_global_shading_table(
	void)
{
	void *shading_table= (void *) NULL;

	switch (bit_depth)
	{
		case 8:
		{
			/* return the last shading_table calculated */
			short collection_index;
		
			for (collection_index=MAXIMUM_COLLECTIONS-1;collection_index>=0;--collection_index)
			{
				struct collection_definition *collection= _get_collection_definition(collection_index);
				
				if (collection)
				{
					shading_table= get_collection_shading_tables(collection_index, 0);
					break;
				}
			}
			
			break;
		}
		
		case 16:
			build_global_shading_table16();
			shading_table= global_shading_table16;
			break;
		
		case 32:
			build_global_shading_table32();
			shading_table= global_shading_table32;
			break;
		
		default:
			halt();
	}
	assert(shading_table);
	
	return shading_table;
}

void load_collections(
	void)
{
	struct collection_header *header;
	short collection_index;

	precalculate_bit_depth_constants();
	
	free_and_unlock_memory(); /* do our best to get a big, unfragmented heap */
	
	/* first go through our list of shape collections and dispose of any collections which
		were marked for unloading.  at the same time, unlock all those collections which
		will be staying (so the heap can move around) */
	for (collection_index= 0, header= collection_headers; collection_index<MAXIMUM_COLLECTIONS; ++collection_index, ++header)
	{
		if ((header->status&markUNLOAD) && !(header->status&markLOAD))
		{
			if (collection_loaded(header))
			{
				unload_collection(header);
			}
		}
		else
		{
			/* if this collection is already loaded, unlock it to tenderize the heap */
			if (collection_loaded(header))
			{
				unlock_collection(header);
			}
		}
	}
	
	/* ... then go back through the list of collections and load any that we were asked to */
	for (collection_index= 0, header= collection_headers; collection_index<MAXIMUM_COLLECTIONS; ++collection_index, ++header)
	{
		/* don�t reload collections which are already in memory, but do lock them */
		if (collection_loaded(header))
		{
			lock_collection(header);
		}
		else
		{
			if (header->status&markLOAD)
			{
				/* load and decompress collection */
				if (!load_collection(collection_index, (header->status&markSTRIP) ? TRUE : FALSE))
				{
					alert_user(fatalError, strERRORS, outOfMemory, -1);
				}
			}
		}
		
		/* clear action flags */
		header->status= markNONE;
		header->flags= 0;
	}
	
	/* remap the shapes, recalculate row base addresses, build our new world color table and
		(finally) update the screen to reflect our changes */
	update_color_environment();

#ifdef DEBUG
	debug_shapes_memory();
#endif

	return;
}

/* ---------- private code */

static void precalculate_bit_depth_constants(
	void)
{
	switch (bit_depth)
	{
		case 8:
			number_of_shading_tables= 32;
			shading_table_fractional_bits= 5;
//			next_shading_table_shift= 8;
			shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel8);
			break;
		case 16:
			number_of_shading_tables= 64;
			shading_table_fractional_bits= 6;
//			next_shading_table_shift= 9;
			shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel16);
			break;
		case 32:
			number_of_shading_tables= 256;
			shading_table_fractional_bits= 8;
//			next_shading_table_shift= 10;
			shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel32);
			break;
	}
	
	return;
}

/* given a list of RGBColors, find out which one, if any, match the given color.  if there
	aren�t any matches, add a new entry and return that index. */
static short find_or_add_color(
	struct rgb_color_value *color,
	register struct rgb_color_value *colors,
	short *color_count)
{
	short i;
	
	// = 1 to skip the transparent color
	for (i= 1, colors+= 1; i<*color_count; ++i, ++colors)
	{
		if (colors->red==color->red && colors->green==color->green && colors->blue==color->blue)
		{
			colors->flags= color->flags;
			return i;
		}
	}

	assert(*color_count<PIXEL8_MAXIMUM_COLORS);
	*colors= *color;
	
	return (*color_count)++;
}

static void update_color_environment(
	void)
{
	short color_count;
	short collection_index;
	short bitmap_index;
	
	pixel8 remapping_table[PIXEL8_MAXIMUM_COLORS];
	struct rgb_color_value colors[PIXEL8_MAXIMUM_COLORS];

	memset(remapping_table, 0, PIXEL8_MAXIMUM_COLORS*sizeof(pixel8));

	// dummy color to hold the first index (zero) for transparent pixels
	colors[0].red= colors[0].green= colors[0].blue= 65535;
	colors[0].flags= colors[0].value= 0;
	color_count= 1;

	/* loop through all collections, only paying attention to the loaded ones.  we�re
		depending on finding the gray run (white to black) first; so it�s the responsibility
		of the lowest numbered loaded collection to give us this */
	for (collection_index=0;collection_index<MAXIMUM_COLLECTIONS;++collection_index)
	{
		struct collection_definition *collection= _get_collection_definition(collection_index);

//		dprintf("collection #%d", collection_index);
		
		if (collection && collection->bitmap_count)
		{
			struct rgb_color_value *primary_colors= get_collection_colors(collection_index, 0)+NUMBER_OF_PRIVATE_COLORS;
			short color_index, clut_index;

//			if (collection_index==15) dprintf("primary clut %p", primary_colors);
//			dprintf("primary clut %d entries;dm #%d #%d", collection->color_count, primary_colors, collection->color_count*sizeof(ColorSpec));

			/* add the colors from this collection�s primary color table to the aggregate color
				table and build the remapping table */
			for (color_index=0;color_index<collection->color_count-NUMBER_OF_PRIVATE_COLORS;++color_index)
			{
				primary_colors[color_index].value= remapping_table[primary_colors[color_index].value]= 
					find_or_add_color(&primary_colors[color_index], colors, &color_count);
			}
			
			/* then remap the collection and recalculate the base addresses of each bitmap */
			for (bitmap_index= 0; bitmap_index<collection->bitmap_count; ++bitmap_index)
			{
				struct bitmap_definition *bitmap= get_bitmap_definition(collection_index, bitmap_index);
				
				/* calculate row base addresses ... */
				bitmap->row_addresses[0]= calculate_bitmap_origin(bitmap);
				precalculate_bitmap_row_addresses(bitmap);

				/* remap it ... */
				remap_bitmap(bitmap, remapping_table);
			}
			
			/* build a shading table for each clut in this collection */
			for (clut_index= 0; clut_index<collection->clut_count; ++clut_index)
			{
				void *primary_shading_table= get_collection_shading_tables(collection_index, 0);
				short collection_bit_depth= collection->type==_interface_collection ? 8 : bit_depth;

				if (clut_index)
				{
					struct rgb_color_value *alternate_colors= get_collection_colors(collection_index, clut_index)+NUMBER_OF_PRIVATE_COLORS;
					void *alternate_shading_table= get_collection_shading_tables(collection_index, clut_index);
					pixel8 shading_remapping_table[PIXEL8_MAXIMUM_COLORS];
					
					memset(shading_remapping_table, 0, PIXEL8_MAXIMUM_COLORS*sizeof(pixel8));
					
//					dprintf("alternate clut %d entries;dm #%d #%d", collection->color_count, alternate_colors, collection->color_count*sizeof(ColorSpec));
					
					/* build a remapping table for the primary shading table which we can use to
						calculate this alternate shading table */
					for (color_index= 0; color_index<PIXEL8_MAXIMUM_COLORS; ++color_index) shading_remapping_table[color_index]= color_index;
					for (color_index= 0; color_index<collection->color_count-NUMBER_OF_PRIVATE_COLORS; ++color_index)
					{
						shading_remapping_table[find_or_add_color(&primary_colors[color_index], colors, &color_count)]= 
							find_or_add_color(&alternate_colors[color_index], colors, &color_count);
					}
//					shading_remapping_table[iBLACK]= iBLACK; /* make iBLACK==>iBLACK remapping explicit */

					switch (collection_bit_depth)
					{
						case 8:
							/* duplicate the primary shading table and remap it */
							memcpy(alternate_shading_table, primary_shading_table, get_shading_table_size(collection_index));
							map_bytes((unsigned char *)alternate_shading_table, shading_remapping_table, get_shading_table_size(collection_index));
							break;
						
						case 16:
							build_shading_tables16(colors, color_count, (unsigned short *)alternate_shading_table, shading_remapping_table); break;
							break;
						
						case 32:
							build_shading_tables32(colors, color_count, (unsigned long *)alternate_shading_table, shading_remapping_table); break;
							break;
						
						default:
							halt();
					}
				}
				else
				{
					/* build the primary shading table */
					switch (collection_bit_depth)
					{
						case 8: build_shading_tables8(colors, color_count, (unsigned char *)primary_shading_table); break;
						case 16: build_shading_tables16(colors, color_count, (unsigned short *)primary_shading_table, (byte *) NULL); break;
						case 32: build_shading_tables32(colors, color_count, (unsigned long *)primary_shading_table, (byte *) NULL); break;
						default: halt();
					}
				}
			}
			
			build_collection_tinting_table(colors, color_count, collection_index);
			
			/* 8-bit interface, non-8-bit main window; remember interface CLUT separately */
			if (collection_index==_collection_interface && interface_bit_depth==8 && bit_depth!=interface_bit_depth) _change_clut(change_interface_clut, colors, color_count);
			
			/* if we�re not in 8-bit, we don�t have to carry our colors over into the next collection */
			if (bit_depth!=8) color_count= 1;
		}
	}

#ifdef DEBUG
//	dump_colors(colors, color_count);
#endif

	/* change the screen clut and rebuild our shading tables */
	_change_clut(change_screen_clut, colors, color_count);

	return;
}

static void _change_clut(
	void (*change_clut_proc)(struct color_table *color_table),
	struct rgb_color_value *colors,
	short color_count)
{
	struct color_table color_table;
	struct rgb_color *color;
	short i;
	
	color= color_table.colors;
	color_table.color_count= PIXEL8_MAXIMUM_COLORS;
	for (i= 0; i<color_count; ++i, ++color, ++colors)
	{
		*color= *((struct rgb_color *)&colors->red);
	}
	for (i= color_count; i<PIXEL8_MAXIMUM_COLORS; ++i, ++color)
	{
		color->red= color->green= color->blue= 0;
	}
	change_clut_proc(&color_table);
	
	return;
}

#ifndef SCREAMING_METAL
static void build_shading_tables8(
	struct rgb_color_value *colors,
	short color_count,
	pixel8 *shading_tables)
{
	short i;
	short start, count, level, value;
	
	memset(shading_tables, iBLACK, PIXEL8_MAXIMUM_COLORS*sizeof(pixel8));
	
	start= 0, count= 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		for (i= 0; i<count; ++i)
		{
			short adjust= start ? 1 : 0;

			for (level= 0; level<number_of_shading_tables; ++level)
			{
				struct rgb_color_value *color= colors + start + i;
				short multiplier= (color->flags&SELF_LUMINESCENT_COLOR_FLAG) ? (level>>1) : level;

				value= i + (multiplier*(count+adjust-i))/(number_of_shading_tables-1);
				if (value>=count) value= iBLACK; else value= start+value;
				shading_tables[PIXEL8_MAXIMUM_COLORS*(number_of_shading_tables-level-1)+start+i]= value;
			}
		}
	}

	return;
}
#else
static short find_closest_color(struct rgb_color *color, register struct rgb_color_value *colors,
	short color_count);

static short find_closest_color(
	struct rgb_color *color,
	register struct rgb_color_value *colors,
	short color_count)
{
	short i;
	long closest_delta= LONG_MAX;
	short closest_index= 0;
	
	// = 1 to skip the transparent color
	for (i= 1, colors+= 1; i<color_count; ++i, ++colors)
	{
		long delta= (long)ABS(colors->red-color->red) +
			(long)ABS(colors->green-color->green) +
			(long)ABS(colors->blue-color->blue);
		
		if (delta<closest_delta) closest_index= i, closest_delta= delta;
	}

	return closest_index;
}

static void build_shading_tables8(
	struct rgb_color_value *colors,
	short color_count,
	pixel8 *shading_tables)
{
	short i;
	short start, count, level;
	
	memset(shading_tables, iBLACK, PIXEL8_MAXIMUM_COLORS*sizeof(pixel8));
	
	start= 0, count= 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		for (i= 0; i<count; ++i)
		{
			for (level= 0; level<number_of_shading_tables; ++level)
			{
				struct rgb_color_value *color= colors + start + i;
				struct rgb_color result;
				
				result.red= (color->red*level)/(number_of_shading_tables-1);
				result.green= (color->green*level)/(number_of_shading_tables-1);
				result.blue= (color->blue*level)/(number_of_shading_tables-1);
				shading_tables[PIXEL8_MAXIMUM_COLORS*level+start+i]=
					find_closest_color(&result, colors, color_count);
			}
		}
	}

	return;
}
#endif

static void build_shading_tables16(
	struct rgb_color_value *colors,
	short color_count,
	pixel16 *shading_tables,
	byte *remapping_table)
{
	short i;
	short start, count, level;
	
	memset(shading_tables, 0, PIXEL8_MAXIMUM_COLORS*sizeof(pixel16));
	
	start= 0, count= 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		for (i=0;i<count;++i)
		{
			for (level= 0; level<number_of_shading_tables; ++level)
			{
				struct rgb_color_value *color= colors + (remapping_table ? remapping_table[start+i] : (start+i));
				short multiplier= (color->flags&SELF_LUMINESCENT_COLOR_FLAG) ? ((number_of_shading_tables>>1)+(level>>1)) : level;
				
				shading_tables[PIXEL8_MAXIMUM_COLORS*level+start+i]= 
					RGBCOLOR_TO_PIXEL16((color->red*multiplier)/(number_of_shading_tables-1),
						(color->green*multiplier)/(number_of_shading_tables-1),
						(color->blue*multiplier)/(number_of_shading_tables-1));
			}
		}
	}

	return;
}

static void build_shading_tables32(
	struct rgb_color_value *colors,
	short color_count,
	pixel32 *shading_tables,
	byte *remapping_table)
{
	short i;
	short start, count, level;
	
	memset(shading_tables, 0, PIXEL8_MAXIMUM_COLORS*sizeof(pixel32));
	
	start= 0, count= 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		for (i= 0; i<count; ++i)
		{
			for (level= 0; level<number_of_shading_tables; ++level)
			{
				struct rgb_color_value *color= colors + (remapping_table ? remapping_table[start+i] : (start+i));
				short multiplier= (color->flags&SELF_LUMINESCENT_COLOR_FLAG) ? ((number_of_shading_tables>>1)+(level>>1)) : level;
				
				shading_tables[PIXEL8_MAXIMUM_COLORS*level+start+i]= 
					RGBCOLOR_TO_PIXEL32((color->red*multiplier)/(number_of_shading_tables-1),
						(color->green*multiplier)/(number_of_shading_tables-1),
						(color->blue*multiplier)/(number_of_shading_tables-1));
			}
		}
	}

	return;
}

static void build_global_shading_table16(
	void)
{
	if (!global_shading_table16)
	{
		short component, value, shading_table;
		pixel16 *write;
		
		global_shading_table16= (pixel16 *) malloc(sizeof(pixel16)*number_of_shading_tables*NUMBER_OF_COLOR_COMPONENTS*(PIXEL16_MAXIMUM_COMPONENT+1));
		assert(global_shading_table16);
		
		write= global_shading_table16;
		for (shading_table= 0; shading_table<number_of_shading_tables; ++shading_table)
		{
			for (component=0;component<NUMBER_OF_COLOR_COMPONENTS;++component)
			{
				short shift= 5*(NUMBER_OF_COLOR_COMPONENTS-component-1);

				for (value=0;value<=PIXEL16_MAXIMUM_COMPONENT;++value)
				{
					*write++= ((value*(shading_table))/(number_of_shading_tables-1))<<shift;
				}
			}
		}
	}
	
	return;
}

static void build_global_shading_table32(
	void)
{
	if (!global_shading_table32)
	{
		short component, value, shading_table;
		pixel32 *write;
		
		global_shading_table32= (pixel32 *) malloc(sizeof(pixel32)*number_of_shading_tables*NUMBER_OF_COLOR_COMPONENTS*(PIXEL32_MAXIMUM_COMPONENT+1));
		assert(global_shading_table32);
		
		write= global_shading_table32;
		for (shading_table= 0; shading_table<number_of_shading_tables; ++shading_table)
		{
			for (component= 0; component<NUMBER_OF_COLOR_COMPONENTS; ++component)
			{
				short shift= 8*(NUMBER_OF_COLOR_COMPONENTS-component-1);

				for (value= 0; value<=PIXEL32_MAXIMUM_COMPONENT; ++value)
				{
					*write++= ((value*(shading_table))/(number_of_shading_tables-1))<<shift;
				}
			}
		}
	}
	
	return;
}

static boolean get_next_color_run(
	struct rgb_color_value *colors,
	short color_count,
	short *start,
	short *count)
{
	boolean not_done= FALSE;
	struct rgb_color_value last_color;
	
	if (*start+*count<color_count)
	{
		*start+= *count;
		for (*count=0;*start+*count<color_count;*count+= 1)
		{
			if (*count)
			{
				if (new_color_run(colors+*start+*count, &last_color))
				{
					break;
				}
			}
			last_color= colors[*start+*count];
		}
		
		not_done= TRUE;
	}
	
	return not_done;
}

static boolean new_color_run(
	struct rgb_color_value *new_run,
	struct rgb_color_value *last)
{
	if ((long)last->red+(long)last->green+(long)last->blue<(long)new_run->red+(long)new_run->green+(long)new_run->blue)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static long get_shading_table_size(
	short collection_code)
{
	short collection_index= GET_COLLECTION(collection_code);
	long size;
	
	switch (bit_depth)
	{
		case 8: size= number_of_shading_tables*shading_table_size; break;
		case 16: size= number_of_shading_tables*shading_table_size; break;
		case 32: size= number_of_shading_tables*shading_table_size; break;
		default: halt();
	}
	
	return size;
}

/* --------- light enhancement goggles */

enum /* collection tint colors */
{
	_tint_collection_red,
	_tint_collection_green,
	_tint_collection_blue,
	_tint_collection_yellow,
	NUMBER_OF_TINT_COLORS
};

struct tint_color8_data
{
	short start, count;
};

static struct rgb_color tint_colors16[NUMBER_OF_TINT_COLORS]=
{
	{65535, 0, 0},
	{0, 65535, 0},
	{0, 0, 65535},
	{65535, 65535, 0},
};

static struct tint_color8_data tint_colors8[NUMBER_OF_TINT_COLORS]=
{
	{45, 13},
	{32, 13},
	{96, 13},
	{83, 13},
};

static void build_collection_tinting_table(
	struct rgb_color_value *colors,
	short color_count,
	short collection_index)
{
	struct collection_definition *collection= get_collection_definition(collection_index);
	void *tint_table= get_collection_tint_tables(collection_index, 0);
	short tint_color;

	/* get the tint color */
	tint_color= NONE;	
	switch (collection->type)
	{
		case _wall_collection: tint_color= _tint_collection_blue; break;
		case _object_collection: tint_color= _tint_collection_red; break;
		case _scenery_collection: tint_color= _tint_collection_green; break;
	}
	switch (collection_index)
	{
		case _collection_weapons_in_hand:
		case _collection_player:
		case _collection_rocket:
		case _collection_civilian:
		case _collection_vacuum_civilian:
			tint_color= _tint_collection_yellow;
			break;
		
		case _collection_items:
			tint_color= _tint_collection_green;
			break;
		
		case _collection_compiler:
		case _collection_scenery1:
		case _collection_scenery2:
		case _collection_scenery3:
		case _collection_scenery4:
		case _collection_scenery5:
			tint_color= _tint_collection_blue;
			break;
	}

	/* build the tint table */	
	if (tint_color!=NONE)
	{
		switch (bit_depth)
		{
			case 8:
				build_tinting_table8(colors, color_count, (unsigned char *)tint_table, tint_colors8[tint_color].start, tint_colors8[tint_color].count);
				break;
			case 16:
				build_tinting_table16(colors, color_count, (unsigned short *)tint_table, tint_colors16+tint_color);
				break;
			case 32:
				build_tinting_table32(colors, color_count, (unsigned long *)tint_table, tint_colors16+tint_color);
				break;
		}
	}
	
	return;
}

static void build_tinting_table8(
	struct rgb_color_value *colors,
	short color_count,
	pixel8 *tint_table,
	short tint_start,
	short tint_count)
{
	short start, count;
	
	start= count= 0;
	while (get_next_color_run(colors, color_count, &start, &count))
	{
		short i;

		for (i=0; i<count; ++i)
		{
			short adjust= start ? 0 : 1;
			short value= (i*(tint_count+adjust))/count;
			
			value= (value>=tint_count) ? iBLACK : tint_start + value;
			tint_table[start+i]= value;
		}
	}
	
	return;
}

static void build_tinting_table16(
	struct rgb_color_value *colors,
	short color_count,
	pixel16 *tint_table,
	struct rgb_color *tint_color)
{
	short i;

	for (i= 0; i<color_count; ++i, ++colors)
	{
		long magnitude= ((long)colors->red + (long)colors->green + (long)colors->blue)/(short)3;
		
		*tint_table++= RGBCOLOR_TO_PIXEL16((magnitude*tint_color->red)/65535,
			(magnitude*tint_color->green)/65535, (magnitude*tint_color->blue)/65535);
	}

	return;
}

static void build_tinting_table32(
	struct rgb_color_value *colors,
	short color_count,
	pixel32 *tint_table,
	struct rgb_color *tint_color)
{
	short i;

	for (i= 0; i<color_count; ++i, ++colors)
	{
		long magnitude= ((long)colors->red + (long)colors->green + (long)colors->blue)/(short)3;
		
		*tint_table++= RGBCOLOR_TO_PIXEL32((magnitude*tint_color->red)/65535,
			(magnitude*tint_color->green)/65535, (magnitude*tint_color->blue)/65535);
	}

	return;
}

static void byte_swap_collection(
	short collection_index)
{
	struct collection_definition *definition= get_collection_definition(collection_index);
	short i;

	// collection_definition	
	byte_swap_data(definition, sizeof(struct collection_definition), 1, &_bs_collection_definition);
	
	// rgb_color_value
	byte_swap_data(collection_offset(definition, definition->color_table_offset), sizeof(struct rgb_color_value),
		definition->color_count*definition->clut_count, &_bs_rgb_color_value);
	
	byte_swap_memory(collection_offset(definition, definition->high_level_shape_offset_table_offset),
		_4byte, definition->high_level_shape_count);
	for (i= 0; i<definition->high_level_shape_count; ++i)
	{
		struct high_level_shape_definition *high= get_high_level_shape_definition(collection_index, i);
		
		byte_swap_data(high, sizeof(struct high_level_shape_definition), 1, &_bs_high_level_shape_definition);
		byte_swap_memory(&high->low_level_shape_indexes, _2byte, high->number_of_views*high->frames_per_view);
	}
	
	byte_swap_memory(collection_offset(definition, definition->low_level_shape_offset_table_offset),
		_4byte, definition->low_level_shape_count);
	for (i= 0; i<definition->low_level_shape_count; ++i)
	{
		struct low_level_shape_definition *low= get_low_level_shape_definition(collection_index, i);
		
		byte_swap_data(low, sizeof(struct low_level_shape_definition), 1, &_bs_low_level_shape_definition);
	}

	byte_swap_memory(collection_offset(definition, definition->bitmap_offset_table_offset),
		_4byte, definition->bitmap_count);
	for (i= 0; i<definition->bitmap_count; ++i)
	{
		struct bitmap_definition *bitmap= get_bitmap_definition(collection_index, i);
		
		byte_swap_data(bitmap, sizeof(struct bitmap_definition), 1, &_bs_bitmap_definition);
	}
	
	return;
}

/* ---------- collection accessors */

static struct collection_header *get_collection_header(
	short collection_index)
{
	assert(collection_index>=0 && collection_index<MAXIMUM_COLLECTIONS);
	
	return collection_headers + collection_index;
}

static void *collection_offset(
	struct collection_definition *definition,
	long offset)
{
	vassert(offset>=0 && offset<definition->size,
		csprintf(temporary, "asked for offset #%d/#%d", offset, definition->size));

	return ((byte *)definition) + offset;
}

static struct rgb_color_value *get_collection_colors(
	short collection_index,
	short clut_number)
{
	struct collection_definition *definition= get_collection_definition(collection_index);

	assert(clut_number>=0&&clut_number<definition->clut_count);
	
	return (struct rgb_color_value *) collection_offset(definition, definition->color_table_offset+clut_number*sizeof(struct rgb_color_value)*definition->color_count);
}

static struct high_level_shape_definition *get_high_level_shape_definition(
	short collection_index,
	short high_level_shape_index)
{
	struct collection_definition *definition= get_collection_definition(collection_index);
	long *offset_table;
	
	vassert(high_level_shape_index>=0&&high_level_shape_index<definition->high_level_shape_count,
		csprintf(temporary, "asked for high-level shape %d/%d, collection %d", high_level_shape_index, definition->high_level_shape_count, collection_index));
	
	offset_table= (long *) collection_offset(definition, definition->high_level_shape_offset_table_offset);
	return (struct high_level_shape_definition *) collection_offset(definition, offset_table[high_level_shape_index]);
}

static struct low_level_shape_definition *get_low_level_shape_definition(
	short collection_index,
	short low_level_shape_index)
{
	struct collection_definition *definition= get_collection_definition(collection_index);
	long *offset_table;

	vassert(low_level_shape_index>=0 && low_level_shape_index<definition->low_level_shape_count,
		csprintf(temporary, "asked for low-level shape %d/%d, collection %d", low_level_shape_index, definition->low_level_shape_count, collection_index));
	
	offset_table= (long *) collection_offset(definition, definition->low_level_shape_offset_table_offset);
	return (struct low_level_shape_definition *) collection_offset(definition, offset_table[low_level_shape_index]);
}

static struct bitmap_definition *get_bitmap_definition(
	short collection_index,
	short bitmap_index)
{
	struct collection_definition *definition= get_collection_definition(collection_index);
	long *offset_table;
	
	vassert(bitmap_index>=0 && bitmap_index<definition->bitmap_count,
		csprintf(temporary, "asked for collection #%d bitmap #%d/#%d", collection_index, bitmap_index, definition->bitmap_count));
	
	offset_table= (long *) collection_offset(definition, definition->bitmap_offset_table_offset);
	return (struct bitmap_definition *) collection_offset(definition, offset_table[bitmap_index]);
}

static void debug_shapes_memory(
	void)
{
	short collection_index;
	struct collection_header *header;
	
	long total_size= 0;

	for (collection_index= 0, header= collection_headers; collection_index<MAXIMUM_COLLECTIONS; ++collection_index, ++header)
	{
		if (collection_loaded(header))
		{
			struct collection_definition *definition= get_collection_definition(collection_index);
			
//			dprintf("collection #% 2d @ #% 9d bytes", collection_index, definition->size);
			total_size+= definition->size;
		}
	}
	
//	dprintf("                  #% 9d bytes total", total_size);

//	dprintf("shapes for this level take #%d bytes;g;", total_size);
	
	return;
}

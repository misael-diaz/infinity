/*
SCOTTISH_TEXTURES.C
Wednesday, April 20, 1994 9:35:36 AM

this is not your father�s texture mapping library.
(in fact it isn�t yours either, dillweed)

Wednesday, April 20, 1994 3:39:21 PM
	vertical repeats would be difficult because it would require testing repeats in the
	innermost loop of the pixel mapper (a compare and branch we can do without).
Saturday, April 23, 1994 10:42:41 AM
	(on the plane to santa clara) finished the slower version of the trapezoid mapper (we
	need to handle stretching with a degenerate switch statement like marathon used to) but
	the whole sampling process is now mathematically correct except for the squared function
	we use to calculate the x texture position and the shading table (but this is accurate to
	within 1/64k and doesn't accumulate error so who cares).
Sunday, April 24, 1994 10:12:47 AM
	(waiting for the CGDC to start at 9:00 PST) added all polygon stuff.  it struck me this
	morning that clipping against the view cone must be deterministic (that is, line segments
	of polygons and line segments of walls must be clipped in the same manner) or our
	edges won't meet up.  ordered dither darkening will look really cool but will be slow in c.
Sunday, April 24, 1994 11:21:47 PM
	still need transparent trapezoids, dither darkening, faster DDA for trapezoid mapping.
Wednesday, April 27, 1994 9:49:55 AM
	i'm just looking for one divine hammer (to bang it all day).  solid polygons are currently
	unaffected by darkening.  i'm not entirely certain we'll even use them.
Sunday, May 8, 1994 8:32:11 AM
	LISP�s lexical contours kick C firmly and painfully in the ass. everything is fast now
	except the landscape mapper which has just been routed and is in full retreat.
Friday, May 13, 1994 10:05:08 AM
	low-level unification of trapezoids and rectangles, transparent runs in shapes are run-length
	encoded now.  maintaining run tables was slower than generating d, delta_d and delta_d_prime
	and using them on the fly.
Wednesday, May 18, 1994 2:16:26 PM
	scope matters (at WWDC).
Sunday, May 22, 1994 12:32:02 PM
	drawing things in column order to cached (i.e., non-screen) memory is like crapping in the
	data cache, right?  maybe drawing rectangles in column-order wasn't such a great idea after all.
	it also occurs to me that i know nothing about how to order instructions for the �040 pipelines.
Thursday, June 16, 1994 9:56:14 PM
	modified _render_textured_polygon_line to handle elevation.
Thursday, July 7, 1994 1:23:09 PM
	changed MAXIMUM_SCRATCH_TABLE_ENTRIES from 4k to 1200.  Modified render code to work as well,
	now the problem is floor/ceiling matching with trapezoids, which should fall out with the 
	rewrite...
Tuesday, July 26, 1994 3:42:16 PM
	OBSOLETE�ed nearly the entire file (fixed_pixels are no more).  rewriting texture_rectangle.
	will do 16bit mapping, soon.  a while ago i rewrote everything in 68k.
Friday, September 16, 1994 6:03:11 PM  (Jason')
	texture_rectangle() now respects top and bottom clips
Tuesday, September 20, 1994 9:58:30 PM  (Jason')
	if we�re so close to a rectangle that n>LARGEST_N then we don�t draw anything
Wednesday, October 26, 1994 3:18:59 PM (Jason)
	for non-convex or otherwise weird lines (dx<=0, dy<=0) we don�t draw anything (somebody�ll
	notice that for sure).
Friday, November 4, 1994 7:35:48 PM  (Jason')
	pretexture_horizontal_polygon_lines() now respects the (x,y) polygon origin and uses z as height.
*/

/*
rectangle shrinking has vertical error and appears to randomly shear the bitmap
pretexture_horizontal_polygon_lines() has integer error in large height cases

_static_transfer doesn�t work for ceilings and floors (because they call the wall mapper)
build_y_table and build_x_table could both be sped up in nearly-horizontal and nearly-vertical cases (respectively)
_pretexture_vertical_polygon_lines() takes up to half the time _texture_vertical_polygon_lines() does
not only that, but texture_horizontal_polygon() is actually faster than texture_vertical_polygon()

//calculate_shading_table() needs to be inlined in a macro
*/

#include "cseries.h"

#include "render.h"

#ifdef mpwc
#pragma segment texture
#endif

#ifdef env68k
#define EXTERNAL
#endif

/* ---------- constants */

#define MAXIMUM_SCRATCH_TABLE_ENTRIES 1024
#define MAXIMUM_PRECALCULATION_TABLE_ENTRY_SIZE 34

#define SHADE_TO_SHADING_TABLE_INDEX(shade) ((shade)>>(FIXED_FRACTIONAL_BITS-shading_table_fractional_bits))
#define DEPTH_TO_SHADE(d) (((fixed)(d))<<(FIXED_FRACTIONAL_BITS-WORLD_FRACTIONAL_BITS-3))

#define LARGEST_N 24

/* ---------- texture horizontal polygon */

#define HORIZONTAL_WIDTH_SHIFT 7 /* 128 (8 for 256) */
#define HORIZONTAL_HEIGHT_SHIFT 7 /* 128 */
#define HORIZONTAL_FREE_BITS (32-TRIG_SHIFT-WORLD_FRACTIONAL_BITS)
#define HORIZONTAL_WIDTH_DOWNSHIFT (32-HORIZONTAL_WIDTH_SHIFT)
#define HORIZONTAL_HEIGHT_DOWNSHIFT (32-HORIZONTAL_HEIGHT_SHIFT)

struct _horizontal_polygon_line_header
{
	long y_downshift;
};

struct _horizontal_polygon_line_data
{
	unsigned long source_x, source_y;
	unsigned long source_dx, source_dy;
	
	void *shading_table;
};

/* ---------- texture vertical polygon */

#define VERTICAL_TEXTURE_WIDTH 128
#define VERTICAL_TEXTURE_WIDTH_BITS 7
#define VERTICAL_TEXTURE_WIDTH_FRACTIONAL_BITS (FIXED_FRACTIONAL_BITS-VERTICAL_TEXTURE_WIDTH_BITS)
#define VERTICAL_TEXTURE_ONE (1<<VERTICAL_TEXTURE_WIDTH_FRACTIONAL_BITS)
#define VERTICAL_TEXTURE_FREE_BITS FIXED_FRACTIONAL_BITS
#define VERTICAL_TEXTURE_DOWNSHIFT (32-VERTICAL_TEXTURE_WIDTH_BITS)

#define HORIZONTAL_WIDTH_SHIFT 7 /* 128 (8 for 256) */
#define HORIZONTAL_HEIGHT_SHIFT 7 /* 128 */
#define HORIZONTAL_FREE_BITS (32-TRIG_SHIFT-WORLD_FRACTIONAL_BITS)
#define HORIZONTAL_WIDTH_DOWNSHIFT (32-HORIZONTAL_WIDTH_SHIFT)
#define HORIZONTAL_HEIGHT_DOWNSHIFT (32-HORIZONTAL_HEIGHT_SHIFT)

struct _vertical_polygon_data
{
	short downshift;
	short x0;
	short width;
	
	short pad;
};

struct _vertical_polygon_line_data
{
	void *shading_table;
	pixel8 *texture;
	long texture_y, texture_dy;
};

/* ---------- macros */

// i0 + i1 == MAX(i0, i1) + MIN(i0, i1)/2
#define CALCULATE_SHADING_TABLE(result, view, shading_tables, depth, ambient_shade) \
{ \
	short table_index; \
	fixed shade; \
	 \
	if ((ambient_shade)<0) \
	{ \
		table_index= SHADE_TO_SHADING_TABLE_INDEX(-(ambient_shade)); \
	} \
	else \
	{ \
		shade= (view)->maximum_depth_intensity - DEPTH_TO_SHADE(depth); \
		shade= PIN(shade, 0, FIXED_ONE); \
		table_index= SHADE_TO_SHADING_TABLE_INDEX((ambient_shade>shade) ? (ambient_shade + (shade>>1)) : (shade + (ambient_shade>>1))); \
	} \
	 \
	switch (bit_depth) \
	{ \
		case 8: result= ((byte*)(shading_tables)) + MAXIMUM_SHADING_TABLE_INDEXES*sizeof(pixel8)* \
			CEILING(table_index, number_of_shading_tables-1); break; \
		case 16: result= ((byte*)(shading_tables)) + MAXIMUM_SHADING_TABLE_INDEXES*sizeof(pixel16)* \
			CEILING(table_index, number_of_shading_tables-1); break; \
		case 32: result= ((byte*)(shading_tables)) + MAXIMUM_SHADING_TABLE_INDEXES*sizeof(pixel32)* \
			CEILING(table_index, number_of_shading_tables-1); break; \
	} \
}

//		table_index= SHADE_TO_SHADING_TABLE_INDEX((view)->maximum_depth_intensity - DEPTH_TO_SHADE(depth)); \
//		table_index= PIN(table_index, 0, number_of_shading_tables); \
//		table_index= MAX(SHADE_TO_SHADING_TABLE_INDEX(ambient_shade), table_index); \

/* ---------- globals */

/* these tables are used by the polygon rasterizer (to store the x-coordinates of the left and
	right lines of the current polygon), the trapezoid rasterizer (to store the y-coordinates
	of the top and bottom of the current trapezoid) and the rectangle mapper (for it�s
	vertical and if necessary horizontal distortion tables).  these are not necessary as
	globals, just as global storage. */
static short *scratch_table0, *scratch_table1;
static void *precalculation_table;

static word texture_random_seed= 1;

/* ---------- private prototypes */

static void _pretexture_horizontal_polygon_lines(struct polygon_definition *polygon,
	struct bitmap_definition *screen, struct view_data *view, struct _horizontal_polygon_line_data *data,
	short y0, short *x0_table, short *x1_table, short line_count);

void _texture_horizontal_polygon_lines8(struct bitmap_definition *texture,
	struct bitmap_definition *screen, struct view_data *view, struct _horizontal_polygon_line_data *data,
	short y0, short *x0_table, short *x1_table, short line_count);
//void _fill_horizontal_polygon_lines8(struct bitmap_definition *texture,
//	struct bitmap_definition *screen, struct view_data *view, struct _horizontal_polygon_line_data *data,
//	short y0, short *x0_table, short *x1_table, short line_count);

void _texture_horizontal_polygon_lines16(struct bitmap_definition *texture,
	struct bitmap_definition *screen, struct view_data *view, struct _horizontal_polygon_line_data *data,
	short y0, short *x0_table, short *x1_table, short line_count);

void _texture_horizontal_polygon_lines32(struct bitmap_definition *texture,
	struct bitmap_definition *screen, struct view_data *view, struct _horizontal_polygon_line_data *data,
	short y0, short *x0_table, short *x1_table, short line_count);

static void _pretexture_vertical_polygon_lines(struct polygon_definition *polygon,
	struct bitmap_definition *screen, struct view_data *view, struct _vertical_polygon_data *data,
	short x0, short *y0_table, short *y1_table, short line_count);

void _texture_vertical_polygon_lines8(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table);
void _transparent_texture_vertical_polygon_lines8(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table);
void _tint_vertical_polygon_lines8(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table, word transfer_data);
void _randomize_vertical_polygon_lines8(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table, word transfer_data);

void _texture_vertical_polygon_lines16(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table);
void _transparent_texture_vertical_polygon_lines16(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table);
void _tint_vertical_polygon_lines16(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table, word transfer_data);
void _randomize_vertical_polygon_lines16(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table, word transfer_data);

void _texture_vertical_polygon_lines32(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table);
void _transparent_texture_vertical_polygon_lines32(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table);
void _tint_vertical_polygon_lines32(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table, word transfer_data);
void _randomize_vertical_polygon_lines32(struct bitmap_definition *screen, struct view_data *view,
	struct _vertical_polygon_data *data, short *y0_table, short *y1_table, word transfer_data);

static short *build_x_table(short *table, short x0, short y0, short x1, short y1);
static short *build_y_table(short *table, short x0, short y0, short x1, short y1);

static void preprocess_landscaped_polygon(struct polygon_definition *polygon, struct view_data *view);

static void _prelandscape_horizontal_polygon_lines(struct polygon_definition *polygon,
	struct bitmap_definition *screen, struct view_data *view, struct _horizontal_polygon_line_data *data,
	short y0, short *x0_table, short *x1_table, short line_count);
void _landscape_horizontal_polygon_lines8(struct bitmap_definition *texture, struct bitmap_definition *screen,
	struct view_data *view, struct _horizontal_polygon_line_data *data, short y0,
	short *x0_table, short *x1_table, short line_count);
void _landscape_horizontal_polygon_lines16(struct bitmap_definition *texture, struct bitmap_definition *screen,
	struct view_data *view, struct _horizontal_polygon_line_data *data, short y0,
	short *x0_table, short *x1_table, short line_count);
void _landscape_horizontal_polygon_lines32(struct bitmap_definition *texture, struct bitmap_definition *screen,
	struct view_data *view, struct _horizontal_polygon_line_data *data, short y0,
	short *x0_table, short *x1_table, short line_count);

/* ---------- code */

/* set aside memory at launch for two line tables (remember, we precalculate all the y-values
	for trapezoids and two lines worth of x-values for polygons before mapping them) */
void allocate_texture_tables(
	void)
{
	scratch_table0= (short *)malloc(sizeof(short)*MAXIMUM_SCRATCH_TABLE_ENTRIES);
	scratch_table1= (short *)malloc(sizeof(short)*MAXIMUM_SCRATCH_TABLE_ENTRIES);
	precalculation_table= malloc(MAXIMUM_PRECALCULATION_TABLE_ENTRY_SIZE*MAXIMUM_SCRATCH_TABLE_ENTRIES);
	assert(scratch_table0&&scratch_table1&&precalculation_table);

	return;
}

void texture_horizontal_polygon(
	struct polygon_definition *polygon,
	struct bitmap_definition *screen,
	struct view_data *view)
{
	short vertex, highest_vertex, lowest_vertex;
	point2d *vertices= (point2d *) &polygon->vertices;

	assert(polygon->vertex_count>=MINIMUM_VERTICES_PER_SCREEN_POLYGON&&polygon->vertex_count<MAXIMUM_VERTICES_PER_SCREEN_POLYGON);

	/* if we get static, tinted or landscaped transfer modes punt to the vertical polygon mapper */
	switch (polygon->transfer_mode)
	{
//		case _solid_transfer:
//			if (bit_depth!=8) polygon->transfer_mode= _textured_transfer;
//			break;
		case _static_transfer:
//		case _landscaped_transfer:
			texture_vertical_polygon(polygon, screen, view);
			return;
	}
	
	/* locate the vertically highest (closest to zero) and lowest (farthest from zero) vertices */
	highest_vertex= lowest_vertex= 0;
	for (vertex= 1; vertex<polygon->vertex_count; ++vertex)
	{
		if (vertices[vertex].y<vertices[highest_vertex].y) highest_vertex= vertex;
		if (vertices[vertex].y>vertices[lowest_vertex].y) lowest_vertex= vertex;
	}

	for (vertex=0;vertex<polygon->vertex_count;++vertex)
	{
		if (!(vertices[vertex].x>=0&&vertices[vertex].x<=screen->width&&vertices[vertex].y>=0&&vertices[vertex].y<=screen->height))
		{
//			dprintf("vertex #%d/#%d out of bounds:;dm %x %x;g;", vertex, polygon->vertex_count, polygon->vertices, polygon->vertex_count*sizeof(point2d));
			return;
		}
	}

	/* if this polygon is not a horizontal line, draw it */
	if (highest_vertex!=lowest_vertex)
	{
		short left_line_count, right_line_count, total_line_count;
		short aggregate_left_line_count, aggregate_right_line_count, aggregate_total_line_count;
		short left_vertex, right_vertex;
		short *left_table= scratch_table0, *right_table= scratch_table1;

		left_line_count= right_line_count= 0; /* zero counts so the left and right lines get initialized */
		aggregate_left_line_count= aggregate_right_line_count= 0; /* we�ve precalculated nothing initially */
		left_vertex= right_vertex= highest_vertex; /* both sides start at the highest vertex */
		total_line_count= vertices[lowest_vertex].y-vertices[highest_vertex].y; /* calculate vertical line count */

		assert(total_line_count<MAXIMUM_SCRATCH_TABLE_ENTRIES); /* make sure we have enough scratch space */
		
		/* precalculate high and low y-coordinates for every x-coordinate */			
		aggregate_total_line_count= total_line_count;
		while (total_line_count>0)
		{
			short delta;
			
			/* if we�re out of scan lines on the left side, get a new vertex and build a table
				of x-coordinates so we can walk toward the new vertex */
			if (left_line_count<=0)
			{
				do /* counter-clockwise vertex search */
				{
					vertex= left_vertex ? (left_vertex-1) : (polygon->vertex_count-1);
					left_line_count= vertices[vertex].y-vertices[left_vertex].y;
					if (!build_x_table(left_table+aggregate_left_line_count, vertices[left_vertex].x, vertices[left_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_left_line_count+= left_line_count;
					left_vertex= vertex;
//					dprintf("add %d left", left_line_count);
				}
				while (!left_line_count);
			}

			/* if we�re out of scan lines on the right side, get a new vertex and build a table
				of x-coordinates so we can walk toward the new vertex */
			if (right_line_count<=0)
			{
				do /* clockwise vertex search */
				{
					vertex= (right_vertex==polygon->vertex_count-1) ? 0 : (right_vertex+1);
					right_line_count= vertices[vertex].y-vertices[right_vertex].y;
					if (!build_x_table(right_table+aggregate_right_line_count, vertices[right_vertex].x, vertices[right_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_right_line_count+= right_line_count;
					right_vertex= vertex;
//					dprintf("add %d right", right_line_count);
				}
				while (!right_line_count);
			}

			/* advance by the minimum of left_line_count and right_line_count */
			delta= MIN(left_line_count, right_line_count);
			assert(delta);
//			dprintf("tc=%d lc=%d rc=%d delta=%d", total_line_count, left_line_count, right_line_count, delta);
			total_line_count-= delta;
			left_line_count-= delta;
			right_line_count-= delta;
			
			assert(delta||!total_line_count); /* if our delta is zero, we�d better be out of lines */
		}
		
		/* make sure every coordinate is accounted for in our tables */
		assert(aggregate_right_line_count==aggregate_total_line_count);
		assert(aggregate_left_line_count==aggregate_total_line_count);

		/* precalculate mode-specific data */
		switch (polygon->transfer_mode)
		{
			case _textured_transfer:
//			case _solid_transfer:
				_pretexture_horizontal_polygon_lines(polygon, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
					vertices[highest_vertex].y, left_table, right_table,
					aggregate_total_line_count);
				break;

			case _big_landscaped_transfer:
				_prelandscape_horizontal_polygon_lines(polygon, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
					vertices[highest_vertex].y, left_table, right_table,
					aggregate_total_line_count);
				break;
			
			default:
				vhalt(csprintf(temporary, "horizontal_polygons dont support mode #%d", polygon->transfer_mode));
		}
		
		/* render all lines */
		switch (bit_depth)
		{
			case 8:
				switch (polygon->transfer_mode)
				{
//					case _solid_transfer:
//#ifdef env68k /* otherwise, fall through to _textured_transfer */
//#ifdef EXTERNAL
//						_fill_horizontal_polygon_lines8(polygon->texture, screen, view, precalculation_table,
//							vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
//						break;
//#endif
//#endif
	
					case _textured_transfer:
						_texture_horizontal_polygon_lines8(polygon->texture, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
							vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
						break;
					case _big_landscaped_transfer:
						_landscape_horizontal_polygon_lines8(polygon->texture, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
							vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
						break;
						
					default:
						halt();
				}
				break;

			case 16:
				switch (polygon->transfer_mode)
				{
					case _textured_transfer:
						_texture_horizontal_polygon_lines16(polygon->texture, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
							vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
						break;
					case _big_landscaped_transfer:
						_landscape_horizontal_polygon_lines16(polygon->texture, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
							vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
						break;
					
					default:
						halt();
				}
				break;

			case 32:
				switch (polygon->transfer_mode)
				{
					case _textured_transfer:
						_texture_horizontal_polygon_lines32(polygon->texture, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
							vertices[highest_vertex].y, left_table, right_table,
							aggregate_total_line_count);
						break;
					case _big_landscaped_transfer:
						_landscape_horizontal_polygon_lines32(polygon->texture, screen, view, (struct _horizontal_polygon_line_data *)precalculation_table,
							vertices[highest_vertex].y, left_table, right_table, aggregate_total_line_count);
						break;
					
					default:
						halt();
				}
				break;

			default:
				halt();
		}
	}
	
	return;
}

void texture_vertical_polygon(
	struct polygon_definition *polygon,
	struct bitmap_definition *screen,
	struct view_data *view)
{
	short vertex, highest_vertex, lowest_vertex;
	point2d *vertices= (point2d *) &polygon->vertices;

	assert(polygon->vertex_count>=MINIMUM_VERTICES_PER_SCREEN_POLYGON&&polygon->vertex_count<MAXIMUM_VERTICES_PER_SCREEN_POLYGON);

	switch (polygon->transfer_mode)
	{
//		case _landscaped_transfer:
//			preprocess_landscaped_polygon(polygon, view);
//			break;
		case _big_landscaped_transfer:
			texture_horizontal_polygon(polygon, screen, view);
			return;
	}

	/* locate the horizontally highest (closest to zero) and lowest (farthest from zero) vertices */
	highest_vertex= lowest_vertex= 0;
	for (vertex=1;vertex<polygon->vertex_count;++vertex)
	{
		if (vertices[vertex].x<vertices[highest_vertex].x) highest_vertex= vertex;
		if (vertices[vertex].x>vertices[lowest_vertex].x) lowest_vertex= vertex;
	}

	for (vertex=0;vertex<polygon->vertex_count;++vertex)
	{
		if (!(vertices[vertex].x>=0&&vertices[vertex].x<=screen->width&&vertices[vertex].y>=0&&vertices[vertex].y<=screen->height))
		{
//			dprintf("vertex #%d/#%d out of bounds:;dm %x %x;g;", vertex, polygon->vertex_count, polygon->vertices, polygon->vertex_count*sizeof(point2d));
			return;
		}
	}

	/* if this polygon is not a vertical line, draw it */
	if (highest_vertex!=lowest_vertex)
	{
		short left_line_count, right_line_count, total_line_count;
		short aggregate_left_line_count, aggregate_right_line_count, aggregate_total_line_count;
		short left_vertex, right_vertex;
		short *left_table= scratch_table0, *right_table= scratch_table1;

		left_line_count= right_line_count= 0; /* zero counts so the left and right lines get initialized */
		aggregate_left_line_count= aggregate_right_line_count= 0; /* we�ve precalculated nothing initially */
		left_vertex= right_vertex= highest_vertex; /* both sides start at the highest vertex */
		total_line_count= vertices[lowest_vertex].x-vertices[highest_vertex].x; /* calculate vertical line count */

		assert(total_line_count<MAXIMUM_SCRATCH_TABLE_ENTRIES); /* make sure we have enough scratch space */
		
		/* precalculate high and low y-coordinates for every x-coordinate */			
		aggregate_total_line_count= total_line_count;
		while (total_line_count>0)
		{
			short delta;
			
			/* if we�re out of scan lines on the left side, get a new vertex and build a table
				of y-coordinates so we can walk toward the new vertex */
			if (left_line_count<=0)
			{
				do /* clockwise vertex search */
				{
					vertex= (left_vertex==polygon->vertex_count-1) ? 0 : (left_vertex+1);
					left_line_count= vertices[vertex].x-vertices[left_vertex].x;
//					dprintf("left line (%d,%d) to (%d,%d) for %d points", vertices[left_vertex].x, vertices[left_vertex].y, vertices[vertex].x, vertices[vertex].y, left_line_count);
					if (!build_y_table(left_table+aggregate_left_line_count, vertices[left_vertex].x, vertices[left_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_left_line_count+= left_line_count;
					left_vertex= vertex;
				}
				while (!left_line_count);
			}

			/* if we�re out of scan lines on the right side, get a new vertex and build a table
				of y-coordinates so we can walk toward the new vertex */
			if (right_line_count<=0)
			{
				do /* counter-clockwise vertex search */
				{
					vertex= right_vertex ? (right_vertex-1) : (polygon->vertex_count-1);
					right_line_count= vertices[vertex].x-vertices[right_vertex].x;
//					dprintf("right line (%d,%d) to (%d,%d) for %d points", vertices[right_vertex].x, vertices[right_vertex].y, vertices[vertex].x, vertices[vertex].y, right_line_count);
					if (!build_y_table(right_table+aggregate_right_line_count, vertices[right_vertex].x, vertices[right_vertex].y, vertices[vertex].x, vertices[vertex].y)) return;
					aggregate_right_line_count+= right_line_count;
					right_vertex= vertex;
				}
				while (!right_line_count);
			}
			
			/* advance by the minimum of left_line_count and right_line_count */
			delta= MIN(left_line_count, right_line_count);
			assert(delta);
			total_line_count-= delta;
			left_line_count-= delta;
			right_line_count-= delta;
			
			assert(delta||!total_line_count); /* if our delta is zero, we�d better be out of lines */
		}
		
		/* make sure every coordinate is accounted for in our tables */
		assert(aggregate_right_line_count==aggregate_total_line_count);
		assert(aggregate_left_line_count==aggregate_total_line_count);

		/* precalculate mode-specific data */
		switch (polygon->transfer_mode)
		{
			case _textured_transfer:
//			case _landscaped_transfer:
			case _static_transfer:
				_pretexture_vertical_polygon_lines(polygon, screen, view, (struct _vertical_polygon_data *)precalculation_table,
					vertices[highest_vertex].x, left_table, right_table, aggregate_total_line_count);
				break;
			
			default:
				vhalt(csprintf(temporary, "vertical_polygons dont support mode #%d", polygon->transfer_mode));
		}
		
		/* render all lines */
		switch (bit_depth)
		{
			case 8:
				switch (polygon->transfer_mode)
				{
					case _textured_transfer:
//					case _landscaped_transfer:
						(polygon->texture->flags&_TRANSPARENT_BIT) ?
							_transparent_texture_vertical_polygon_lines8(screen, view, (struct _vertical_polygon_data *)precalculation_table, left_table, right_table) :
							_texture_vertical_polygon_lines8(screen, view, (struct _vertical_polygon_data *)precalculation_table, left_table, right_table);
						break;
					
					case _static_transfer:
//						_randomize_vertical_polygon_lines8(screen, view, precalculation_table,
//							left_table, right_table, polygon->transfer_data);
						break;
					
					default:
						halt();
				}
				break;
				
			case 16:
				switch (polygon->transfer_mode)
				{
					case _textured_transfer:
//					case _landscaped_transfer:
						(polygon->texture->flags&_TRANSPARENT_BIT) ?
							_transparent_texture_vertical_polygon_lines16(screen, view, (struct _vertical_polygon_data *)precalculation_table, left_table, right_table) :
							_texture_vertical_polygon_lines16(screen, view, (struct _vertical_polygon_data *)precalculation_table, left_table, right_table);
						break;
					
					case _static_transfer:
//						_randomize_vertical_polygon_lines16(screen, view, precalculation_table,
//							left_table, right_table, polygon->transfer_data);
						break;

					default:
						halt();
				}
				break;
			
			case 32:
				switch (polygon->transfer_mode)
				{
					case _textured_transfer:
//					case _landscaped_transfer:
						(polygon->texture->flags&_TRANSPARENT_BIT) ?
							_transparent_texture_vertical_polygon_lines32(screen, view, (struct _vertical_polygon_data *)precalculation_table, left_table, right_table) :
							_texture_vertical_polygon_lines32(screen, view, (struct _vertical_polygon_data *)precalculation_table, left_table, right_table);
						break;
					
					case _static_transfer:
//						_randomize_vertical_polygon_lines32(screen, view, precalculation_table,
//							left_table, right_table, polygon->transfer_data);
						break;

					default:
						halt();
				}
				break;
			
			default:
				halt();
		}
	}
	
	return;
}

void texture_rectangle(
	struct rectangle_definition *rectangle,
	struct bitmap_definition *screen,
	struct view_data *view)
{
	if (rectangle->x0<rectangle->x1 && rectangle->y0<rectangle->y1)
	{
		/* subsume screen boundaries into clipping parameters */
		if (rectangle->clip_left<0) rectangle->clip_left= 0;
		if (rectangle->clip_right>screen->width) rectangle->clip_right= screen->width;
		if (rectangle->clip_top<0) rectangle->clip_top= 0;
		if (rectangle->clip_bottom>screen->height) rectangle->clip_bottom= screen->height;
	
		/* subsume left and right sides of the rectangle into clipping parameters */
		if (rectangle->clip_left<rectangle->x0) rectangle->clip_left= rectangle->x0;
		if (rectangle->clip_right>rectangle->x1) rectangle->clip_right= rectangle->x1;
		if (rectangle->clip_top<rectangle->y0) rectangle->clip_top= rectangle->y0;
		if (rectangle->clip_bottom>rectangle->y1) rectangle->clip_bottom= rectangle->y1;
	
		/* only continue if we have a non-empty rectangle, at least some of which is on the screen */
		if (rectangle->clip_left<rectangle->clip_right && rectangle->clip_top<rectangle->clip_bottom &&
			rectangle->clip_right>0 && rectangle->clip_left<screen->width &&
			rectangle->clip_bottom>0 && rectangle->clip_top<screen->height)
		{
			short delta; /* scratch */
			short screen_width= rectangle->x1-rectangle->x0;
			short screen_height= rectangle->y1-rectangle->y0;
			short screen_x= rectangle->x0, screen_y= rectangle->y0;
			struct bitmap_definition *texture= rectangle->texture;
			void *shading_table;
	
			short *y0_table= scratch_table0, *y1_table= scratch_table1;
			struct _vertical_polygon_data *header= (struct _vertical_polygon_data *)precalculation_table;
			struct _vertical_polygon_line_data *data= (struct _vertical_polygon_line_data *) (header+1);
			
			fixed texture_dx= INTEGER_TO_FIXED(texture->width)/screen_width;
			fixed texture_x= texture_dx>>1;
	
			fixed texture_dy= INTEGER_TO_FIXED(texture->height)/screen_height;
			fixed texture_y0= 0;
			fixed texture_y1;
			
			if (texture_dx&&texture_dy)
			{
				/* handle horizontal mirroring */
				if (rectangle->flip_horizontal)
				{
					texture_dx= -texture_dx;
					texture_x= INTEGER_TO_FIXED(texture->width)+(texture_dx>>1);
				}
				
				/* left clipping */		
				if ((delta= rectangle->clip_left-rectangle->x0)>0)
				{
					texture_x+= delta*texture_dx;
					screen_width-= delta;
					screen_x= rectangle->clip_left;
				}				
				/* right clipping */
				if ((delta= rectangle->x1-rectangle->clip_right)>0)
				{
					screen_width-= delta;
				}
				
				/* top clipping */
				if ((delta= rectangle->clip_top-rectangle->y0)>0)
				{
					texture_y0+= delta*texture_dy;
					screen_height-= delta;
				}
				
				/* bottom clipping */
				if ((delta= rectangle->y1-rectangle->clip_bottom)>0)
				{
					screen_height-= delta;
				}
	
				texture_y1= texture_y0 + screen_height*texture_dy;
				
				header->downshift= FIXED_FRACTIONAL_BITS;
				header->width= screen_width;
				header->x0= screen_x;
				
				/* calculate shading table, once */
				switch (rectangle->transfer_mode)
				{
					case _textured_transfer:
						if (!(rectangle->flags&_SHADELESS_BIT))
						{
							CALCULATE_SHADING_TABLE(shading_table, view, rectangle->shading_tables, rectangle->depth, rectangle->ambient_shade);
							break;
						}
						/* if shadeless, fall through to a single shading table, ignoring depth */
					case _tinted_transfer:
					case _static_transfer:
						shading_table= rectangle->shading_tables;
						break;
					
					default:
						vhalt(csprintf(temporary, "rectangles dont support mode #%d", rectangle->transfer_mode));
				}
		
				for (; screen_width; --screen_width)
				{
					void *read= texture->row_addresses[FIXED_INTEGERAL_PART(texture_x)];
					short first= *((short*)read)++, last= *((short*)read)++;
					fixed texture_y= texture_y0;
					short y0= rectangle->clip_top, y1= rectangle->clip_bottom;
					
					if (FIXED_INTEGERAL_PART(texture_y0)<first)
					{
						delta= (INTEGER_TO_FIXED(first) - texture_y0)/texture_dy + 1;
						vassert(delta>=0, csprintf(temporary, "[%x,%x] �=%x (#%d,#%d)", texture_y0, texture_y1, texture_dy, first, last));
						
						y0= MIN(y1, y0+delta);
						texture_y+= delta*texture_dy;
					}
					
					if (FIXED_INTEGERAL_PART(texture_y1)>last)
					{
						delta= (texture_y1 - INTEGER_TO_FIXED(last))/texture_dy + 1;
						vassert(delta>=0, csprintf(temporary, "[%x,%x] �=%x (#%d,#%d)", texture_y0, texture_y1, texture_dy, first, last));
						
						y1= MAX(y0, y1-delta);
					}
					
					data->texture_y= texture_y - INTEGER_TO_FIXED(first);
					data->texture_dy= texture_dy;
					data->shading_table= shading_table;
					data->texture= (byte *)read;
					
					texture_x+= texture_dx;
					data+= 1;
					
					*y0_table++= y0;
					*y1_table++= y1;
					
					assert(y0<=y1);
					assert(y0>=0 && y1>=0);
					assert(y0<=screen->height);
					assert(y1<=screen->height);
				}
		
				switch (bit_depth)
				{
					case 8:
						switch (rectangle->transfer_mode)
						{
							case _textured_transfer:
								_transparent_texture_vertical_polygon_lines8(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1);
								break;
							
							case _static_transfer:
								_randomize_vertical_polygon_lines8(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1, rectangle->transfer_data);
								break;
							
							case _tinted_transfer:
								_tint_vertical_polygon_lines8(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1, rectangle->transfer_data);
								break;
							
							default:
								halt();
						}
						break;
		
					case 16:
						switch (rectangle->transfer_mode)
						{
							case _textured_transfer:
								_transparent_texture_vertical_polygon_lines16(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1);
								break;
							
							case _static_transfer:
								_randomize_vertical_polygon_lines16(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1, rectangle->transfer_data);
								break;
							
							case _tinted_transfer:
								_tint_vertical_polygon_lines16(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1, rectangle->transfer_data);
								break;
							
							default:
								halt();
						}
						break;
		
					case 32:
						switch (rectangle->transfer_mode)
						{
							case _textured_transfer:
								_transparent_texture_vertical_polygon_lines32(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1);
								break;
							
							case _static_transfer:
								_randomize_vertical_polygon_lines32(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1, rectangle->transfer_data);
								break;
							
							case _tinted_transfer:
								_tint_vertical_polygon_lines32(screen, view, (struct _vertical_polygon_data *)precalculation_table,
									scratch_table0, scratch_table1, rectangle->transfer_data);
								break;
							
							default:
								halt();
						}
						break;
		
					default:
						halt();
				}
			}
		}
	}

	return;
}

/* generate code for 8-bit texture-mapping */
#define BIT_DEPTH 8
#include "low_level_textures.c"
#undef BIT_DEPTH

/* generate code for 16-bit texture-mapping */
#define BIT_DEPTH 16
#include "low_level_textures.c"
#undef BIT_DEPTH

/* generate code for 32-bit texture-mapping */
#define BIT_DEPTH 32
#include "low_level_textures.c"
#undef BIT_DEPTH

/* ---------- private code */

#if 0

#define LANDSCAPE_REPEATS 12
static void preprocess_landscaped_polygon(
	struct polygon_definition *polygon,
	struct view_data *view)
{
	polygon->origin.x= (world_distance) ((10000*LANDSCAPE_REPEATS*WORLD_ONE)/(2*31415));
	polygon->origin.y= -(((LANDSCAPE_REPEATS*WORLD_ONE*view->yaw)>>ANGULAR_BITS)&(WORLD_ONE-1));
	polygon->origin.z= 0;
	
	polygon->vector.i= 0;
	polygon->vector.j= WORLD_ONE;
	polygon->vector.k= -WORLD_ONE;

	polygon->ambient_shade= FIXED_ONE;
	
	return;
}

#endif

/* starting at x0 and for line_count vertical lines between *y0 and *y1, precalculate all the
	information _texture_vertical_polygon_lines will need to work */
static void _pretexture_vertical_polygon_lines(
	struct polygon_definition *polygon,
	struct bitmap_definition *screen,
	struct view_data *view,
	struct _vertical_polygon_data *data,
	short x0,
	short *y0_table,
	short *y1_table,
	short line_count)
{
	short screen_x= x0-view->half_screen_width;
	long dz0= view->world_to_screen_y*polygon->origin.z;
	long unadjusted_ty_denominator= view->world_to_screen_y*polygon->vector.k;
	long tx_numerator, tx_denominator, tx_numerator_delta, tx_denominator_delta;
	struct _vertical_polygon_line_data *line= (struct _vertical_polygon_line_data *) (data+1);

	#pragma unused (screen)

	assert(sizeof(struct _vertical_polygon_line_data)<=MAXIMUM_PRECALCULATION_TABLE_ENTRY_SIZE);

	data->downshift= VERTICAL_TEXTURE_DOWNSHIFT;
	data->x0= x0;
	data->width= line_count;

	/* calculate and rescale tx_numerator, tx_denominator, etc. */
	tx_numerator= view->world_to_screen_x*polygon->origin.y - screen_x*polygon->origin.x;
	tx_denominator= screen_x*polygon->vector.i - view->world_to_screen_x*polygon->vector.j;
	tx_numerator_delta= -polygon->origin.x;
	tx_denominator_delta= polygon->vector.i;

	while (--line_count>=0)
	{
		fixed tx;
		world_distance world_x;
		short x0, y0= *y0_table++, y1= *y1_table++;
		short screen_y0= view->half_screen_height-y0+view->dtanpitch;
		long ty_numerator, ty_denominator;
		fixed ty, ty_delta;

		/* would our precision be greater here if we shifted the numerator up to $7FFFFFFF and
			then downshifted only the numerator?  too bad we can�t use BFFFO in 68k */
		{
			long adjusted_tx_denominator= tx_denominator;
			long adjusted_tx_numerator= tx_numerator;
			
			while (adjusted_tx_numerator>((1<<(31-VERTICAL_TEXTURE_WIDTH_BITS))-1) ||
				adjusted_tx_numerator<((-1)<<(31-VERTICAL_TEXTURE_WIDTH_BITS)))
			{
				adjusted_tx_numerator>>= 1, adjusted_tx_denominator>>= 1;
			}
			if (!adjusted_tx_denominator) adjusted_tx_denominator= 1; /* -1 will still be -1 */
			x0= ((adjusted_tx_numerator<<VERTICAL_TEXTURE_WIDTH_BITS)/adjusted_tx_denominator)&(VERTICAL_TEXTURE_WIDTH-1);

			while (adjusted_tx_numerator>SHORT_MAX||adjusted_tx_numerator<SHORT_MIN)
			{
				adjusted_tx_numerator>>= 1, adjusted_tx_denominator>>= 1;
			}
			if (!adjusted_tx_denominator) adjusted_tx_denominator= 1; /* -1 will still be -1 */
			tx= INTEGER_TO_FIXED(adjusted_tx_numerator)/adjusted_tx_denominator;
		}
		
		world_x= polygon->origin.x + ((tx*polygon->vector.i)>>FIXED_FRACTIONAL_BITS);
		if (world_x<0) world_x= -world_x; /* it is mostly unclear what we�re supposed to do with negative x values */

		/* calculate and rescale ty_numerator, ty_denominator and calculate ty */
		ty_numerator= world_x*screen_y0 - dz0;
		ty_denominator= unadjusted_ty_denominator;
		while (ty_numerator>SHORT_MAX||ty_numerator<SHORT_MIN)
		{
			ty_numerator>>= 1, ty_denominator>>= 1;
		}
		if (!ty_denominator) ty_denominator= 1; /* -1 will still be -1 */
		ty= INTEGER_TO_FIXED(ty_numerator)/ty_denominator;

		ty_delta= - INTEGER_TO_FIXED(world_x)/(unadjusted_ty_denominator>>8);
		vassert(ty_delta>=0, csprintf(temporary, "ty_delta=W2F(%d)/%d=%d", world_x, unadjusted_ty_denominator, ty_delta));

		/* calculate the shading table for this column */
		if (polygon->flags&_SHADELESS_BIT)
		{
			line->shading_table= polygon->shading_tables;
		}
		else
		{
			CALCULATE_SHADING_TABLE(line->shading_table, view, polygon->shading_tables, world_x, polygon->ambient_shade);
		}

//		if (ty_delta)
		{
			/* calculate texture_y and texture_dy (floor-mapper style) */
//			data->n= VERTICAL_TEXTURE_DOWNSHIFT;
			line->texture_y= ty<<VERTICAL_TEXTURE_FREE_BITS;
			line->texture_dy= ty_delta<<(VERTICAL_TEXTURE_FREE_BITS-8);
			line->texture= polygon->texture->row_addresses[x0];
			
			line+= 1;
		}
		
		tx_numerator+= tx_numerator_delta;
		tx_denominator+= tx_denominator_delta;
		
		screen_x+= 1;
	}
	
	return;
}

static void _pretexture_horizontal_polygon_lines(
	struct polygon_definition *polygon,
	struct bitmap_definition *screen,
	struct view_data *view,
	struct _horizontal_polygon_line_data *data,
	short y0,
	short *x0_table,
	short *x1_table,
	short line_count)
{
	long hcosine, dhcosine;
	long hsine, dhsine;
	long hworld_to_screen;
	boolean higher_precision= polygon->origin.z>-WORLD_ONE && polygon->origin.z<WORLD_ONE;

	#pragma unused (screen)
	
	/* precalculate a bunch of multiplies */
	hcosine= cosine_table[view->yaw];
	hsine= sine_table[view->yaw];
	if (higher_precision)
	{
		hcosine*= polygon->origin.z;
		hsine*= polygon->origin.z;
	}
	hworld_to_screen= polygon->origin.z*view->world_to_screen_y;
	dhcosine= view->world_to_screen_y*hcosine;
	dhsine= view->world_to_screen_y*hsine;

	while ((line_count-=1)>=0)
	{
		world_distance depth;
		short screen_x, screen_y;
		short x0= *x0_table++, x1= *x1_table++;
		
		/* calculate screen_x,screen_y */
		screen_x= x0-view->half_screen_width;
		screen_y= view->half_screen_height-y0+view->dtanpitch;
		if (!screen_y) screen_y= 1; /* this will avoid division by zero and won't change rendering */
		
		/* calculate source_x, source_y, source_dx, source_dy */
//#ifdef env68k
//		if (polygon->transfer_mode!=_solid_transfer)
//#endif
		{
			long source_x, source_y, source_dx, source_dy;
			
			/* calculate texture origins and deltas (source_x,source_dx,source_y,source_dy) */
			if (higher_precision)
			{
				source_x= (dhcosine - screen_x*hsine)/screen_y + (polygon->origin.x<<TRIG_SHIFT);
				source_dx= - hsine/screen_y;
				source_y= (screen_x*hcosine + dhsine)/screen_y + (polygon->origin.y<<TRIG_SHIFT);
				source_dy= hcosine/screen_y;
			}
			else
			{
				source_x= ((dhcosine - screen_x*hsine)/screen_y)*polygon->origin.z + (polygon->origin.x<<TRIG_SHIFT);
				source_dx= - (hsine*polygon->origin.z)/screen_y;
				source_y= ((screen_x*hcosine + dhsine)/screen_y)*polygon->origin.z + (polygon->origin.y<<TRIG_SHIFT);
				source_dy= (hcosine*polygon->origin.z)/screen_y;
			}
		
			/* voodoo so x,y texture wrapping is handled automatically by downshifting
				(subtract one from HORIZONTAL_FREE_BITS to double scale) */
			data->source_x= source_x<<HORIZONTAL_FREE_BITS, data->source_dx= source_dx<<HORIZONTAL_FREE_BITS;
			data->source_y= source_y<<HORIZONTAL_FREE_BITS, data->source_dy= source_dy<<HORIZONTAL_FREE_BITS;
		}

		/* get shading table (with absolute value of depth) */
		if ((depth= hworld_to_screen/screen_y)<0) depth= -depth;
		if (polygon->flags&_SHADELESS_BIT)
		{
			data->shading_table= polygon->shading_tables;
		}
		else
		{
			CALCULATE_SHADING_TABLE(data->shading_table, view, polygon->shading_tables, (short)MIN(depth, SHORT_MAX), polygon->ambient_shade);
		}
		
		data+= 1;
		y0+= 1;
	}
	
	return;
}

// height must be determined emperically (texture is vertically centered at 0�)
#define LANDSCAPE_REPEAT_BITS 1
static void _prelandscape_horizontal_polygon_lines(
	struct polygon_definition *polygon,
	struct bitmap_definition *screen,
	struct view_data *view,
	struct _horizontal_polygon_line_data *data,
	short y0,
	short *x0_table,
	short *x1_table,
	short line_count)
{
	short landscape_width_bits= polygon->texture->height==1024 ? 10 : 9;
	short texture_height= polygon->texture->width;
	fixed ambient_shade= FIXED_ONE; // MPW C died if we passed the constant directly to the macro
	fixed first_pixel= view->yaw<<(landscape_width_bits+LANDSCAPE_REPEAT_BITS+FIXED_FRACTIONAL_BITS-ANGULAR_BITS);
	fixed pixel_delta= (view->half_cone<<(1+landscape_width_bits+LANDSCAPE_REPEAT_BITS+FIXED_FRACTIONAL_BITS-ANGULAR_BITS))/view->standard_screen_width;
	short landscape_free_bits= 32-FIXED_FRACTIONAL_BITS-landscape_width_bits;
	void *shading_table;
	
	#pragma unused (screen)

	/* calculate the shading table */	
	if (polygon->flags&_SHADELESS_BIT)
	{
		shading_table= polygon->shading_tables;
	}
	else
	{
		CALCULATE_SHADING_TABLE(shading_table, view, polygon->shading_tables, 0, ambient_shade);
	}

	y0-= view->half_screen_height + view->dtanpitch; /* back to virtual screen coordinates */
	while ((line_count-= 1)>=0)
	{
		short x0= *x0_table++, x1= *x1_table++;
		
		data->shading_table= shading_table;
		data->source_y= FIXED_INTEGERAL_PART(y0*pixel_delta) + (texture_height>>1);
		data->source_y= texture_height - PIN(data->source_y, 0, texture_height-1) - 1;
		
		data->source_x= (first_pixel + x0*pixel_delta)<<landscape_free_bits;
		data->source_dx= pixel_delta<<landscape_free_bits;
		
		data+= 1;
		y0+= 1;
	}
	
	return;
}

/* y0<y1; this is for vertical polygons */
static short *build_x_table(
	short *table,
	short x0,
	short y0,
	short x1,
	short y1)
{
	short dx, dy, adx, ady; /* 'a' prefix means absolute value */
	short x, y; /* x,y screen positions */
	short d, delta_d, d_max; /* descriminator, delta_descriminator, descriminator_maximum */
	short *record;

	/* calculate SGN(dx),SGN(dy) and the absolute values of dx,dy */	
	dx= x1-x0, adx= ABS(dx), dx= SGN(dx);
	dy= y1-y0, ady= ABS(dy), dy= SGN(dy);

	assert(ady<MAXIMUM_SCRATCH_TABLE_ENTRIES); /* can't overflow table */
//	vassert(dy>=0, csprintf(temporary, "dy must be positive, (%d,%d) to (%d,%d)", x0, y0, x1, y1));
	if (dy>0)
	{
		/* setup initial (x,y) location and initialize a pointer to our table */
		x= x0, y= y0;
		record= table;
	
		if (adx>=ady)
		{
			/* x-dominant line (we need to record x every time y changes) */
	
			d= adx-ady, delta_d= - 2*ady, d_max= 2*adx;
			while ((adx-=1)>=0)
			{
				if (d<0) y+= 1, d+= d_max, *record++= x, ady-= 1;
				x+= dx, d+= delta_d;
			}
			if (ady==1) *record++= x; else assert(!ady);
		}
		else
		{
			/* y-dominant line (we need to record x every iteration) */
	
			d= ady-adx, delta_d= - 2*adx, d_max= 2*ady;
			while ((ady-=1)>=0)
			{
				if (d<0) x+= dx, d+= d_max;
				*record++= x;
				y+= 1, d+= delta_d;
			}
		}
	}
	else
	{
		/* can�t build a table for negative dy */
		if (dy<0) table= (short *) NULL;
	}
	
	return table;
}

/* x0<x1; this is for horizontal polygons */
static short *build_y_table(
	short *table,
	short x0,
	short y0,
	short x1,
	short y1)
{
	short dx, dy, adx, ady; /* 'a' prefix means absolute value */
	short x, y; /* x,y screen positions */
	short d, delta_d, d_max; /* descriminator, delta_descriminator, descriminator_maximum */
	short *record;

	/* calculate SGN(dx),SGN(dy) and the absolute values of dx,dy */	
	dx= x1-x0, adx= ABS(dx), dx= SGN(dx);
	dy= y1-y0, ady= ABS(dy), dy= SGN(dy);

	assert(adx<MAXIMUM_SCRATCH_TABLE_ENTRIES); /* can't overflow table */
//	vassert(dx>=0, csprintf(temporary, "dx must be positive, (%d,%d) to (%d,%d)", x0, y0, x1, y1));
	if (dx>=0) /* vertical lines allowed */
	{
		/* setup initial (x,y) location and initialize a pointer to our table */
		if (dy>=0)
		{
			x= x0, y= y0;
			record= table;
		}
		else
		{
			x= x1, y= y1;
			record= table+adx;
		}
	
		if (adx>=ady)
		{
			/* x-dominant line (we need to record y every iteration) */
	
			d= adx-ady, delta_d= - 2*ady, d_max= 2*adx;
			while ((adx-=1)>=0)
			{
				if (d<0) y+= 1, d+= d_max;
				if (dy>=0) *record++= y; else *--record= y;
				x+= dx, d+= delta_d;
			}
		}
		else
		{
			/* y-dominant line (we need to record y every time x changes) */
	
			d= ady-adx, delta_d= - 2*adx, d_max= 2*ady;
			while ((ady-=1)>=0)
			{
				if (d<0) { x+= dx, d+= d_max, adx-= 1; if (dy>=0) *record++= y; else *--record= y; }
				y+= 1, d+= delta_d;
			}
			if (adx==1) if (dy>=0) *record++= y; else *--record= y; else assert(!adx);
		}
	}
	else
	{
		/* can�t build a table for a negative dx */
		table= (short *) NULL;
	}
	
	return table;
}

#ifdef OBSOLETE /* unnecessary, but do not delete */
/* clip_low<clip_high in destination units; this function builds a table of source coordinates
	for each unclipped destination coordinate (the landscape mapper and the object mapper
	will both use this to build y-coordinate lookup tables) */
static short *build_distortion_table(
	short *table,
	short source,
	short destination,
	short clip_low,
	short clip_high,
	boolean mirror)
{
	register short i;
	short count, source_coordinate;
	register fixed d, delta_d, d_max;
	register short *record;
	
	assert(clip_low<=clip_high);
	if (clip_low<0) clip_low= 0;
	if (clip_high>destination) clip_high= destination;
	
	/* initialize d, delta_d, d_max */
	d= 2*destination - source;
	delta_d= 2*source;
	d_max= 2*destination;
	
	/* if unclipped, we'll generation translation entries for every destination pixel */
	count= destination;
	
	/* if unclipped we'll start at source==0 */
	source_coordinate= 0;
	
	/* even marginally clever editing of d_max and delta_d with the addition of a new
		source_coordinate_delta variable would allow us to change the while (d<=0) loop
		into an if (d<=0) below */

	if (mirror)
	{
		short low_clipped_pixels= clip_low, high_clipped_pixels= destination-clip_high;
		clip_low= high_clipped_pixels, clip_high= destination-low_clipped_pixels;
	}

	/* handle high clipping by adjusting count */
	if (clip_high<destination) count-= destination-clip_high;
	
	/* handle low clipping by adjusting d, count, source_coordinate */
	if (clip_low>0)
	{
		d-= clip_low*delta_d;
		if (d<=0)
		{
			short n= 1 - d/d_max;
		
			d+= n*d_max;
			source_coordinate+= n;
		}

		count-= clip_low;
	}

	for (i=count,record=mirror?table+count:table;i>0;--i)
	{
		while (d<=0) source_coordinate+= 1, d+= d_max;
		if (mirror) *--record= source_coordinate; else *record++= source_coordinate;
		d-= delta_d;
	}

	if (count)
	{
		if (table[0]<0 || table[0]>=source || table[count-1]<0 || table[count-1]>=source)
		{
			dprintf("%d==>%d [%d/%d==>%d] at %p", source, destination, clip_low, clip_high, count, table);
		}
	}
	
//	if (destination<20) dprintf("table %d-->%d (cliped to %d);dm #%d #%d;", source, destination, fuck, table, fuck*sizeof(short));
	
	return table;
}
#endif

#ifdef OBSOLETE /* replaced by macro */
/* a negative ambient_shade is interpreted as an absolute shade, and is not modified by depth
	shading */
void *calculate_shading_table(
	struct view_data *view,
	void *shading_tables,
	world_distance depth,
	fixed ambient_shade)
{
	short table_index;
	long shading_table_size;
	
	switch (bit_depth)
	{
		case 8: shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel8); break;
		case 16: shading_table_size= PIXEL8_MAXIMUM_COLORS*sizeof(pixel16); break;
		default: halt();
	}

#ifdef env68k
	/* eventually the shading function will depend on something in the view_data structure
		(and this will replace the DEPTH_TO_SHADE macro) */
	#pragma unused (view)
#endif

	if (ambient_shade<0)
	{
		assert(ambient_shade>=-FIXED_ONE);
		table_index= SHADE_TO_SHADING_TABLE_INDEX(-ambient_shade);
	}
	else
	{
		/* calculate shading index due to viewer�s depth, adjust it according to FIRST_ and
			LAST_SHADING_TABLE, then take whichever of this index or the index due to the
			ambient shade is higher */
		table_index= SHADE_TO_SHADING_TABLE_INDEX(DEPTH_TO_SHADE(depth));
		table_index= LAST_SHADING_TABLE-PIN(table_index, FIRST_SHADING_TABLE, LAST_SHADING_TABLE);
		table_index= MAX(SHADE_TO_SHADING_TABLE_INDEX(ambient_shade), table_index);
	}

	/* calculate and return exact shading table */
	return ((byte*)shading_tables) + shading_table_size*CEILING(table_index, number_of_shading_tables-1);
}
#endif

#if OBSOLETE_CLIPPING
		/* fake clipping */
		{
			short i;

			high_point= vertices[highest_vertex].y;
			
			for (i=0;i<aggregate_total_line_count;++i)
			{
				if (left_table[i]<0) left_table[i]= 0;
				if (right_table[i]>screen->width) right_table[i]= screen->width;
				if (left_table[i]<polygon->clip_left) left_table[i]= polygon->clip_left;
				if (right_table[i]>polygon->clip_right) right_table[i]= polygon->clip_right-1;
			}

			if (vertices[highest_vertex].y<0)
			{
				aggregate_total_line_count+= vertices[highest_vertex].y;
				left_table-= vertices[highest_vertex].y;
				right_table-= vertices[highest_vertex].y;
				
				high_point= 0; /* ow */
			}
			
			if (vertices[lowest_vertex].y>screen->height)
			{
				aggregate_total_line_count-= vertices[lowest_vertex].y-screen->height;
			}

			if (aggregate_total_line_count<=0) return;
		}
#endif

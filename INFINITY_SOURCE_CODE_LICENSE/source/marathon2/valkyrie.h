/*
VALKYRIE.H
Saturday, August 20, 1994 11:21:55 AM
*/

/* ---------- prototypes/VALKYRIE.C */

boolean machine_has_valkyrie(GDSpecPtr spec);

void valkyrie_initialize(GDHandle device, boolean pixel_doubling, Rect *frame, pixel8 transparent);
void valkyrie_restore(void);

void valkyrie_begin(void);
void valkyrie_end(void);

void valkyrie_initialize_invisible_video_buffer(struct bitmap_definition *bitmap);
void valkyrie_switch_to_invisible_video_buffer(void);

void valkyrie_erase_video_buffers(void);
void valkyrie_erase_graphic_key_frame(pixel8 transparent_color_index);

void valkyrie_change_video_clut(CTabHandle color_table);
void valkyrie_change_graphic_clut(CTabHandle color_table);

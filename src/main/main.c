#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MAX_WADFILE_NAME_LEN 63
#define MAX_WADFILE_NAME_SIZE (MAX_WADFILE_NAME_LEN + 1)
#define PREFERENCES_NAME_LEN 255
#define PREFERENCES_NAME_SIZE (PREFERENCES_NAME_LEN + 1)
#define FD_NAME_LEN MAX_WADFILE_NAME_LEN
#define FD_NAME_SIZE (FD_NAME_LEN + 1)
#define LEVEL_NAME_LEN MAX_WADFILE_NAME_LEN
#define LEVEL_NAME_SIZE (LEVEL_NAME_LEN + 1)
#define MAX_OBJECT_TYPES 64

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

struct player_preferences {
	uint64_t last_time_exec;
	int16_t difficulty_level;
	int16_t color;
	int16_t team;
	char name[PREFERENCES_NAME_SIZE];
	bool background_music_enabled;
};

static struct preferences *preferences = NULL;

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

struct world_point2d {
	int16_t x;
	int16_t y;
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
	struct world_point2d game_beacon;
	uint64_t tick_count;
	uint32_t random_seed;
	int16_t player_count;
	int16_t speaking_player_index;
	int16_t unused;
	int16_t platform_count;
	int16_t endpoint_count;
	int16_t line_count;
	int16_t side_count;
	int16_t polygon_count;
	int16_t lightsource_count;
	int16_t map_index_count;
	int16_t ambient_sound_image_count, random_sound_image_count;
	int16_t object_count;
	int16_t monster_count;
	int16_t projectile_count;
	int16_t effect_count;
	int16_t light_count;
	int16_t default_annotation_count;
	int16_t personal_annotation_count;
	int16_t initial_objects_count;
	int16_t garbage_object_count;
	int16_t last_monster_index_to_get_time, last_monster_index_to_build_path;
	int16_t new_monster_mangler_cookie, new_monster_vanishing_cookie;
	int16_t civilians_killed_by_players;
	int16_t random_monsters_left[MAX_OBJECT_TYPES];
	int16_t current_monster_count[MAX_OBJECT_TYPES];
	int16_t random_items_left[MAX_OBJECT_TYPES];
	int16_t current_item_count[MAX_OBJECT_TYPES];
	int16_t current_level_number;
	int16_t current_civilian_causalties, current_civilian_count;
	int16_t total_civilian_causalties, total_civilian_count;
	int16_t game_player_index;
};

static struct data_static *world_static = NULL;
static struct data_dynamic *world_dynamic = NULL;

void *wad_extractTypeFromWad(uint64_t *length, struct wad const *wad, uint64_t wadDataType);
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

#define WAD_FILENAME "wadfile.dat"

int main (void)
{
	printf("sizeof(struct data_static): %zu\n", sizeof(struct data_static));
	printf("sizeof(struct wad_header): %zu\n", sizeof(struct wad_header));
	char wadfile[FD_NAME_SIZE] = WAD_FILENAME;
	FILE *file = fopen(wadfile, "w");
	if (!file) {
		printf("main: IO ERROR\n");
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
	return 0;
}

void *wad_extractTypeFromWad (uint64_t *length, struct wad const *wad, uint64_t wadDataType)
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

void *wad_getDataFromPreferences (uint64_t tag,
				  uint64_t expected_size,
				  void (*initialize) (void *prefs),
				  bool (*validate) (void *prefs))
{

	return NULL;
}

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
		return;
	}
	printf("wad_writeWadHeader: bytes-written: %zu\n", bytes_written);
}

void wad_createEmptyWad (struct wad *wad)
{
	memset(wad, 0, sizeof(*wad));
}

void allocate_memory_map (void)
{
	world_static = (struct data_static*) malloc(sizeof(struct data_static));
	world_dynamic = (struct data_dynamic*) malloc(sizeof(struct data_dynamic));
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

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

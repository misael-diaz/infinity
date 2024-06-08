#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define PREFERENCES_NAME_LEN 255
#define PREFERENCES_NAME_SIZE (PREFERENCES_NAME_LEN + 1)
#define FD_NAME_LEN 63
#define FD_NAME_SIZE (FD_NAME_LEN + 1)

typedef unsigned char byte;

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
	byte *data;
};

struct wad {
	struct tag *tag;
	byte *read_only_data;
	int16_t num_wads;
	int16_t pad;
};

struct FileDescriptor {
	uint64_t id;
	int16_t reference_number;
	byte name[FD_NAME_SIZE];
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

int main (void)
{
	return 0;
}

void *wad_extractTypeFromWad (uint64_t *length, struct wad const *wad, uint64_t wadDataType)
{
	*length = ((uint64_t) 0);
	void *data = NULL;
	for (int16_t i = 0; i != wad->num_wads; ++i) {
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

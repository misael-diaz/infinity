#ifndef GUARD_INFINITY_UTIL_H
#define GUARD_INFINITY_UTIL_H

#include <stddef.h>

void *Util_Free(void *p);
void *Util_Clear(void);
void *Util_Malloc(size_t const sz);
char *Util_CopyString(const char *string);

#endif

/*

INFINITY                                             June 07, 2023

author: @misael-diaz
source: include/util.h

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

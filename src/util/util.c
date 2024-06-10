#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct m_chain_s {
	struct m_chain_s *prev;
	struct m_chain_s *next;
	void *data;
	size_t size;
} m_chain_t;

static m_chain_t m_chain;
static size_t m_size = 0;
static size_t m_count = 0;

static m_chain_t *Util_Chain (m_chain_t *node)
{
	m_chain_t *next = (m_chain.next)? m_chain.next : NULL;
	if (next) {
		next->prev = node;
	}

	node->next = next;
	node->prev = &m_chain;
	m_chain.next = node;
	return node;
}

static m_chain_t *Util_Remove (m_chain_t *node)
{
	m_chain_t *prev = node->prev;
	m_chain_t *next = node->next;
	if (next) {
		next->prev = prev;
	}

	prev->next = next;
	node->next = NULL;
	node->prev = NULL;
	node->data = NULL;
	free(node);
	node = NULL;
	return node;
}

void *Util_Free (void *p)
{
	if (!p) {
		return NULL;
	}

	m_chain_t *node = ((m_chain_t*) p) - 1;
	size_t const size = node->size;
	node = Util_Remove(node);

	m_size -= size;
	--m_count;

	return NULL;
}

void Util_Clear (void)
{
	m_chain_t *next = NULL;
	for (m_chain_t *node = m_chain.next; node; node = next) {
		next = node->next;
		void *data = node->data;
		node = (m_chain_t*) Util_Free(data);
	}

	m_size = 0;
	m_count = 0;
}

void *Util_Malloc (size_t const sz)
{
	size_t const size = sizeof(m_chain_t) + sz;
	void *p = malloc(size);
	if (!p) {
		fprintf(stderr, "Util_Malloc: %s\n", strerror(errno));
		return NULL;
	}

	m_chain_t* node = (m_chain_t*) p;
	void *data = (node + 1);

	node = Util_Chain(node);
	node->data = data;
	node->size = size;

	m_size += size;
	++m_count;

	return data;
}

char *Util_CopyString (const char *string)
{
	size_t const len = strlen(string);
	size_t const sz = (len + 1);
	void *ptr = Util_Malloc(sz);
	if (!ptr) {
		fprintf(stderr, "Util_CopyString: error\n");
		return NULL;
	}

	const char *src = string;
	char *dst = (char*) ptr;
	return strcpy(dst, src);
}

/*

INFINITY                                             June 07, 2023

author: @misael-diaz
source: src/util/util.c

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

/*
 * List
 *
 * Copyright (C) 2024 Andre Naef
 */


#include "list.h"
#include <stdlib.h>
#include <string.h>


static int list_check_alloc(size_t size, size_t alloc);
static size_t list_alloc(size_t alloc);


list_t *list_create (size_t size, size_t alloc) {
	list_t  *l;

	l = calloc(1, sizeof(list_t));
	if (!l) {
		return NULL;
	}
	l->size = size;
	l->alloc = list_alloc(alloc > 0 ? alloc : 1);
	if (!list_check_alloc(l->size, l->alloc)) {
		free(l);
		return NULL;
	}
	l->entries = calloc(l->alloc, l->size);
	if (!l->entries) {
		free(l);
		return NULL;
	}
	return l;
}

void list_free (list_t *l) {
	char   *entry;
	size_t  i;

	if (l->free) {
		entry = l->entries;
		for (i = 0; i < l->count; i++) {
			free(*(void **)entry);
			entry += l->size;
		}
	}
	free(l->entries);
	free(l);
}

void list_clear (list_t *l) {
	char   *entry;
	size_t  i;

	if (l->free) {
		entry = l->entries;
		for (i = 0; i < l->count; i++) {
			free(*(void **)entry);
			entry += l->size;
		}
	}
	l->count = 0;
}

int list_set_free (list_t *l, int free) {
	if (l->count) {
		return -1;
	}
	l->free = free;
	return 0;
}

void *list_append (list_t *l) {
	void   *entries_new;
	size_t  alloc_new;

	if (l->count == l->alloc) {
		alloc_new = l->alloc << 1;
		if (!list_check_alloc(l->size, alloc_new)) {
			return NULL;
		}
		entries_new = realloc(l->entries, alloc_new * l->size);
		if (!entries_new) {
			return NULL;
		}
		memset((char *)entries_new + l->count * l->size, 0, (alloc_new - l->count) * l->size);
		l->alloc = alloc_new;
		l->entries = entries_new;
	}
	return (char *)l->entries + l->count++ * l->size;
}

void *list_get (list_t *l, size_t index) {
	return (char *)l->entries + index * l->size;
}

void *list_pop (list_t *l) {
	if (l->count == 0) {
		return NULL;
	}
	return (char *)l->entries + --l->count * l->size;
}

static int list_check_alloc (size_t size, size_t alloc) {
	return size > 0 && alloc > 0 && alloc <= SIZE_MAX / size;
}

static size_t list_alloc (size_t alloc) {
	/* find the next power of 2 */
	alloc--;
	alloc |= alloc >> 1;
	alloc |= alloc >> 2;
	alloc |= alloc >> 4;
	alloc |= alloc >> 8;
	alloc |= alloc >> 16;
	alloc |= alloc >> 32;
	alloc++;
	return alloc;
}	

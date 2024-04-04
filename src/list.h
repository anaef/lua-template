/*
 * List
 *
 * Copyright (C) 2024 Andre Naef
 */


#ifndef _LIST_INCLUDED
#define _LIST_INCLUDED


#include <stddef.h>
#include <stdint.h>


typedef struct list_s list_t;

struct list_s {
	size_t          size;     /* entry size */
	size_t          alloc;    /* allocated entries */
	size_t          count;    /* number of entries */
	void           *entries;  /* entries */
	unsigned        free:1;   /* free entries on free/clear */
};


list_t *list_create(size_t size, size_t alloc);
void list_free(list_t *t);
void list_clear(list_t *l);
int list_set_free(list_t *l, int free);
void *list_append(list_t *l);
void *list_get(list_t *l, size_t index);
void *list_pop(list_t *l);


#endif /* _LIST_INCLUDED */

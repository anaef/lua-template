/*
 * Table
 *
 * Copyright (C) 2023-2024 Andre Naef
 */


#ifndef _TABLE_INCLUDED
#define _TABLE_INCLUDED


#include <stddef.h>
#include <stdint.h>


typedef struct table_s table_t;
typedef struct table_entry_s table_entry_t;

struct table_s {
	size_t          alloc;     /* allocated slots */
	size_t          load;      /* load limit for rehash */
	size_t          count;     /* number of entries */
	table_entry_t  *entries;   /* entries */
	unsigned        dup:1;     /* duplicate keys */
	unsigned        free:1;    /* free values */
	unsigned        ci:1;      /* case insensitive */
};

typedef enum {
	TES_UNUSED,
	TES_SET,
	TES_DELETED
} table_entry_state_e;

struct table_entry_s {
	const char          *key;    /* key; managed if dup is set */
	void                *value;  /* value; managed if free is set */
	uint64_t             hash;   /* key hash */
	table_entry_state_e  state;  /* state [unused, set, deleted] */
};


table_t *table_create(size_t load);
void table_free(table_t *t);
void table_clear(table_t *t);
int table_set_dup(table_t *t, int dup);
int table_set_free(table_t *t, int free);
int table_set_ci(table_t *t, int ci);
void *table_get(table_t *t, const char *key);
int table_set(table_t *t, const char *key, void *value);


#endif /* _TABLE_INCLUDED */

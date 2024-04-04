/*
 * Table
 *
 * Copyright (C) 2023-2024 Andre Naef
 */


#include "table.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


static uint64_t table_hash(table_t *t, const char *key);
static size_t table_size(size_t size);
static size_t table_load(size_t alloc);
static int table_rehash(table_t *t, size_t alloc);
static table_entry_t *table_find(table_t *t, const char *key, uint64_t hash);
static table_entry_t *table_insert(table_t *t, uint64_t hash);
static void table_remove(table_t *t, table_entry_t *entry);


static size_t table_sizes[] = {
	3, 5, 7, 11, 13, 17, 23, 29, 41, 53, 67, 89, 127, 157, 211, 277, 373, 499, 659, 877, 1171,
	1553, 2081, 2767, 3691, 4909, 6547, 8731, 11633, 15511, 20681, 27581, 36749, 49003, 65353,
	87107, 116141, 154871, 206477, 275299, 367069, 489427, 652559, 870083, 1160111, 1546799,
	2062391, 2749847, 3666461, 4888619, 6518173, 8690917, 11587841, 15450437, 20600597,
	27467443, 36623261, 48831017, 65107997, 86810681, 115747549, 154330079, 205773427,
	274364561, 365819417, 487759219, 650345651, 867127501, 1156170011, 1541560037, 2055413317,
	2740551103, 3654068141, 4872090871, 6496121063, 8661494753, 11548659701, 15398212901,
	20530950533, 27374600677, 36499467569, 48665956771, 64887942367, 86517256433,
	115356341911, 153808455923, 205077941191, 273437254897, 364583006561, 486110675443,
	648147567293, 864196756231, 1152262341641, 1536349788871, 2048466385123, 2731288513529,
	3641718017983, 4855624023953, 6474165365293, 8632220487029, 11509627316059,
	15346169754719, 20461559672951, 27282079563967, 36376106085223, 48501474780299,
	64668633040457, 86224844053847, 114966458738489, 153288611651291, 204384815535079,
	272513087380099
};
static int table_sizes_n = 112;


table_t *table_create (size_t load) {
	size_t    alloc;
	table_t  *t;

	t = calloc(1, sizeof(table_t));
	if (!t) {
		return NULL;
	}
	alloc = load + (load < table_sizes[table_sizes_n - 1] ? load / 7 + 3 : 0);
	if ((t->alloc = table_size(alloc)) == (size_t)-1) {
		free(t);
		return NULL;
	}
	t->load = table_load(t->alloc);  /* t->load >= load */
	t->entries = calloc(t->alloc, sizeof(table_entry_t));
	if (!t->entries) {
		free(t);
		return NULL;
	}
	return t;
}

void table_free (table_t *t) {
	table_entry_t  *entry, *sentinel;

	if (t->dup || t->free) {
		sentinel = t->entries + t->alloc;
		for (entry = t->entries; entry < sentinel; entry++) {
			if (entry->state == TES_SET) {
				table_remove(t, entry);
			}
		}
	}
	free(t->entries);
	free(t);
}

void table_clear (table_t *t) {
	table_entry_t  *entry, *sentinel;

	if (t->dup || t->free) {
		sentinel = t->entries + t->alloc;
		for (entry = t->entries; entry < sentinel; entry++) {
			if (entry->state == TES_SET) {
				table_remove(t, entry);
			}
		}
	} else {
		t->count = 0;
	}
	memset(t->entries, 0, t->alloc * sizeof(table_entry_t));
}

int table_set_dup (table_t *t, int dup) {
	if (t->count) {
		return -1;
	}
	t->dup = dup;
	return 0;
}

int table_set_free (table_t *t, int free) {
	if (t->count) {
		return -1;
	}
	t->free = free;
	return 0;
}

int table_set_ci (table_t *t, int ci) {
	if (t->count) {
		return -1;
	}
	t->ci = ci;
	return 0;
}

void *table_get (table_t *t, const char *key) {
	uint64_t        hash;
	table_entry_t  *entry;

	hash = table_hash(t, key);
	entry = table_find(t, key, hash);
	if (!entry) {
		return NULL;
	}
	return entry->value;
}

int table_set (table_t *t, const char *key, void *value) {
	uint64_t        hash;
	table_entry_t  *entry;

	hash = table_hash(t, key);
	entry = table_find(t, key, hash);
	if (value) {
		if (entry) {
			/* update existing */
			if (t->free && value != entry->value) {
				free(entry->value);
			}
			entry->value = value;
		} else {
			/* rehash as needed */
			if (t->count == t->load) {
				if (table_rehash(t, t->alloc + 1) != 0) {
					return -1;
				}
			}

			/* new entry */
			entry = table_insert(t, hash);
			if (t->dup) {
				entry->key = strdup(key);
				if (!entry->key) {
					return -1;
				}
			} else {
				entry->key = key;
			}
			entry->value = value;
			entry->hash = hash;
			entry->state = TES_SET;
			t->count++;
		}
	} else {
		/* remove */
		if (entry) {
			table_remove(t, entry);
		}
	}
	return 0;
}

static uint64_t table_hash (table_t *t, const char *key) {
	uint64_t     hash;
	const char  *p;

	/* FNV-1a; source: http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-1a */
	hash = 14695981039346656037U;
	p = key + strlen(key);
	if (t->ci) {
		while (p > key) {
			hash ^= tolower(*--p);
			hash *= 1099511628211;
		}
	} else {
		while (p > key) {
			hash ^= *--p;
			hash *= 1099511628211;
		}
	}
	return hash;
}

static size_t table_size (size_t size) {
	int  lower, upper, mid;

	lower = 0;
	upper = table_sizes_n - 1;
	while (lower <= upper) {
		mid = (lower + upper) / 2;
		if (table_sizes[mid] < size) {
			lower = mid + 1;
		} else {
			upper = mid - 1;
		}
	}
	if (lower >= table_sizes_n) {
		return (size_t)-1;
	}
	return table_sizes[lower];
}

static size_t table_load (size_t alloc) {
	return (alloc >> 1) + (alloc >> 2) + (alloc >> 3);  /* ~87.5 percent */
}

static int table_rehash (table_t *t, size_t alloc) {
	size_t          alloc_new;
	table_entry_t  *entries, *entry, *entry_sentinel, *entries_new, *entry_new;

	/* allocate */
	if ((alloc_new = table_size(alloc)) == (size_t)-1) {
		return -1;
	}
	entries_new = calloc(alloc_new, sizeof(table_entry_t));
	if (!entries_new) {
		return -1;
	}

	/* update table */
	entries = t->entries;
	entry_sentinel = entries + t->alloc;
	t->alloc = alloc_new;
	t->load = table_load(t->alloc);
	t->entries = entries_new;

	/* reinsert */
	for (entry = entries; entry < entry_sentinel; entry++) {
		if (entry->state == TES_SET) {
			entry_new = table_insert(t, entry->hash);
			*entry_new = *entry;
		}
	}
	free(entries);
	return 0;
}

static table_entry_t *table_find (table_t *t, const char *key, uint64_t hash) {
	size_t          r, h, q;
	table_entry_t  *entry;

	/* Brent's method; source: https://maths-people.anu.edu.au/~brent/pd/rpb013.pdf */
	r = h = hash % t->alloc;
	q = hash % (t->alloc - 2) + 1;
	entry = &t->entries[h];
	while (entry->state != TES_UNUSED) {
		if (entry->hash == hash && (t->ci ? strcasecmp(entry->key, key)
				: strcmp(entry->key, key)) == 0) {
			return entry;
		}
		h = (h + q) % t->alloc;
		if (h == r) {
			break;
		}
		entry = &t->entries[h];
	}
	return NULL;
}

static table_entry_t *table_insert (table_t *t, uint64_t hash) {
	size_t          h, q, len_worst, len, q_move, h_move, len_move, len_entry_move;
	table_entry_t  *entry, *entry_move, *entry_move_old, *entry_move_new;

	/* Brent's method; source: https://maths-people.anu.edu.au/~brent/pd/rpb013.pdf */
	/* determine worst case */
	h = hash % t->alloc;
	q = hash % (t->alloc - 2) + 1;
	entry = &t->entries[h];
	len_worst = 1;
	while (entry->state == TES_SET) {
		h = (h + q) % t->alloc;
		entry = &t->entries[h];
		len_worst++;
	}
	if (len_worst <= 2) {
		/* "worst" case is optimal overall */
		return entry;
	}

	/* check for a better overall outcome by moving conflicting entries */
	entry_move_old = entry_move_new = NULL;
	len_entry_move = (size_t)-1;
	h = hash % t->alloc;
	entry = &t->entries[h];
	len = 1;
	do {
		q_move = entry->hash % (t->alloc - 2) + 1;
		h_move = (h + q_move) % t->alloc;
		entry_move = &t->entries[h_move];
		len_move = 1;
		while (1) {
			if (entry_move->state != TES_SET) {
				/* we found a better overall outcome */
				entry_move_old = entry;
				entry_move_new = entry_move;
				len_entry_move = len_move;
				break;
			}
			if (len + len_move >= len_worst - 1) {  /* zero-sum at best otherwise */
				break;
			}
			h_move = (h_move + q_move) % t->alloc;
			entry_move = &t->entries[h_move];
			len_move++;
		}
		h = (h + q) % t->alloc;
		entry = &t->entries[h];
		len++;
	} while (len < len_worst - 1 && len < len_entry_move);  /* zero-sum at best otherwise */

	/* move if a better overall outcome was found */
	if (entry_move_old) {  /* implied if len < len_entry_move is false */
		*entry_move_new = *entry_move_old;
		return entry_move_old;
	}  /* len is invariably len_worst - 1 at this point */

	/* cannot do better than the worst case */
	h = (h + q) % t->alloc;
	entry = &t->entries[h];
	return entry;
}

static void table_remove (table_t *t, table_entry_t *entry) {
	if (t->dup) {
		free((void *)entry->key);
	}
	if (t->free) {
		free(entry->value);
	}
	entry->state = TES_DELETED;
	t->count--;
}

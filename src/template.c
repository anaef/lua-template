/*
 * Template
 *
 * Copyright (C) 2011-2024 Andre Naef
 */


#include "template.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <lauxlib.h>
#include "table.h"
#include "list.h"


#define TEMPLATE_EOPEN      1     /* opening element */
#define TEMPLATE_ECLOSE     2     /* closing element */

#define TEMPLATE_FESC       0xff  /* escape mask */
#define TEMPLATE_FESCXML    1     /* 'x'; flag to escape XML/HTML characters */
#define TEMPLATE_FESCURL    2     /* 'u'; flag to escape URL characters */
#define TEMPLATE_FESCJS     3     /* 'j'; flag to escape JavaScript string characters */
#define TEMPLATE_FSUPNIL    256   /* 'n'; flag to suppress nil values */

#define TEMPLATE_MAX_STACK  1024  /* maximum expression length allocated on stack */
#define TEMPLATE_MAX_DEPTH  8     /* maximum template inclusion depth */


typedef struct template_s template_t;
typedef struct parser_s parser_t;
typedef struct node_s node_t;
typedef struct block_s block_t;
typedef struct memstream_s memstream_t;

struct template_s {
	char        *str;    /* template contents */
	list_t      *nodes;  /* list of template nodes */
	const void  *env;    /* environment */
};

struct parser_s {
	const char  *filename;  /* filename */
	lua_State   *L;         /* Lua state */
	char        *str;       /* template contents */
	char        *begin;     /* begin of raw content */
	char        *pos;       /* parsing position */
	int          element;   /* current element flags */
	table_t     *attrs;     /* current element attributes */
	list_t      *nodes;     /* list of template nodes */
	list_t      *blocks;    /* block stack (if, for) */
};

typedef enum {
	NT_NONE,
	NT_JUMP,
	NT_IF,
	NT_FOR_INIT,
	NT_FOR_NEXT,
	NT_SET,
	NT_INCLUDE,
	NT_SUB,
	NT_RAW,
} node_type_e;

struct node_s {
	node_type_e      type;            /* node type */
	union {
		struct {
			off_t    jump_next;       /* node index to jump to */
		};
		struct {
			int      if_ref;          /* condition expression reference */
			off_t    if_next;         /* node index to jump to if condition is false */
		};
		struct {
			int      for_init_ref;    /* init expression reference */
		};
		struct {
			list_t  *for_next_names;  /* list of names */
			off_t    for_next_next;   /* node index to jump to when iteration ends */
		};
		struct {
			list_t  *set_names;       /* list of names*/
			int      set_ref;         /* set expression reference */
		};
		struct {
			int      include_ref;     /* include filename reference */
		};
		struct {
			int      sub_ref;         /* substitution expression reference */
			int      sub_flags;       /* substitution flags */
		};
		struct {
			char    *raw_str;         /* raw string */
			size_t   raw_len;         /* raw length */
		};
	};
};

struct block_s {
	node_type_e     type;       /* block type (NT_IF, NT_FOR_NEXT) */
	union {
		struct {
			off_t   if_start;   /* first if condition node index */
			off_t   if_last;    /* trailing if condition node index; -1 if 'else' was found */
			size_t  if_count;   /* number of 'elseif' and 'else' elements */
		};
		struct {
			off_t   for_start;  /* for-next node index */
		};
	};
};

struct memstream_s {
	luaL_Stream  stream;  /* Lua stream */
	char        *str;     /* memory buffer */
	size_t       len;     /* size of buffer */ 
};


/* parsing */
static void template_unescape_xml(char *str);
static int template_error(parser_t *p, const char *msg);
static int template_oom(parser_t *p);
static node_t *template_append_node(parser_t *p);
static block_t *template_append_block(parser_t *p);
static int template_parse_flags(parser_t *p, const char *flags);
static list_t *template_parse_names(parser_t *p, char *names);
static int template_parse_expression(parser_t *p, const char *exp);
static void template_parse_if(parser_t *p);
static void template_parse_elseif(parser_t *p);
static void template_parse_else(parser_t *p);
static void template_parse_for(parser_t *p);
static void template_parse_set(parser_t *p);
static void template_parse_include(parser_t *p);
static void template_parse_element(parser_t *p);
static void template_parse_sub(parser_t *p);
static void template_parse_raw(parser_t *p);
static void template_resolve(parser_t *p);
static int template_parse(lua_State *L);
static void template_nodes_free(lua_State *L, list_t *nodes);
static int template_parser_gc(lua_State *L);
static int template_tostring(lua_State *L);
static int template_gc(lua_State *L);

/* rendering */
static void template_eval(lua_State *L, int index, int nret);
static void template_eval_str(lua_State *L, int index);
static void template_setenv(lua_State *L, template_t *t);
static void template_render_template(lua_State *L, FILE *f, const char *filename, int depth);
static int template_fclose(lua_State *L);
static int template_render(lua_State *L);

/* library */
static int template_getresolver(lua_State *L);
static int template_setresolver(lua_State *L);
static int template_clear(lua_State *L);


static char template_hex_digits[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};


/*
 * parsing
 */

static void template_unescape_xml (char *str) {
	char  *r, *w;

	r = str;
	w = str;
	while (*r != '\0') {
		if (*r == '&') {
			if (r[1] == 'q' && r[2] == 'u' && r[3] == 'o' && r[4] == 't' && r[5] == ';') {
				*w++ = '"';
				r += 6;
			} else if (r[1] == 'l' && r[2] == 't' && r[3] == ';') {
				*w++ = '<';
				r += 4;
			} else if (r[1] == 'g' && r[2] == 't' && r[3] == ';') {
				*w++ = '>';
				r += 4;
			} else if (r[1] == 'a' && r[2] == 'm' && r[3] == 'p' && r[4] == ';') {
				*w++ = '&';
				r += 5;
			} else {
				*w++ = *r++;
			}	
		} else {
			*w++ = *r++;
		}
	}
	*w = '\0';
}

static int template_error (parser_t *p, const char *msg) {
	int    line;
	char  *pos;

	pos = p->str;
	line = 1;
	while (pos < p->pos) {
		switch (*pos) {
		case '\n':
			line++;
			pos++;
			break;

		case '\r':
			line++;
			pos++;
			if (*pos == '\n') {
				pos++;
			}
			break;

		default:
			pos++;
		}
	}
	return luaL_error(p->L, "%s:%d: %s", p->filename, line, msg);
}

static int template_oom (parser_t *p) {
	return template_error(p, "out of memory");
}

static node_t *template_append_node (parser_t *p) {
	node_t  *node;

	node = list_append(p->nodes);
	if (!node) {
		template_oom(p);
	}
	return node;
}

static block_t *template_append_block (parser_t *p) {
	block_t  *block;

	block = list_append(p->blocks);
	if (!block) {
		template_oom(p);
	}
	return block;
}

static int template_parse_flags (parser_t *p, const char *flags) {
	int          value;
	const char  *f;

	value = 0;
	for (f = flags; *f != '\0'; f++) {
		switch (*f) {
		case 'x':
			if (value & TEMPLATE_FESC) {
				template_error(p, "bad flags: multiple escacpes");
			}
			value |= TEMPLATE_FESCXML;
			break;

		case 'u':
			if (value & TEMPLATE_FESC) {
				template_error(p, "bad flags: multiple escacpes");
			}
			value |= TEMPLATE_FESCURL;
			break;

		case 'j':
			if (value & TEMPLATE_FESC) {
				template_error(p, "bad flags: multiple escacpes");
			}
			value |= TEMPLATE_FESCJS;
			break;

		case 'n':
			value |= TEMPLATE_FSUPNIL;
			break;

		default:
			template_error(p, "bad flags: unknown character");
		}
	}
	return value;
}

static list_t *template_parse_names (parser_t *p, char *names) {
	char    *name, *state, **entry;
	list_t  *l;

	l = list_create(sizeof(char *), 2);
	if (!l) {
		template_oom(p);
	}
	state = NULL;
	name = strtok_r(names, "\t ,", &state);
	while (name != NULL) {
		entry = list_append(l);
		if (!entry) {
			list_free(l);
			template_oom(p);
		}
		*entry = name;
		name = strtok_r(NULL, "\t ,", &state);		
	}
	if (l->count == 0) {
		list_free(l);
		template_error(p, "empty 'names'");
	}
	return l;
}

static int template_parse_expression (parser_t *p, const char *exp) {
	int     on_stack;
	char   *chunk;
	size_t  len;

	len = strlen(exp);
	on_stack = sizeof("return ") - 1 + len <= TEMPLATE_MAX_STACK;
	if (on_stack) {
		chunk = alloca(sizeof("return ") - 1 + len);
	} else {
		chunk = malloc(sizeof("return ") - 1 + len);
		if (!chunk) {
			return template_oom(p);
		}
	}
	memcpy(chunk, "return ", sizeof("return ") - 1);
	memcpy(chunk + sizeof("return ") - 1, exp, len);
	if (luaL_loadbufferx(p->L, chunk, sizeof("return ") - 1 + len, exp, "t") != LUA_OK) {
		if (!on_stack) {
			free(chunk);
		}
		return template_error(p, lua_tostring(p->L, -1));
	}
	if (!on_stack) {
		free(chunk);
	}
	return luaL_ref(p->L, LUA_REGISTRYINDEX);
}

static void template_parse_if (parser_t *p) {
	char     *cond;
	node_t   *node;
	block_t  *block;

	if ((p->element & TEMPLATE_EOPEN) != 0) {
		block = template_append_block(p);
		block->type = NT_IF;
		block->if_start = p->nodes->count;
		block->if_last = p->nodes->count;
		block->if_count = 0;
		node = template_append_node(p);
		node->type = NT_IF;
		node->if_ref = LUA_NOREF;
		cond = table_get(p->attrs, "cond");
		if (cond == NULL) {
			template_error(p, "missing attribute 'cond'");
		}
		node->if_ref = template_parse_expression(p, cond);
		node->if_next = -1;
	}	
	if ((p->element & TEMPLATE_ECLOSE) != 0) {
		block = list_pop(p->blocks);
		if (block == NULL || block->type != NT_IF) {
			template_error(p, "no 'if' to close");
		}
		if (block->if_last != -1) {
			node = list_get(p->nodes, block->if_last);
			node->if_next = p->nodes->count;
		}
		node = list_get(p->nodes, block->if_start);
		while (block->if_count > 0) {
			node = list_get(p->nodes, node->if_next);
			(node - 1)->jump_next = p->nodes->count;
			block->if_count--;
		}
	}
}

static void template_parse_elseif (parser_t *p) {
	char     *cond;
	node_t   *node;
	block_t  *block;

	if ((p->element & TEMPLATE_EOPEN) != 0) {
		if (p->blocks->count == 0) {
			template_error(p, "no 'if' to continue");
		}
		block = list_get(p->blocks, p->blocks->count - 1);
		if (block->type != NT_IF || block->if_last == -1) {
			template_error(p, "no 'if' to continue");
		}
		node = template_append_node(p);
		node->type = NT_JUMP;
		node->jump_next = -1;
		block->if_count++;
		node = list_get(p->nodes, block->if_last);
		node->if_next = p->nodes->count;
		block->if_last = p->nodes->count;
		node = template_append_node(p);
		node->type = NT_IF;
		node->if_ref = LUA_NOREF;
		cond = table_get(p->attrs, "cond");
		if (cond == NULL) {
			template_error(p, "missing attribute 'cond'");
		}
		node->if_ref = template_parse_expression(p, cond);
		node->if_next = -1;
	}
}

static void template_parse_else (parser_t *p) {
 	node_t   *node;
	block_t  *block;

	if ((p->element & TEMPLATE_EOPEN) != 0) {
		if (p->blocks->count == 0) {
			template_error(p, "no 'if' to continue");
		}
		block = list_get(p->blocks, p->blocks->count - 1);
		if (block->type != NT_IF || block->if_last == -1) {
			template_error(p, "no 'if' to continue");
		}
		node = template_append_node(p);
		node->type = NT_JUMP;
		node->jump_next = -1;
		block->if_count++;
		node = list_get(p->nodes, block->if_last);
		node->if_next = p->nodes->count;
		block->if_last = -1;
	}
} 

static void template_parse_for (parser_t *p) {
	char     *in, *names;
	node_t   *node;
	block_t  *block;

	if ((p->element & TEMPLATE_EOPEN) != 0) {
		node = template_append_node(p);
		node->type = NT_FOR_INIT;
		node->for_init_ref = LUA_NOREF;
		in = table_get(p->attrs, "in");
		if (in == NULL) {
			template_error(p, "missing attribute 'in'");
		}
		node->for_init_ref = template_parse_expression(p, in);
		block = template_append_block(p);
		block->type = NT_FOR_NEXT;
		block->for_start = p->nodes->count;
		node = template_append_node(p);
		node->type = NT_FOR_NEXT;
		names = table_get(p->attrs, "names");
		if (names == NULL) {
			template_error(p, "missing attribute 'names'");
		}
		node->for_next_names = template_parse_names(p, (char *)names);
		node->for_next_next = -1;
	}
	if ((p->element & TEMPLATE_ECLOSE) != 0) {
		block = list_pop(p->blocks);
		if (block == NULL || block->type != NT_FOR_NEXT) {
				template_error(p, "no 'for' to close");
		}
		node = template_append_node(p);
		node->type = NT_JUMP;
		node->jump_next = block->for_start;
		node = list_get(p->nodes, block->for_start);
		node->for_next_next = p->nodes->count;
	}
}

static void template_parse_set (parser_t *p) {
	char    *names, *expressions;
	node_t  *node;

	if ((p->element & TEMPLATE_EOPEN) != 0) {
		node = template_append_node(p);
		node->type = NT_SET;
		node->set_ref = LUA_NOREF;
		names = table_get(p->attrs, "names");
		if (names == NULL) {
				template_error(p, "missing attribute 'names'");
		}
		node->set_names = template_parse_names(p, (char *)names);
		expressions = table_get(p->attrs, "expressions");
		if (expressions == NULL) {
				template_error(p, "missing attribute 'expressions'");
		}
		node->set_ref = template_parse_expression(p, expressions);
	}
}

static void template_parse_include (parser_t *p) {
	char    *filename;
	node_t  *node;

	if ((p->element & TEMPLATE_EOPEN) != 0) {
		node = template_append_node(p);
		node->type = NT_INCLUDE;
		node->include_ref = LUA_NOREF;
		filename = table_get(p->attrs, "filename");
		if (filename == NULL) {
			template_error(p, "missing attribute 'filename'");
		}
		node->include_ref = template_parse_expression(p, filename);
	}
}

static void template_parse_element (parser_t *p) {
	char  *element, *element_end, *key, *key_end, *val, *val_end;

	p->pos++;
	if (*p->pos == '/') {
		p->element = TEMPLATE_ECLOSE;
		p->pos++;
	} else {
		p->element = TEMPLATE_EOPEN;
	}
	p->pos += 2;
	element = p->pos;
	while (!isspace(*p->pos) && *p->pos != '>' && *p->pos != '/' && *p->pos != '\0') {
		p->pos++;
	}
	element_end = p->pos;
	while (isspace(*p->pos)) {
		p->pos++;
	}
	table_clear(p->attrs);
	while (*p->pos != '>' && *p->pos != '/' && *p->pos != '\0') {
		key = p->pos;
		while (!isspace(*p->pos) && *p->pos != '=' && *p->pos != '\0') {
			p->pos++;
		}
		if (p->pos == key) {
			template_error(p, "attribute name expected");
		}
		key_end = p->pos;
		while (isspace(*p->pos)) {
			p->pos++;
		}
		if (*p->pos != '=') {
			template_error(p, "'=' expected");
		}
		p->pos++;
		while (isspace(*p->pos)) {
			p->pos++;
		}
		if (*p->pos != '"') {
			template_error(p, "'\"' expected");
		}
		p->pos++;
		val = p->pos;
		while (*p->pos != '"' && *p->pos != '\0') {
			p->pos++;
		}
		val_end = p->pos;
		if (*p->pos != '"') {
			template_error(p, "'\"' expected");
		}
		p->pos++;
		*key_end = '\0';
		template_unescape_xml(key);
		*val_end = '\0';
		template_unescape_xml(val);
		table_set(p->attrs, key, val);
		while (isspace(*p->pos)) {
			p->pos++;
		}
	}
	if (*p->pos == '/') {
		p->element |= TEMPLATE_ECLOSE;
		p->pos++;
	}
	if (*p->pos != '>') {
		template_error(p, "'>' expected");
	}
	p->pos++;
	switch (element_end - element) {
	case 2:
		if (strncmp(element, "if", 2) == 0) {
			template_parse_if(p);
		} else {
			template_error(p, "bad element");
		}
		break;

	case 3:
		if (strncmp(element, "for", 3) == 0) {
			template_parse_for(p);
		} else if (strncmp(element, "set", 3) == 0) {
			template_parse_set(p);
		} else {
			template_error(p, "bad element");
		}
		break;

	case 4:
		if (strncmp(element, "else", 4) == 0) {
			template_parse_else(p);
		} else {
			template_error(p, "bad element");
		}
		break;

	case 6:
		if (strncmp(element, "elseif", 6) == 0) {
			template_parse_elseif(p);
		} else {
			template_error(p, "bad element");
		}
		break;

	case 7:
		if (strncmp(element, "include", 7) == 0) {
			template_parse_include(p);
		} else {
			template_error(p, "bad element");
		}
		break;

	default:
		template_error(p, "bad element");
	}
}	

static void template_parse_sub (parser_t *p) {
	int      braces, quot;
	char    *flags, *expression;
	node_t  *node;

	node = template_append_node(p);	
	node->type = NT_SUB;
	node->sub_ref = LUA_NOREF;
	p->pos++;

	/* optional flags */
	if (*p->pos == '[') {
		p->pos++;
		flags = p->pos;
		while (*p->pos != ']' && *p->pos != '\0') {
			p->pos++;
		}
		if (*p->pos != ']') {
			template_error(p, "']' expected");
		}
		*p->pos = '\0';
		node->sub_flags = template_parse_flags(p, flags);
		p->pos++;
	} else {
		node->sub_flags = TEMPLATE_FESCXML;
	}

	/* expression */
	if (*p->pos != '{') {
		template_error(p, "'{' expected");
	}
	braces = 1;
	quot = 0;
	p->pos++;
	expression = p->pos;
	while (*p->pos != '\0' && braces > 0) {
		switch (*p->pos) {
		case '{':
			if (!quot) {
				braces++;
			}
			break;

		case '}':
			if (!quot) {	
				braces--;
			}
			break;
		
		case '"':
			switch (quot) {
			case 0:
				quot = 1;
				break;

			case 1:
				quot = 0;
				break;
			}
			break;

		case '\'':
			switch (quot) {
			case 0:
				quot = 2;
				break;

			case 2:
				quot = 0;
				break;
			}
			break;

		case '\\':
			switch (quot) {
			case 1:
				if (p->pos[1] == '"') {
					p->pos++;
				}
				break;

			case 2:
				if (p->pos[1] == '\'') {
					p->pos++;
				}
				break;
			}
			break;
		}
		p->pos++;
	}
	if (braces > 0) {
		template_error(p, "'}' expected");
	}
	*(p->pos - 1) = '\0';
	template_unescape_xml(expression);
	node->sub_ref = template_parse_expression(p, expression);
}

static void template_parse_raw (parser_t *p) {
	node_t  *node;

	if (p->pos > p->begin) {
		node = template_append_node(p);
		node->type = NT_RAW;
		node->raw_str = p->begin;
		node->raw_len = p->pos - p->begin;
	}
}

static void template_resolve (parser_t *p) {
	FILE        *f;
	struct stat  statbuf;

	if (stat(p->filename, &statbuf) != 0) {
		luaL_error(p->L, "%s: template not found", p->filename);
	}
	if (!(p->str = malloc(statbuf.st_size + 1))) {
		luaL_error(p->L, "%s: out of memory", p->filename);
	}
	if (!(f = fopen(p->filename, "r"))) {
		luaL_error(p->L, "%s: error opening template", p->filename);
	}
	if (fread(p->str, 1, statbuf.st_size, f) != (size_t)statbuf.st_size) {
		fclose(f);
		luaL_error(p->L, "%s: error reading template", p->filename);
	}
	if (fclose(f) != 0) {
		luaL_error(p->L, "%s: error closing template", p->filename);
	}
	p->str[statbuf.st_size] = '\0';
}

static int template_parse (lua_State *L) {
	size_t       len;
	parser_t    *p;
	template_t  *t;
	const char  *str;

	/* push parser */
	p = lua_newuserdata(L, sizeof(parser_t));
	memset(p, 0, sizeof(parser_t));
	luaL_setmetatable(L, TEMPLATE_PARSER);
	p->filename = luaL_checkstring(L, 1);
	p->L = L;
	p->attrs = table_create(4);
	p->nodes = list_create(sizeof(node_t), 32);
	p->blocks = list_create(sizeof(block_t), 8);
	if (!p->attrs || !p->nodes || !p->blocks) {
		return luaL_error(L, "error allocating parser");
	}

	/* resolve template */
	if (lua_getfield(L, LUA_REGISTRYINDEX, TEMPLATE_RESOLVER) == LUA_TNIL) {
		/* default file system resolver */
		template_resolve(p);
	} else {
		/* custom resolver */
		lua_pushvalue(L, 1);
		lua_call(L, 1, 1);
		if (!lua_isstring(L, -1)) {
			return luaL_error(L, "%s: error resolving template", p->filename);
		}
		str = lua_tolstring(L, -1, &len);
		p->str = malloc(len + 1);
		if (!p->str) {
			return luaL_error(L, "%s: out of memory", p->filename);
		}
		memcpy(p->str, str, len + 1);
	}
	lua_pop(L, 1);

	/* process elements and substitution, treat all else as raw */
	p->pos = p->str;
	p->begin = p->pos;
	while (*p->pos != '\0') {
		switch (*p->pos) {
		case '<':
			if ((p->pos[1] == 'l' && p->pos[2] == ':')
					|| (p->pos[1] == '/' && p->pos[2] == 'l' && p->pos[3] == ':')) {
				template_parse_raw(p);
				template_parse_element(p);
				p->begin = p->pos;
			} else {
				p->pos++;
			}
			break;

		case '$':
			switch (p->pos[1]) {
			case '{':
			case '[':
				template_parse_raw(p);
				template_parse_sub(p);
				p->begin = p->pos;
				break;

			case '$':
				p->pos++;
				template_parse_raw(p);
				p->pos++;
				p->begin = p->pos;
				break;

			default:
				p->pos++;
			}
			break;

		default:
			p->pos++;
		}
	} 
	template_parse_raw(p);
	if (p->blocks->count > 0) {
		return luaL_error(L, "%s: %d open element(s) at end of template", p->filename,
				p->blocks->count);
	}

	/* return parsed template */
	t = lua_newuserdata(L, sizeof(template_t));
	memset(t, 0, sizeof(template_t));
	luaL_setmetatable(L, TEMPLATE_TEMPLATE);
	t->str = p->str;
	p->str = NULL;
	t->nodes = p->nodes;
	p->nodes = NULL;
	return 1;
};

static void template_nodes_free (lua_State *L, list_t *nodes) {
	node_t  *node;
	size_t   i;

	for (i = 0; i < nodes->count; i++) {
		node = list_get(nodes, i);
		switch (node->type) {
		case NT_NONE:
		case NT_JUMP:
		case NT_RAW:
			break;

		case NT_IF:
			luaL_unref(L, LUA_REGISTRYINDEX, node->if_ref);
			break;

		case NT_FOR_INIT:
			luaL_unref(L, LUA_REGISTRYINDEX, node->for_init_ref);
			break;

		case NT_FOR_NEXT:
			if (node->for_next_names) {
				list_free(node->for_next_names);
			}
			break;

		case NT_SET:
			if (node->set_names) {
				list_free(node->set_names);
			}
			luaL_unref(L, LUA_REGISTRYINDEX, node->set_ref);
			break;

		case NT_INCLUDE:
			luaL_unref(L, LUA_REGISTRYINDEX, node->include_ref);
			break;

		case NT_SUB:
			luaL_unref(L, LUA_REGISTRYINDEX, node->sub_ref);
			break;
		}
	}
	list_free(nodes);
}

static int template_parser_gc (lua_State *L) {
	parser_t  *p;

	p = luaL_checkudata(L, 1, TEMPLATE_PARSER);
	if (p->attrs) {
		table_free(p->attrs);
	}
	if (p->nodes) {
		template_nodes_free(L, p->nodes);
	}
	if (p->blocks) {
		list_free(p->blocks);
	}
	free(p->str);
	return 0;
}

static int template_tostring (lua_State *L) {
	template_t  *t;

	t = luaL_checkudata(L, 1, TEMPLATE_TEMPLATE);
	lua_pushfstring(L, TEMPLATE_TEMPLATE ": %p", t);
	return 1;
}

static int template_gc (lua_State *L) {
	template_t  *t;

	t = luaL_checkudata(L, 1, TEMPLATE_TEMPLATE);
	if (t->nodes) {
		template_nodes_free(L, t->nodes);
	}
	free(t->str);
	return 0;
}


/*
 * rendering
 */

static void template_eval (lua_State *L, int index, int nret) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, index);
	lua_call(L, 0, nret);
} 

static void template_eval_str (lua_State *L, int index) {
	template_eval(L, index, 1);
	if (!lua_isstring(L, -1)) {
		lua_pushfstring(L, "(%s)", luaL_typename(L, -1));
		lua_replace(L, -2);
	}
}

static void template_setenv (lua_State *L, template_t *t) {
	node_t  *node;
	size_t   i;

	for (i = 0; i < t->nodes->count; i++) {
		node = list_get(t->nodes, i);
		switch (node->type) {
		case NT_IF:
			lua_rawgeti(L, LUA_REGISTRYINDEX, node->if_ref);
			break;

		case NT_FOR_INIT:
			lua_rawgeti(L, LUA_REGISTRYINDEX, node->for_init_ref);
			break;

		case NT_SET:
			lua_rawgeti(L, LUA_REGISTRYINDEX, node->set_ref);
			break;

		case NT_INCLUDE:
			lua_rawgeti(L, LUA_REGISTRYINDEX, node->include_ref);
			break;

		case NT_SUB:
			lua_rawgeti(L, LUA_REGISTRYINDEX, node->sub_ref);
			break;

		default:
			continue;
		}
		lua_pushvalue(L, 2);
		lua_setupvalue(L, -2, 1);
		lua_pop(L, 1);
	}
}

static void template_render_template (lua_State *L, FILE *f, const char *filename, int depth) {
	int          result;
	node_t      *node;
	size_t       i, nret;
	template_t  *template;
	const char  *str, *c;

	/* check depth */
	if (depth > TEMPLATE_MAX_DEPTH) {
		luaL_error(L, "template depth exceeds %d", TEMPLATE_MAX_DEPTH);
	}

	/* get template, parsing it as needed */
	if (lua_getfield(L, 4, filename) != LUA_TUSERDATA
			|| !(template = luaL_testudata(L, -1, TEMPLATE_TEMPLATE))) {
		lua_pop(L, 1);
		lua_pushcfunction(L, template_parse);
		lua_pushstring(L, filename);
		lua_call(L, 1, 1);
		lua_pushvalue(L, -1);
		lua_setfield(L, 4, filename);
		template = lua_touserdata(L, -1);
	}
	if (template->env != lua_topointer(L, 2)) {
		template_setenv(L, template);
		template->env = lua_topointer(L, 2);
	}

	/* render template */
	i = 0;
	while (i < template->nodes->count) {
		node = list_get(template->nodes, i);
		switch (node->type) {
		case NT_NONE:
			i++;
			break;

		case NT_JUMP:
			i = node->jump_next;
			break;

		case NT_IF:
			template_eval(L, node->if_ref, 1);
			if (lua_toboolean(L, -1)) {
				i++;
			} else {
				i = node->if_next;
			}
			lua_pop(L, 1);
			break;

		case NT_FOR_INIT:
			template_eval(L, node->for_init_ref, 3);
			i++;
			break;

		case NT_FOR_NEXT:
			lua_pushvalue(L, -3);
			lua_pushvalue(L, -3);
			lua_pushvalue(L, -3);
			nret = node->for_next_names->count;
			lua_call(L, 2, nret);
			if (lua_isnil(L, -nret)) {
				lua_pop(L, 3 + nret);
				i = node->for_next_next;
			} else {
				lua_pushvalue(L, -nret);
				lua_replace(L, -1 - nret - 1);
				while (nret > 0) {
					nret--;
					lua_setfield(L, 2, *(const char **)list_get(node->for_next_names, nret));
				}
				i++;
			}
			break;

		case NT_SET:
			nret = node->set_names->count;
			template_eval(L, node->set_ref, nret);
			while (nret > 0) {
				nret--;
				lua_setfield(L, 2, *(const char **)list_get(node->set_names, nret));
			}
			i++;
			break;
 
		case NT_INCLUDE:
			template_eval_str(L, node->include_ref);
			template_render_template(L, f, lua_tostring(L, -1), depth + 1);
			lua_pop(L, 1);
			i++;
			break;			

		case NT_SUB:
			template_eval(L, node->sub_ref, 1);
			if (lua_isstring(L, -1)) {
				str = lua_tostring(L, -1);
			} else if (lua_isnil(L, -1) && (node->sub_flags & TEMPLATE_FSUPNIL)) {
				str = "";
			} else {
				lua_pushfstring(L, "(%s)", luaL_typename(L, -1));
				lua_replace(L, -2);
				str = lua_tostring(L, -1);
			}
			switch (node->sub_flags & TEMPLATE_FESC) {
			case TEMPLATE_FESCXML:
				result = 0;
				for (c = str; *c != '\0'; c++) {
					switch (*c) {
					case '&':
						result = fputs("&amp;", f);
						break;

					case '<':
						result = fputs("&lt;", f);
						break;

					case '>':
						result = fputs("&gt;", f);
						break;

					default:
						result = fputc(*c, f);
					}
					if (result == EOF) {
						break;
					}
				}
				break;

			case TEMPLATE_FESCURL:
				result = 0;
				for (c = str; *c != '\0'; c++) {
					if (isalnum(*c) || *c == '-' || *c == '.' || *c == '_' || *c == '~') {
						result = fputc(*c, f);
					} else {
						result = (fputc('%', f) != EOF
								&& fputc(template_hex_digits[*c / 16], f) != EOF
								&& fputc(template_hex_digits[*c % 16], f) != EOF) ? 3 : EOF;
					}
					if (result == EOF) {
						break;
					}
				}
				break;

			case TEMPLATE_FESCJS:
				result = 0;
				for (c = str; *c != '\0'; c++) {
					switch (*c) {
					case '\b':
						result = fputs("\\b", f);
						break;

					case '\t':
						result = fputs("\\t", f);
						break;

					case '\n':
						result = fputs("\\n", f);
						break;

					case '\v':
						result = fputs("\\v", f);
						break;

					case '\f':
						result = fputs("\\f", f);
						break;

					case '\r':
						result = fputs("\\r", f);
						break;

					case '"':
						result = fputs("\\\"", f);
						break;

					case '\'':
						result = fputs("\\'", f);
						break;

					case '\\':
						result = fputs("\\\\", f);
						break;

					default:
						result = fputc(*c, f);
					}
					if (result == EOF) {
						break;
					}
				}
				break;

			default:
				result = fputs(str, f);
			}
			if (result == EOF) {
				luaL_error(L, "error writing template");
			}
			lua_pop(L, 1);
			i++;
			break;

		case NT_RAW:
			if (fwrite(node->raw_str, 1, node->raw_len, f) != node->raw_len) {
				luaL_error(L, "error writing template");
			}
			i++;
			break;
		}
	}

	/* pop template */
	lua_pop(L, 1);
}

static int template_fclose (lua_State *L) {
	int           result;
	memstream_t  *memstream;

	memstream = luaL_checkudata(L, 1, LUA_FILEHANDLE);
	result = luaL_fileresult(L, fclose(memstream->stream.f) == 0, NULL);
	free(memstream->str);
	return result;
}

static int template_render (lua_State *L) {
	int           have_stream;
	const char   *filename;
	luaL_Stream  *stream;
	memstream_t  *memstream;

	/* check arguments */
	filename = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);
	have_stream = lua_gettop(L) >= 3;
	if (have_stream) {
		stream = luaL_checkudata(L, 3, LUA_FILEHANDLE);
		lua_settop(L, 3);
	} else {
		memstream = lua_newuserdata(L, sizeof(memstream_t));
		memstream->stream.closef = NULL;
		memstream->str = NULL;
		luaL_setmetatable(L, LUA_FILEHANDLE);
		memstream->stream.f = open_memstream(&memstream->str, &memstream->len);
		if (!memstream->stream.f) {
			luaL_error(L, "error opening memory stream");
		}
		memstream->stream.closef = template_fclose;
		stream = &memstream->stream;
	}
	
	/* get templates registry */
	if (lua_getfield(L, LUA_REGISTRYINDEX, TEMPLATE_TEMPLATES) != LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, TEMPLATE_TEMPLATES);
	}

	/* render */
	template_render_template(L, stream->f, filename, 1);

	/* return result, if any */
	if (have_stream) {
		return 0;
	} else {
		if (fclose(memstream->stream.f) != 0) {
			return luaL_error(L, "error closing memory stream");
		}
		lua_pushlstring(L, memstream->str, memstream->len);
		free(memstream->str);
		memstream->stream.closef = NULL;
		return 1;
	}
}


/*
 * library
 */

static int template_getresolver (lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, TEMPLATE_RESOLVER);
	return 1;
}

static int template_setresolver (lua_State *L) {
	if (!lua_isnoneornil(L, 1)) {
		luaL_checktype(L, 1, LUA_TFUNCTION);
	}
	lua_settop(L, 1);
	lua_setfield(L, LUA_REGISTRYINDEX, TEMPLATE_RESOLVER);
	return 0;
}

static int template_clear (lua_State *L) {
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, TEMPLATE_TEMPLATES);
	return 0;
}

int luaopen_template (lua_State *L) {
	static luaL_Reg template_lua_functions[] = {
		{"render", template_render},
		{"getresolver", template_getresolver},
		{"setresolver", template_setresolver},
		{"clear", template_clear},
		{NULL, NULL}
	};

	/* functions */
	luaL_newlib(L, template_lua_functions);

	/* parser */
	luaL_newmetatable(L, TEMPLATE_PARSER);
	lua_pushcfunction(L, template_parser_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	/* template */
	luaL_newmetatable(L, TEMPLATE_TEMPLATE);
	lua_pushcfunction(L, template_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pushcfunction(L, template_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	return 1;
}

/*
 * Template
 *
 * Copyright (C) 2011-2024 Andre Naef
 */


#ifndef _TEMPLATE_INCLUDED
#define _TEMPLATE_INCLUDED


#include <lua.h>


#define TEMPLATE_PARSER     "template.parser"     /* parser metatable */
#define TEMPLATE_TEMPLATE   "template.template"   /* template metatable */
#define TEMPLATE_TEMPLATES  "template.templates"  /* loaded templates */
#define TEMPLATE_RESOLVER   "template.resolver"   /* resolver function */


int luaopen_template(lua_State *L);


#endif  /* _TEMPLATE_INCLUDED */

LUA_INCDIR=/usr/include/lua5.3
LUA_BIN=/usr/bin/lua5.3
LIBDIR=/usr/local/lib/lua/5.3
CFLAGS=-Wall -Wextra -Wpointer-arith -Werror -fPIC -O3 -D_REENTRANT -D_GNU_SOURCE
LDFLAGS=-shared -fPIC

export LUA_CPATH=$(PWD)/?.so

default: all

all: template.so

template.so: template.o table.o list.o
	gcc $(LDFLAGS) -o template.so template.o table.o list.o

template.o: src/template.h src/template.c src/table.h src/list.h
	gcc -c -o template.o $(CFLAGS) -I$(LUA_INCDIR) src/template.c

table.o: src/table.h src/table.c
	gcc -c -o table.o $(CFLAGS) -I$(LUA_INCDIR) src/table.c

list.o: src/list.h src/list.c
	gcc -c -o list.o $(CFLAGS) -I$(LUA_INCDIR) src/list.c

.PHONY: test
test:
	$(LUA_BIN) test/test.lua

install:
	cp template.so $(LIBDIR)

clean:
	-rm -f template.o table.o list.o template.so

.PHONY: all build test clean

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -g
CLI_SRC := src/mote_cli.c
CORE_SRC := src/mote_core.c
HDRS := $(wildcard src/*.h) $(wildcard src/module/*.h) $(wildcard src/semantic/*.h) $(wildcard src/typesystem/*.h) $(wildcard src/mir/*.h) $(wildcard src/mir/lowering/*.h)
COMPILER = mote
CORE_LIB = mote_core.dll
CORE_IMPLIB = libmote_core.a

all: build

build: $(COMPILER) $(CORE_LIB)

$(COMPILER): $(CLI_SRC) $(CORE_LIB) $(CORE_IMPLIB)
	$(CC) $(CFLAGS) $(CLI_SRC) -L. -lmote_core -o $(COMPILER)

$(CORE_LIB): $(CORE_SRC) $(HDRS)
	$(CC) $(CFLAGS) -shared $(CORE_SRC) -Wl,--out-implib,$(CORE_IMPLIB) -o $(CORE_LIB)

clean:
	rm -f $(COMPILER) $(CORE_LIB) $(CORE_IMPLIB)

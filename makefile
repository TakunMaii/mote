.PHONY: all build test clean

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -g
CLI_SRC := src/mote_cli.c
CORE_SRC := src/mote_core.c
HDRS := $(wildcard src/*.h) $(wildcard src/module/*.h) $(wildcard src/semantic/*.h) $(wildcard src/typesystem/*.h) $(wildcard src/mir/*.h) $(wildcard src/mir/lowering/*.h)

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
CORE_LIB := mote_core.dll
CORE_LINK_INPUT := .\mote_core.lib
CORE_BUILD_FLAGS := -shared -Wl,--out-implib,mote_core.lib
REMOVE := del /q
else
UNAME_S := $(shell uname -s 2>/dev/null)
EXE_EXT :=
ifeq ($(UNAME_S),Darwin)
CORE_LIB := libmote_core.dylib
CORE_BUILD_FLAGS := -dynamiclib -Wl,-install_name,@rpath/libmote_core.dylib
else
CORE_LIB := libmote_core.so
CORE_BUILD_FLAGS := -shared
endif
CORE_LINK_INPUT := -L. -lmote_core
REMOVE := rm -f
endif

COMPILER = mote$(EXE_EXT)

all: build

build: $(COMPILER) $(CORE_LIB)

$(COMPILER): $(CLI_SRC) $(CORE_LIB)
	$(CC) $(CFLAGS) $(CLI_SRC) $(CORE_LINK_INPUT) -o $(COMPILER)

$(CORE_LIB): $(CORE_SRC) $(HDRS)
	$(CC) $(CFLAGS) $(CORE_BUILD_FLAGS) $(CORE_SRC) -o $(CORE_LIB)

clean:
	-$(REMOVE) $(COMPILER) $(CORE_LIB) mote_core.lib libmote_core.a

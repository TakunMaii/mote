.PHONY: all build test clean

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -g
SRC := src/main.c
HDRS := $(shell find src -name '*.h' -print)
COMPILER = mote

all: build

build: $(COMPILER)

$(COMPILER): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) $(SRC) -o $(COMPILER)

clean:
	rm -f $(COMPILER)

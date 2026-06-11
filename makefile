.PHONY: all build test clean

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -g
SRC := src/main.c
HDRS := $(shell find src -name '*.h' -print)
COMPILER ?= mote_test

all: build

build: $(COMPILER)

$(COMPILER): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) $(SRC) -o $(COMPILER)

test:
	python3 scripts/test.py --build --compiler-path ./$(COMPILER)

clean:
	rm -f $(COMPILER)

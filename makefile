.PHONY: all build test

SRC=src/main.c
TEST_SCRIPT=scripts/test.ps1

all: build

build: $(SRC)
	gcc $(SRC) -o mote	

test:
	python3 scripts/test.py --build

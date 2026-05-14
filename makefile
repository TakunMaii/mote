.PHONY: all build

SRC=src/main.c

all: build

build: $(SRC)
	gcc $(SRC) -o mote	

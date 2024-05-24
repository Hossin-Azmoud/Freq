CC=gcc
OBJECTS=./obj
INLCUDE=-I./include
BIN=./bin
SRC=./src/*.c # ./include/miniaudio/miniaudio.h
LIBS=-lncurses -pthread -lm
CFLAGS=-ggdb
OUT=$(BIN)/Freq

all: Freq
dirs:
	mkdir -p $(BIN)

Freq: dirs
	$(CC) $(INLCUDE) -o $(OUT) $(SRC) $(CFLAGS) $(LIBS)
run:
	$(BIN)/Freq ./blue.mp3

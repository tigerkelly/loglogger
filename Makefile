
# This make file has only been tested on a RPI 3B+.
CC=gcc

SRC=loglogger.c

LDFLAGS=-g -L/usr/local/lib -L../utils/libs -lini -lstrutils -lz -lpthread -lm
CFLAGS=-std=gnu99

CFLAGS += -g -Wall -O2 -I./ -I../utils/incs

PRG=loglogger

all: $(PRG)

$(PRG): $(PRG).o
	# ctl stop loglogger
	$(CC) $(PRG).o -o $(PRG) $(LDFLAGS)
	cp $(PRG) ~/work/cmhome/bin/$(PRG)
	cp $(PRG) ~/bin/$(PRG)

$(PRG).o: $(PRG).c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(PRG).o $(PRG)


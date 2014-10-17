CC=gcc

CFLAGS = -ansi -c -O3 -Wall -pedantic -D_GNU_SOURCE -fdiagnostics-color=always -Wno-variadic-macros

all: mcast start_mcast


mcast: mcast.o buffer.o recv_dbg.o linkedlist.o
	$(CC) -o mcast mcast.o buffer.o recv_dbg.o linkedlist.o -O3


start_mcast: start_mcast.o
	$(CC) -o start_mcast start_mcast.o 

clean:

	rm *.o
	rm mcast
	rm start_mcast

%.o:    %.c config.h buffer.h linkedlist.h
	$(CC) $(CFLAGS) $*.c -O3



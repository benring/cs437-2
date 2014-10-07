CC=gcc

CFLAGS = -ansi -c -Wall -pedantic -D_GNU_SOURCE -fdiagnostics-color=always

all: mcast start_mcast


mcast: mcast.o buffer.o
	$(CC) -o mcast mcast.o buffer.o -O3


start_mcast: start_mcast.o
	$(CC) -o start_mcast start_mcast.o 

clean:

	rm *.o
	rm mcast
	rm start_mcast

%.o:    %.c message.h config.h buffer.h
	$(CC) $(CFLAGS) $*.c -O3



CC=gcc

CFLAGS = -ansi -c -Wall -pedantic -D_GNU_SOURCE -fdiagnostics-color=always

all: mcast start_mcast


mcast: mcast.o
	$(CC) -o mcast mcast.o 


start_mcast: start_mcast.o
	$(CC) -o start_mcast start_mcast.o 

clean:

	rm *.o
	rm mcast
	rm start_mcast


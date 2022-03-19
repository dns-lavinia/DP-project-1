CC = gcc
CFLAGS = -g -Wall -O3 -pedantic -pthread

all: 
	$(CC) $(CFLAGS) -o prog peer.c

clean:
	$(RM) prog
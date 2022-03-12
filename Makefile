CC = gcc
CFLAGS = -g -Wall -O3 -pedantic

all: 
	$(CC) $(CFLAGS) -o prog peer.c

clean:
	$(RM) prog
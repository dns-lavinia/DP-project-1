C_FLAGS = -Wall -pthread
LIBS = lib/protocol.c lib/netio.c lib/logger.c

server:
	gcc $(C_FLAGS) -o bin/server.exe src/server.c $(LIBS)

test:
	gcc $(C_FLAGS) -o bin/test.exe src/test.c $(LIBS)
	
peer:
	gcc $(C_FLAGS) -o bin/peer.exe src/peer.c $(LIBS)
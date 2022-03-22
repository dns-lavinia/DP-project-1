server:
	gcc -Wall -pthread -o bin/server.exe src/server.c  lib/protocol.c lib/netio.c

test:
	gcc -Wall -o bin/test.exe src/test.c  lib/protocol.c lib/netio.c
	
peer:
	gcc -Wall -pthread -o bin/peer.exe src/peer.c  lib/protocol.c lib/netio.c
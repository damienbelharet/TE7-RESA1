CC=gcc
CFLAGS=-Wall -Wextra -Werror -g

all: client2 server2

client2: client2.c common.c p2p.c common.h msg_struct.h p2p.h
	$(CC) $(CFLAGS) -o client2 client2.c common.c p2p.c

server2: server2.c common.c user-utils.c common.h msg_struct.h user-utils.h
	$(CC) $(CFLAGS) -o server2 server2.c common.c user-utils.c

clean:
	rm -f client2 server2
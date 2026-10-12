CC=gcc
CFLAGS=-Wall -Wextra -Werror -g

.PHONY: all clean

all: client server

client: client.c common.c p2p.c common.h msg_struct.h p2p.h
	$(CC) $(CFLAGS) -o client client.c common.c p2p.c

server: server.c common.c user-utils.c common.h msg_struct.h user-utils.h
	$(CC) $(CFLAGS) -o server server.c common.c user-utils.c

clean:
	rm -f client server client server
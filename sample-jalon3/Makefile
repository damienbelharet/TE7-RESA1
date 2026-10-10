CC=gcc
CFLAGS=-Wall -Wextra -Werror

all: client2 server2

client2: client2.c common.c
	$(CC) $(CFLAGS) -o client2 client2.c common.c

server2: server2.c common.c user-utils.c
	$(CC) $(CFLAGS) -o server2 server2.c common.c user-utils.c

clean:
	rm -f client2 server2
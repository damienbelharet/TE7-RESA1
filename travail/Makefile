CC=gcc
CFLAGS=-Wall -Wextra

all: client2 server2

client2: client2.c
	$(CC) $(CFLAGS) -o client2 client2.c

server2: server2.c
	$(CC) $(CFLAGS) -o server2 server2.c

clean:
	rm -f client2 server2
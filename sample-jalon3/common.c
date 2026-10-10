#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "msg_struct.h"

#include "common.h"

char* msg_type_str[] = {
    "NICKNAME_NEW",
    "NICKNAME_LIST",
    "NICKNAME_INFOS",
    "ECHO_SEND",
    "UNICAST_SEND", 
    "BROADCAST_SEND",
    "MULTICAST_CREATE",
    "MULTICAST_LIST",
    "MULTICAST_JOIN",
    "MULTICAST_SEND",
    "MULTICAST_QUIT",
    "FILE_REQUEST",
    "FILE_ACCEPT",
    "FILE_REJECT",
    "FILE_SEND",
    "FILE_ACK"
};

void die(int ret_value, char * message)
{
    if (ret_value < 0) {
        perror(message);
        exit(EXIT_FAILURE);
    }
}

int write_on_socket(int fd, void *buf, int to_write) {
    int written_bytes = 0;
    int ret_value = 0;

    while (written_bytes != to_write) {
        ret_value = write(fd, (char *)buf + written_bytes, to_write - written_bytes);
        if (ret_value <= 0) {
            return -1;
        }
        written_bytes += ret_value;
    }
    return written_bytes;
}

int read_on_socket(int fd, void *ptr, int size) {
    int read_bytes = 0;
    int ret_value = 0;

    while (read_bytes != size) {
        ret_value = read(fd, (char *)ptr + read_bytes, size - read_bytes);
        if (ret_value <= 0) {
            return -1;
        }
        read_bytes += ret_value;
    }
    return read_bytes;
}
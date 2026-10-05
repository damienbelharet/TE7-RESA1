#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

void die(int ret_value, char * message)
{
    if (ret_value < 0) {
        printf("Erreur (die) : %s", message);
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
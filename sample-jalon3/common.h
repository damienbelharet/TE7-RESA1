#ifndef COMMON_H
#define COMMON_H

#define MSG_LEN 1024
#define SERV_PORT "8080"
#define SERV_ADDR "127.0.0.1"

void die(int ret_value, char * message);
int write_on_socket(int fd, void *buf, int to_write);
int read_on_socket(int fd, void *ptr, int size);

#endif
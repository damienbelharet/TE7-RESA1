#ifndef USER_UTILS_H
#define USER_UTILS_H

#include <arpa/inet.h>
#include <poll.h>
#include <time.h>
#include "msg_struct.h" // Nécessaire pour NICK_LEN

struct client_node {
    int fd;
    char ip[INET_ADDRSTRLEN];
    int port;
    char nick[NICK_LEN];
    struct client_node *next;
    time_t time_sconnected;
};

// extern indique que la variable est définie dans le .c
extern struct client_node *client_list;

void add_client(int fd, const char *ip, int port);
void remove_client(int fd);
void destroy_list(void);
void disconnecte_client(struct pollfd *fds, int index);

#endif
#ifndef P2P_H
#define P2P_H

int receive_file(int listen_fd);
int create_listen_sock(int *port);
int send_file(int p2p_fd, char *path, char *my_pseudo);

#endif

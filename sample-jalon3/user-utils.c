#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "user-utils.h"

struct client_node *client_list = NULL;

void add_client(int fd, const char *ip, int port){
    struct client_node * noeud = malloc(sizeof(struct client_node));
    if (noeud == NULL){
        return;
    }
    noeud->fd = fd;
    noeud->port = port;
    strncpy(noeud->ip, ip, INET_ADDRSTRLEN);
    memset(noeud->nick, 0, NICK_LEN);
    noeud->next = client_list;
    noeud->time_sconnected = time(NULL);
    client_list = noeud;
}

void remove_client(int fd){
    struct client_node *curr = client_list;
    struct client_node *prec = NULL;
    
    while(curr != NULL){
        if (curr->fd == fd){
            if(prec == NULL){
                client_list = curr->next;
            }
            else{
                prec->next = curr->next;
            }
            free(curr);
            return;
        }
        else{
            prec = curr;
            curr = curr->next;
        }
    }
    printf("echec de remove client\n");
}

void destroy_list(void){
    struct client_node *curr = client_list;
    struct client_node *next_node;
    while(curr != NULL){
        next_node = curr->next;
        free(curr);
        curr = next_node;
    }
    client_list = NULL;
}

void disconnecte_client(struct pollfd *fds, int index){
    remove_client(fds[index].fd);
    close(fds[index].fd);
    fds[index].fd = -1;
    fds[index].events = 0;
    fds[index].revents = 0;
}

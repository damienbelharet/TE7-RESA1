#define _POSIX_C_SOURCE 200112L // GEMINI

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "common.h"
#define MAX_CLIENTS 10

struct client_node {
    int fd;
    char ip[INET_ADDRSTRLEN];
    int port;
    struct client_node *next;
};

struct client_node *client_list = NULL;


void add_client(int fd, struct sockaddr_in *addr) {
    struct client_node *new_c = malloc(sizeof(struct client_node));
    if (!new_c) return;

    new_c->fd = fd;
    inet_ntop(AF_INET, &(addr->sin_addr), new_c->ip, INET_ADDRSTRLEN);
    new_c->port = ntohs(addr->sin_port);

    new_c->next = client_list;
    client_list = new_c;
}

void remove_client(int fd) {
    struct client_node *curr = client_list;
    struct client_node *prev = NULL;

    while (curr != NULL) {
        if (curr->fd == fd) {
            if (prev == NULL) {
                client_list = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr); 
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void die(ssize_t ret_value, const char * msg){
	if (ret_value < 0){
		perror(msg);
		exit(EXIT_FAILURE);
	}
}
int write_on_socket(int sockfd, void * buf, int to_write )
{
    int written_bytes = 0;
    int ret_value = 0;

    while (written_bytes != to_write){
    ret_value = write(sockfd, (char *)buf + written_bytes, to_write - written_bytes);
    if (ret_value == 0){
        printf("Disconnected\n");
        exit(EXIT_FAILURE);
        }
    die(ret_value, "write_on_socket");
    written_bytes += ret_value;

    }
    return written_bytes;
}



int read_on_socket(int fd, void * ptr, int size) {

    int read_bytes = 0;
    int ret_value = 0;

    while (read_bytes != size){
    ret_value = read(fd, (char *)ptr + read_bytes, size - read_bytes);
    if (ret_value == 0){
        printf("Disconnected\n");
        return 0;
        }
    die(ret_value, "reading msg header");
    read_bytes += ret_value;


    }
    return read_bytes;
}

void echo_server(int sockfd) {
	char buff[MSG_LEN];
	struct pollfd fds[MAX_CLIENTS];

	fds[0].fd = sockfd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	for(int i = 1; i < MAX_CLIENTS; i++)
	{
		fds[i].fd = -1;
		fds[i].events = 0;
		fds[i].revents = 0;
	}
	printf("Server listenning ...\n");

	while (1) {;
		printf("Waiting for activity...\n");
		int nb_active_fd = poll(fds, MAX_CLIENTS, -1);
		die(nb_active_fd, "on polling");

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (i == 0 && (fds[0].revents & POLLIN))
			{
				fds[0].revents = 0;
				struct sockaddr_in cli;
				socklen_t len = sizeof(cli);
				int client_fd = accept(sockfd, (struct sockaddr*)&cli, &len);

				if (client_fd < 0)
				{
					perror("accept"); 
					continue;
				}

				printf("New client %d\n", client_fd);

				int slot_found = 0;
				for (size_t j = 1; j < MAX_CLIENTS; j++) //on cherche une case libre
				{
					if (fds[j].fd == -1)
					{
						fds[j].fd = client_fd;
						fds[j].events = POLLIN;
						fds[j].revents = 0;
						add_client(client_fd, &cli);
						slot_found = 1;
						break;
					}
				}
				if (!slot_found){
					fprintf(stderr,"Serveur full, refuse connexion\n");
					close(client_fd);
				}
			}

		else if (i > 0 && (fds[i].revents & POLLIN))
		{
			fds[i].revents = 0;
			memset(buff, 0, MSG_LEN);
			int incoming_size = 0;
            
			if (read_on_socket(fds[i].fd, &incoming_size, sizeof(int)) <= 0) {
				fprintf(stderr, "Client disconnected fd = %d\n", fds[i].fd);
				remove_client(fds[i].fd);
				close(fds[i].fd);
				fds[i].fd = -1;
				continue;
			}

			if (incoming_size > MSG_LEN - 1) incoming_size = MSG_LEN - 1;

			if (read_on_socket(fds[i].fd, buff, incoming_size) <= 0) {
				fprintf(stderr, "Client disconnected fd = %d\n", fds[i].fd);
				remove_client(fds[i].fd);
				close(fds[i].fd);
				fds[i].fd = -1;
				continue;
			}
			buff[incoming_size] = '\0';

			// 1.7
			if (strncmp(buff, "/quit\n", 6) == 0) {
				printf("Client %d a demandé la déconnexion (/quit).\n", fds[i].fd);
				remove_client(fds[i].fd);
				close(fds[i].fd);
				fds[i].fd = -1; 
				continue; 
			}
			printf("Received: %s", buff);

			write_on_socket(fds[i].fd, &incoming_size, sizeof(int));
			write_on_socket(fds[i].fd, buff, incoming_size);
			printf("Message sent to %d!\n", fds[i].fd);
			
		}		
	}
}
}

int handle_bind(const char * server_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo)); // hints c'est une structure où on va mettre ce qu'on cherche 
	hints.ai_family = AF_UNSPEC; //Ipv4 et IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // On écoute toutes les inferfaces réseaux disponibles
	if (getaddrinfo(NULL, server_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}

		int opt = 1;
        die(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)), "setsockopt");

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char * argv[]) {
	if (argc != 2)
	{
		fprintf(stderr, "Usage : %s <server_port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	int sfd = handle_bind(argv[1]);
	if ((listen(sfd, SOMAXCONN)) != 0) {  //SOMAXCONN = la plus grande file d'attente pour les clients
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}
	
	echo_server(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}


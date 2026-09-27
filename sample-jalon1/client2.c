#define _POSIX_C_SOURCE 200112L
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

void echo_client(int sockfd) {

    char buff[MSG_LEN];
	int n;
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    printf("Message: ");
    fflush(stdout);

	while (1) {

        int nb_active = poll(fds, 2, -1);
        die(nb_active, "on polling...");

        memset(buff, 0, MSG_LEN);
        // Getting message from client

        if (fds[0].revents & POLLIN){
            fds[0].revents = 0;

            n = 0;
            int c; //int car EOF vaut -1 et char ne peut pas être -1
            while ((c = getchar())){    //Double parenthèse à cause de Werror. C'est bizarre de faire une assignation dans un while.
                if (c == '\n' || c == EOF){
                    break;
                }
                else{
                    buff[n] = c;
                    n++;
                }
            }
            buff[n] = '\0';

            if (n == 0){
                printf("Message: ");
                fflush(stdout);
                continue;
            }

            if (write_on_socket(sockfd, &n, sizeof(int)) <= 0) {
                close(sockfd);
                break;
            }
            printf("nb carac envoyé!\n");

            // Sending message (ECHO)
            if (write_on_socket(sockfd, buff, n) <= 0) {
                close(sockfd);
                break;
            }
            printf("Message sent!\n");

            if(strcmp(buff, "/quit") == 0){
                close(sockfd);
                break;
            }
        }
        else if(fds[1].revents & POLLIN){
            fds[1].revents = 0;

            // Cleaning memory
            memset(buff, 0, MSG_LEN);
            n = 0;

            if (read_on_socket(sockfd, &n, sizeof(int)) <= 0) {
                close(sockfd);
                break;
            }
            printf("received and nb of carac is: %d\n", n);


            // Receiving message
            if (read_on_socket(sockfd, buff, n) <= 0) {
                close(sockfd);
                break;
            }
            printf("Received: %s\n", buff);

        }
        printf("Message2: ");
        fflush(stdout);		
    }   
}

int handle_connect(char * server_name, char * server_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(server_name, server_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char * argv[]) {

    if (argc < 3){
        fprintf(stdout, "Error not enough arguments. Use :\n ./client2.c <server.name> <server.port> \n<");
        exit(EXIT_FAILURE);
    }
    if (argc > 3){
        fprintf(stdout, "Error to many arguments. Use :\n ./client2.c <server.name> <server.port> \n<");
        exit(EXIT_FAILURE);
    }
	int sfd;
	sfd = handle_connect(argv[1], argv[2]);
	echo_client(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}
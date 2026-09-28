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
#define FDS_SIZE 2


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
    die(ret_value, "reading msg header");
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



void echo_client(int sockfd) {
	char buff[MSG_LEN];
	int n;
	struct pollfd fds[FDS_SIZE];
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	fds[1].fd = sockfd;
	fds[1].events = POLLIN;
	fds[1].revents = 0;

	while (1) {

		int nb_active_fd;
		printf("Message: ");
		fflush(stdout); // pour reinitialiser le tampon
		nb_active_fd = poll(fds, FDS_SIZE, -1);
		die(nb_active_fd, "on polling");

		if (fds[0].revents & POLLIN)
		{
			memset(buff, 0, MSG_LEN);
			// Getting message from client
			n = 0;
			while ((buff[n++] = getchar()) != '\n' && n < MSG_LEN) {} // trailing '\n' will be sent
			
            if (strncmp(buff, "/quit\n", 6) == 0) {
                int msg_size = strlen(buff);
                write_on_socket(sockfd, &msg_size, sizeof(int));
                write_on_socket(sockfd, buff, msg_size);
                
                printf("Fermeture de la connexion...\n");
                break; // On sort de la boucle infinie
            }

			int msg_size = strlen(buff);
			if (write_on_socket(sockfd, &msg_size, sizeof(int)) < 0)
			{
				break;
			}
			
			if (write_on_socket(sockfd, buff, msg_size) < 0)
			{
				break;
			}

			printf("Message sent!\n");
			fds[0].revents = 0;
		}
		
		if (fds[1].revents & POLLIN)
		{
			memset(buff, 0, MSG_LEN);
			// Receiving message
			int incoming_size = 0;
			if (read_on_socket(sockfd, &incoming_size, sizeof(int)) <= 0)
			{
				fds[1].revents = 0;
				break;
			}
			if (incoming_size > MSG_LEN - 1) incoming_size = MSG_LEN - 1;
			if (read_on_socket(sockfd, buff, incoming_size) <= 0)
			{
				fds[1].revents = 0;
				break;
			}
			buff[incoming_size] = '\0';
			printf("\nReceived: %s", buff);

		}		
	}
}


int handle_connect(const char *server_name, const char *server_port) {
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

	if (argc != 3)
	{
		fprintf(stderr, "Usage : %s <server_name> <server_port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int sfd;
	sfd = handle_connect(argv[1], argv[2]);
	echo_client(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}


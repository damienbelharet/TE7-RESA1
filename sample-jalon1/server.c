#define _POSIX_C_SOURCE 200112L // GEMINI

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

void echo_server(int sockfd) {
	char buff[MSG_LEN];
	while (1) {
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Receiving message
		if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
			break;
		}
		printf("Received: %s", buff);
		// Sending message (ECHO)
		if (send(sockfd, buff, strlen(buff), 0) <= 0) {
			break;
		}
		printf("Message sent!\n");
	}
}

int handle_bind() {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo)); // hints c'est une structure où on va mettre ce qu'on cherche 
	hints.ai_family = AF_UNSPEC; //Ipv4 et IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // On écoute toutes les inferfaces réseaux disponibles
	if (getaddrinfo(NULL, SERV_PORT, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
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

int main() {
	struct sockaddr cli;
	int sfd, connfd;
	socklen_t len;
	sfd = handle_bind();
	if ((listen(sfd, SOMAXCONN)) != 0) {  //SOMAXCONN = la plus grande file d'attente pour les clients
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}
	len = sizeof(cli); // on prépare la taille max 
	if ((connfd = accept(sfd, (struct sockaddr*) &cli, &len)) < 0) { // &len = 16 pour ipv4 et 28 pour ipv6
		perror("accept()\n");
		exit(EXIT_FAILURE);
	}
	echo_server(connfd);
	close(sfd);
	return EXIT_SUCCESS;
}


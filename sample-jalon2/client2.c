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
#include "msg_struct.h"

void echo_client(int sockfd) {

    char my_pseudo[NICK_LEN] = "";
    char pseudo[NICK_LEN] = "";
    struct message msgstruct;
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

        memset(&msgstruct, 0, sizeof(struct message));
        memset(buff, 0, MSG_LEN);
        char commande[MSG_LEN];
        char extra[MSG_LEN];
        int ret = 0;
        // Getting message from client

        if (fds[0].revents & POLLIN){
            fds[0].revents = 0;

            n = 0;
            int c  = 0; //int car EOF vaut -1 et char ne peut pas être -1
            while ((c = getchar())){    //Double parenthèse à cause de Werror. C'est bizarre de faire une assignation dans un while.
                if (c == '\n' || c == EOF){
                    break;
                }
                else if (n < MSG_LEN - 1)
                {
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

            if(strcmp(buff, "/quit") == 0){
                close(sockfd);
                break;
            }

            memset(&msgstruct, 0, sizeof(struct message)); // on reinitialise la structure
            strncpy(msgstruct.nick_sender, my_pseudo, NICK_LEN - 1); // assigne le pseudo actuel

            ret = sscanf(buff, "%1023s %1023s %1023s", commande, pseudo, extra);

            if (ret >= 1 && strcmp(commande, "/nick") == 0) {
                const char *autorises = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";


                if (ret < 2 || ret == 3 || strlen(pseudo) >= NICK_LEN || strspn(pseudo, autorises) != strlen(pseudo)) {
                    printf("Pseudo invalide. Utilisez uniquement des lettres et des chiffres.\nMessage: ");
                    fflush(stdout);
                    continue;
                }

                msgstruct.type = NICKNAME_NEW;
                strncpy(msgstruct.infos, pseudo, INFOS_LEN - 1);
                msgstruct.pld_len = 0; // Pas de payload pour NICKNAME_NEW
            }
            else if (ret >= 1 && strcmp(commande, "/who") == 0){
                msgstruct.type = NICKNAME_LIST;
                msgstruct.pld_len = 0;
                strncpy(msgstruct.infos, "", INFOS_LEN);
            }
            else if (ret >= 1 && strcmp(commande, "/whois") == 0){
                msgstruct.type = NICKNAME_INFOS;
                msgstruct.pld_len = 0;

                strncpy(msgstruct.infos, pseudo, INFOS_LEN);
            }
            else if (ret > 1 && strcmp(commande, "/mll") == 0){ //on peut pas faire comme /who car le message peut être une phrase respecte pas sscanf

                char * message_texte = strchr(buff ,' ');  //renvoie l'adresse mémoire du premier espace dans buff
                if (message_texte != NULL){ // strchr renvoie null si elle trouve pas le carac dans le char * donc on vérifie ici
                    message_texte++; //possible car l'adresse mémoire pointe spécifiquement sur un char
                }

                msgstruct.type = BROADCAST_SEND;
                msgstruct.pld_len = strlen(message_texte);
                strncpy(msgstruct.infos, "", INFOS_LEN);

                char tmp[MSG_LEN]; // sans ça gros bug. strcpy copie les octets de gauche à droite tout en modifiant buff. Donc j'écrase avant de lire ça transforme le message
                strncpy(tmp, message_texte, MSG_LEN - 1);
                tmp[MSG_LEN - 1] = '\0'; // sécurité si message_texte > MSG_LEN - 1 strcnpy ne rajoute pas le \0
                strncpy(buff, tmp, MSG_LEN - 1);

            }
            else if (ret >= 3 && strcmp(commande, "/msg") == 0){ //on peut pas faire comme /who car le message peut être une phrase respecte pas sscanf

                char * space1 = strchr(buff ,' '); 
                if (space1 != NULL){ 
                    while(*space1 == ' '){
                        space1++;
                    } // si plusieurs espaces entre /msg et pseudo char

                    char *space2 = strchr(space1, ' ');
                    if(space2 != NULL){
                        while (*space2 == ' '){
                            space2++;
                            }
                        msgstruct.type = UNICAST_SEND;
                        msgstruct.pld_len = strlen(space2); 
                        strncpy(msgstruct.infos, pseudo, INFOS_LEN - 1);

                        char tmp[MSG_LEN]; 
                        strncpy(tmp, space2, MSG_LEN - 1);
                        tmp[MSG_LEN - 1] = '\0'; // sécurité si message_texte > MSG_LEN - 1 strcnpy ne rajoute pas le \0
                        strncpy(buff, tmp, MSG_LEN - 1);
                    }
                    else{
                        printf("/msg <pseudo> <message> \nMessage: ");
                        fflush(stdout);
                        continue;
                    }
                }
                else{
                    printf("/msg <pseudo> <message> \nMessage: ");
                        fflush(stdout);
                        continue;
                }

            }
            else{
                // Message normal
                msgstruct.type = ECHO_SEND;
                msgstruct.pld_len = strlen(buff);
                strncpy(msgstruct.infos, "", INFOS_LEN);
            }

            if (write_on_socket(sockfd, &msgstruct, sizeof(msgstruct)) <= 0) {
                close(sockfd);
                break;
            }

            if (msgstruct.pld_len > 0) {
                if (write_on_socket(sockfd, buff, msgstruct.pld_len) <= 0) {
                    close(sockfd);
                    break;
                }
            }
            printf("Message sent!\n");
            printf("Message: ");
            fflush(stdout);
        }
        else if(fds[1].revents & POLLIN){
            fds[1].revents = 0;

            memset(&msgstruct, 0, sizeof(struct message));
            memset(buff, 0, MSG_LEN);
            // Receiving structure
            if (read_on_socket(sockfd, &msgstruct, sizeof(struct message)) <= 0) {
                close(sockfd);
                break;
            }
            // Receiving message
            if (msgstruct.pld_len > 0) {
                if (read_on_socket(sockfd, buff, msgstruct.pld_len) <= 0) {
                    close(sockfd);
                    break;
                }
            }
            printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
            printf("Received: %s", buff);

            if (msgstruct.type == NICKNAME_NEW) {
                strncpy(my_pseudo, msgstruct.infos, NICK_LEN - 1);
                my_pseudo[NICK_LEN -1] = '\0';
            }

            printf("\nMessage: ");
            fflush(stdout);
        }	
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
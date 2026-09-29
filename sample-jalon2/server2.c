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
#include <time.h>

#define FDS_SIZE 128

#include "common.h"
#include "msg_struct.h"

struct client_node {
    int fd;
    char ip[INET_ADDRSTRLEN]; //tableau de carac pour @ip sous forme de texte elle vaut 16
    int port;
    char nick[NICK_LEN];
    struct client_node *next;
    time_t time_sconnected;
};
struct client_node *client_list = NULL;

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

void add_client(int fd, const char *ip, int port){
    struct client_node * noeud = malloc(sizeof(struct client_node));
    if (noeud == NULL){
        return;
    }
    noeud->fd = fd;
    noeud->port = port;
    strncpy(noeud->ip, ip, INET_ADDRSTRLEN); // c'est strcpy mais on limite la taille pour pas que ça déborde dans la RAM.
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
         printf("echec de remove client");
        return;   
}

void destroy_list(){
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

void handle_new_connection(struct pollfd *fds){
    struct sockaddr_in client_addr; //on crée la boite vide
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(fds[0].fd, (struct sockaddr *)&client_addr, &client_len);
    die(client_fd, "accept");
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN); // IPV4, L'adresse de l'ip écrit en posix, la ou on va la convertir en compréhensible, taille max
    int port = ntohs(client_addr.sin_port); // ntop bloc binaire -> chaine carac ok | ntohs little/big endian -> ca le met dans le bon ordre
    printf("Nouveau client : IP %s, Port %d, FD %d\n", ip_str, port, client_fd);

    size_t j; // initialise avant la boucle pour le tester après for
    for(j = 1; j < FDS_SIZE; j++)
        {
        if (fds[j].fd == -1)
            {
            fds[j].fd = client_fd;
            fds[j].events = POLLIN;
            fds[j].revents = 0;
            add_client(fds[j].fd, ip_str, port);
            break;
            }
        }
    if (j == FDS_SIZE){
        printf("Serveur plein erreur\n");
        close(client_fd);
    }
}

void handle_message(struct pollfd *fds, int i){ // ASTUCE : remplacer les continue par des return
    
    struct message msgstruct;
	char buff[MSG_LEN];
    memset(&msgstruct, 0, sizeof(struct message));
    memset(buff, 0, MSG_LEN);
    // Receiving structure
    if (read_on_socket(fds[i].fd, &msgstruct, sizeof(struct message)) <= 0) {
        disconnecte_client(fds, i);
        return;
    }

    msgstruct.infos[INFOS_LEN -1 ] = '\0';
    msgstruct.nick_sender[NICK_LEN -1 ] = '\0';

    if (msgstruct.pld_len >= MSG_LEN){ //on évite fuite de mémoire mais techniquement un payload pourrait être de taille entre MSG_LEN et PROTO_MAX_PAYLOAD.
        printf("Erreur payload too big");
        disconnecte_client(fds, i);
        return;
    }

    // Receiving message
    if (msgstruct.pld_len > 0){
        if (read_on_socket(fds[i].fd, buff, msgstruct.pld_len) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
    }
    char * mot_recu = buff;
    if (msgstruct.pld_len < MSG_LEN){
        mot_recu[msgstruct.pld_len] = '\0'; //on évite de segault
    }
    else{
        mot_recu[MSG_LEN - 1] = '\0';
    }
    
    if (strcmp(mot_recu, "/quit") == 0){
        disconnecte_client(fds, i);
        return;
        }

    printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
    printf("Received: %s\n", buff);
    
    if (msgstruct.type == NICKNAME_NEW) {
        struct client_node *current = client_list;
        while (current != NULL){
            if (strcmp(msgstruct.infos, current->nick) == 0){
                char reply[MSG_LEN];
                if (current->fd == fds[i].fd){
                    snprintf(reply, MSG_LEN, "[Serveur] : error same pseudo %s\n", msgstruct.infos);
                }
                else{
                    snprintf(reply, MSG_LEN, "[Serveur] : a user already has this pseudo %s\n", msgstruct.infos);
                }
                struct message rep_struct;
                memset(&rep_struct, 0, sizeof(struct message));
                rep_struct.type = NICKNAME_NEW;
                rep_struct.pld_len = strlen(reply);
                strncpy(rep_struct.nick_sender, "Server", NICK_LEN - 1);

                strncpy(rep_struct.infos, current->nick, INFOS_LEN - 1);

                if (write_on_socket(fds[i].fd, &rep_struct, sizeof(rep_struct)) <= 0) {
                    disconnecte_client(fds, i);
                    return;
                }
                if (write_on_socket(fds[i].fd, reply, rep_struct.pld_len) <= 0) {
                    disconnecte_client(fds, i);
                    return;
                }
                return;
            }
            else{
                current = current->next;
            }
        }
        struct client_node *curr = client_list;
        while (curr != NULL && curr->fd != fds[i].fd) {
            curr = curr->next;
        }

        if (curr != NULL) {
            strncpy(curr->nick, msgstruct.infos, NICK_LEN - 1);
            printf("Client fd %d a pris le pseudo : %s\n", curr->fd, curr->nick);
        }

        char reply[MSG_LEN];
        snprintf(reply, MSG_LEN, "[Serveur] : Welcome on the chat %s\n", msgstruct.infos);

        struct message rep_struct;
        memset(&rep_struct, 0, sizeof(struct message));
        rep_struct.type = NICKNAME_NEW;
        rep_struct.pld_len = strlen(reply);
        strncpy(rep_struct.nick_sender, "Server", NICK_LEN - 1);
        if (curr != NULL){ // WOW merci gdb
            strncpy(rep_struct.infos, curr->nick, INFOS_LEN - 1);
        }

        if (write_on_socket(fds[i].fd, &rep_struct, sizeof(rep_struct)) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
        if (write_on_socket(fds[i].fd, reply, rep_struct.pld_len) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
        return;
    }
    else if (msgstruct.type == ECHO_SEND) {
        // Sending structure (ECHO)
        if (write_on_socket(fds[i].fd, &msgstruct, sizeof(msgstruct)) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
        // Sending message (ECHO)
        if(msgstruct.pld_len > 0){
            if (write_on_socket(fds[i].fd, buff, msgstruct.pld_len) <= 0) {
                disconnecte_client(fds, i);
                return;
            }
        }
        printf("Message sent!\n");
    }
    else if(msgstruct.type == NICKNAME_LIST){
        char reply[MSG_LEN];
        snprintf(reply, MSG_LEN, "[Server] : Online users are\n");
        struct client_node *curr = client_list;
        while (curr != NULL){
            if (strlen(curr->nick) > 0){ //le client a un pseudo
                snprintf(reply + strlen(reply), MSG_LEN - strlen(reply), "        - %s\n", curr->nick);
            }
            else if (strlen(curr->nick) == 0){
                snprintf(reply + strlen(reply), MSG_LEN - strlen(reply), "        - anonymous\n");
            }
            curr = curr->next;
        }
        struct message rep_struct;
        memset(&rep_struct, 0, sizeof(struct message));
        rep_struct.type = NICKNAME_LIST;
        rep_struct.pld_len = strlen(reply);
        strncpy(rep_struct.nick_sender, "Server", NICK_LEN - 1);

        if (write_on_socket(fds[i].fd, &rep_struct, sizeof(rep_struct)) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
        if (write_on_socket(fds[i].fd, reply, rep_struct.pld_len) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
        return;

    }
        else if(msgstruct.type == NICKNAME_INFOS){
        char reply[MSG_LEN];

        struct client_node *curr = client_list;
        int trouve = 0;
        while (curr != NULL){
            if (strcmp(curr->nick, msgstruct.infos) == 0){

                struct tm *timeinfo = localtime(&curr->time_sconnected); //struct de time qui contient year mois jour etc | localtime convertit le gros nombre en sec dans la struct en prenant compte le fuseau horaire de la machine
                char time_str[64]; // dans la struct les variables sont séparés printf complexe donc il existe strftime
                strftime(time_str, sizeof(time_str), "%Y/%m/%d@%H:%M", timeinfo); //tableau, taille, forme, struct ou les var sont


                snprintf(reply, MSG_LEN, "[Server] : %s is connected since %s from %s %d\n", curr->nick, time_str, curr->ip, curr->port);
                trouve = 1;
            }
            curr = curr->next;
        }
        if (trouve == 0){
            snprintf(reply, MSG_LEN, "[Server] : %s does not exist\n", msgstruct.infos);
        }
        struct message rep_struct;
        memset(&rep_struct, 0, sizeof(struct message));
        rep_struct.type = NICKNAME_INFOS;
        rep_struct.pld_len = strlen(reply);
        strncpy(rep_struct.nick_sender, "Server", NICK_LEN - 1);

        if (write_on_socket(fds[i].fd, &rep_struct, sizeof(rep_struct)) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
        if (write_on_socket(fds[i].fd, reply, rep_struct.pld_len) <= 0) {
            disconnecte_client(fds, i);
            return;
        }
        return;

    }
    else if(msgstruct.type == UNICAST_SEND){
        struct client_node *curr = client_list;
        int find = 0;
        while (curr != NULL){
            if (strcmp(curr->nick, msgstruct.infos) == 0){ 
                find = 1;
                if (write_on_socket(curr->fd, &msgstruct, sizeof(msgstruct)) <= 0){
                    disconnecte_client(fds, i);
                    return;
                }
                if (msgstruct.pld_len > 0){
                    if (write_on_socket(curr->fd, buff, msgstruct.pld_len) <= 0) {
                    disconnecte_client(fds, i);
                    return;
                    }
                break;

                }
            }
            curr = curr->next;
        }

        if (find == 0){
            char reply[MSG_LEN];
            snprintf(reply, MSG_LEN, "[Server] : user %s does not exist\n", msgstruct.infos);
            struct message rep_struct;
            memset(&rep_struct, 0, sizeof(struct message));
            rep_struct.type = UNICAST_SEND;
            rep_struct.pld_len = strlen(reply);
            strncpy(rep_struct.nick_sender, "Server", NICK_LEN - 1);

            if (write_on_socket(fds[i].fd, &rep_struct, sizeof(rep_struct)) <= 0) {
                disconnecte_client(fds, i);
                return;
            }
            if (write_on_socket(fds[i].fd, reply, rep_struct.pld_len) <= 0) {
                disconnecte_client(fds, i);
                return;
            }
        return;
        }
    }
    else if(msgstruct.type == BROADCAST_SEND){
        struct client_node *curr = client_list;
        while (curr != NULL){
            if (curr->fd != fds[i].fd){ // on envoie pas à lui même
                write_on_socket(curr->fd, &msgstruct, sizeof(msgstruct));

                if (msgstruct.pld_len > 0){
                    write_on_socket(curr->fd, buff, msgstruct.pld_len);
                }
            }
            curr = curr->next;
        }
    }
    else{
        printf("Erreur: type de message inconnue de la part %d\n", fds[i].fd);
        disconnecte_client(fds, i);
        return;

    }
}


void echo_server(int sfd) {

    struct pollfd fds[FDS_SIZE];
    fds[0].fd = sfd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    for (int i = 1; i < FDS_SIZE; i++){
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;    
    }

	while (1) {
        printf("Waiting for activity...\n");
        int nb_active = poll(fds, FDS_SIZE, -1);
        die(nb_active, "Error on poll");
        printf("New active nb_fd = %d\n", nb_active);
        for(size_t i = 0; i < FDS_SIZE; i++)
        {
            if (i == 0 && fds[0].revents & POLLIN){
                fds[0].revents = 0;
                handle_new_connection(fds);
            }
            else if (fds[i].revents & POLLIN){
                fds[i].revents = 0;
                handle_message(fds, i);
            }
        }
    }
}

int handle_bind(char * server_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, server_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
            int yes=1;
            int ret_value = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
            die(ret_value, "On setting socket options");
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
        perror("explication");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char * argv[]) {

    if (argc < 2){
        fprintf(stdout, "Error not enough arguments. Use :\n ./server2.c <server.port> \n<");
        exit(EXIT_FAILURE);
    }
    if (argc > 2){
        fprintf(stdout, "Error to many arguments. Use :\n ./server2.c <server.port> \n<");
        exit(EXIT_FAILURE);
    }
	int sfd;
	sfd = handle_bind(argv[1]);
	if ((listen(sfd, SOMAXCONN)) != 0) {
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}
	echo_server(sfd);

    destroy_list();
	close(sfd);
	return EXIT_SUCCESS;
}
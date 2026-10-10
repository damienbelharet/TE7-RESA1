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
#include "user-utils.h"

void handle_new_connection(struct pollfd *fds)
{
    struct sockaddr_in client_addr; // on crée la boite vide
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(fds[0].fd, (struct sockaddr *)&client_addr, &client_len);
    die(client_fd, "accept");
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN); // IPV4, L'adresse de l'ip écrit en posix, la ou on va la convertir en compréhensible, taille max
    int port = ntohs(client_addr.sin_port);                               // ntop bloc binaire -> chaine carac ok | ntohs little/big endian -> ca le met dans le bon ordre
    printf("Nouveau client : IP %s, Port %d, FD %d\n", ip_str, port, client_fd);

    size_t j; // initialise avant la boucle pour le tester après for
    for (j = 1; j < FDS_SIZE; j++)
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
    if (j == FDS_SIZE)
    {
        printf("Serveur plein erreur\n");
        close(client_fd);
    }
}

struct client_node *find_client_by_nick(const char *nick)
{

    if (nick[0] == '\0')
    { // sinon /msg vers "" tombe sur un client anonyme
        return NULL;
    }

    struct client_node *curr = client_list;
    while (curr != NULL && strcmp(curr->nick, nick) != 0)
    {
        curr = curr->next;
    }
    return curr;
}

struct client_node *find_client_by_fd(const int fd){
    struct client_node *curr = client_list;
    while(curr != NULL && curr->fd != fd){
        curr = curr->next;
    }
    return curr;
}

int send_message_to_client(int fd, struct message *msg, char *payload)
{

    if (write_on_socket(fd, msg, sizeof(struct message)) <= 0)
    {
        return -1;
    }
    if (msg->pld_len > 0 && payload != NULL)
    {
        if (write_on_socket(fd, payload, msg->pld_len) <= 0)
        {
            return -1;
        }
    }
    return 0;
}

int send_server_reply(int fd, enum msg_type type, char *text, char *info)
{
    struct message server_reply;
    memset(&server_reply, 0, sizeof(struct message));
    strcpy(server_reply.nick_sender, "Server");
    server_reply.type = type;
    server_reply.pld_len = strlen(text);
    strncpy(server_reply.infos, info, INFOS_LEN - 1);

    int ret_value = send_message_to_client(fd, &server_reply, text);
    return ret_value;
}

void handle_message(struct pollfd *fds, int i)
{ 

    struct message msgstruct;
    char buff[MSG_LEN];
    memset(&msgstruct, 0, sizeof(struct message));
    memset(buff, 0, MSG_LEN);
    // Receiving structure
    if (read_on_socket(fds[i].fd, &msgstruct, sizeof(struct message)) <= 0)
    {
        disconnecte_client(fds, i);
        return;
    }

    msgstruct.infos[INFOS_LEN - 1] = '\0';
    msgstruct.nick_sender[NICK_LEN - 1] = '\0';

    if (msgstruct.pld_len >= MSG_LEN)
    { // on évite fuite de mémoire mais techniquement un payload pourrait être de taille entre MSG_LEN et PROTO_MAX_PAYLOAD.
        printf("Erreur payload too big");
        disconnecte_client(fds, i);
        return;
    }

    // Receiving message
    if (msgstruct.pld_len > 0)
    {
        if (read_on_socket(fds[i].fd, buff, msgstruct.pld_len) <= 0)
        {
            disconnecte_client(fds, i);
            return;
        }
    }
    char *mot_recu = buff;
    if (msgstruct.pld_len < MSG_LEN)
    {
        mot_recu[msgstruct.pld_len] = '\0'; // on évite de segault
    }
    else
    {
        mot_recu[MSG_LEN - 1] = '\0';
    }

    if (strcmp(mot_recu, "/quit") == 0)
    {
        disconnecte_client(fds, i);
        return;
    }

    printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
    printf("Received: %s\n", buff);

    switch (msgstruct.type)
    {
    case NICKNAME_NEW:
    {
        struct client_node *exist = find_client_by_nick(msgstruct.infos);
        char reply[MSG_LEN];
        if (exist != NULL)
        {
            if (exist->fd == fds[i].fd)
            {
                snprintf(reply, MSG_LEN, "[Serveur] : error same pseudo %s\n", msgstruct.infos);
            }
            else
            {
                snprintf(reply, MSG_LEN, "[Serveur] : a user already has this pseudo %s\n", msgstruct.infos);
            }
            if (send_server_reply(fds[i].fd, ECHO_SEND, reply, exist->nick) < 0)
            {
                disconnecte_client(fds, i);
            }
            return;
        }

        struct client_node *dest = find_client_by_fd(fds[i].fd);
        if (dest == NULL)
        { // WOW merci gdb
            printf("Erreur : client fd %d introuvable\n", fds[i].fd);
            return;
        }
        strncpy(dest->nick, msgstruct.infos, NICK_LEN - 1);
        printf("Client fd %d a pris le pseudo : %s\n", dest->fd, dest->nick);

        snprintf(reply, MSG_LEN, "[Serveur] : Welcome on the chat %s\n", msgstruct.infos);

        if (send_server_reply(fds[i].fd, NICKNAME_NEW, reply, dest->nick) < 0)
        {
            disconnecte_client(fds, i);
        }
        return;
    }

    case ECHO_SEND:
    {
        if (send_message_to_client(fds[i].fd, &msgstruct, buff) < 0)
        {
            disconnecte_client(fds, i);
            return;
        }
        printf("Message sent!\n");
        break;
    }

    case NICKNAME_LIST:
    {
        char reply[MSG_LEN];
        snprintf(reply, MSG_LEN, "[Server] : Online users are\n");
        struct client_node *curr = client_list;
        while (curr != NULL)
        {
            if (strlen(curr->nick) > 0)
            { // le client a un pseudo
                snprintf(reply + strlen(reply), MSG_LEN - strlen(reply), "        - %s\n", curr->nick);
            }
            else
            {
                snprintf(reply + strlen(reply), MSG_LEN - strlen(reply), "        - anonymous\n");
            }
            curr = curr->next;
        }

        if (send_server_reply(fds[i].fd, NICKNAME_LIST, reply, "") < 0)
        {
            disconnecte_client(fds, i);
        }
        return;
    }

    case NICKNAME_INFOS:
    {
        char reply[MSG_LEN];
        struct client_node *dest = find_client_by_nick(msgstruct.infos);
        if (dest == NULL)
        {
            snprintf(reply, MSG_LEN, "[Server] : %s does not exist\n", msgstruct.infos);
        }
        else
        {
            struct tm *timeinfo = localtime(&dest->time_sconnected);          // struct de time qui contient year mois jour etc | localtime convertit le gros nombre en sec dans la struct en prenant compte le fuseau horaire de la machine
            char time_str[64];                                                // dans la struct les variables sont séparés printf complexe donc il existe strftime
            strftime(time_str, sizeof(time_str), "%Y/%m/%d@%H:%M", timeinfo); // tableau, taille, forme, struct ou les var sont

            snprintf(reply, MSG_LEN, "[Server] : %s is connected since %s from %s %d\n", dest->nick, time_str, dest->ip, dest->port);
        }
        if (send_server_reply(fds[i].fd, NICKNAME_INFOS, reply, "") < 0)
        {
            disconnecte_client(fds, i);
        }
        return;
    }

    case UNICAST_SEND:
    case FILE_REQUEST:
    case FILE_REJECT:
    case FILE_ACCEPT:
    {
        struct client_node *dest = find_client_by_nick(msgstruct.infos);
        if (dest == NULL)
        {
            char reply[MSG_LEN];
            snprintf(reply, MSG_LEN, "[Server] : user %s does not exist\n", msgstruct.infos);

            if (send_server_reply(fds[i].fd, UNICAST_SEND, reply, "") < 0)
            {
                printf("Destinataire doesn't respond\n");
            }
            return;
        }
        if (send_message_to_client(dest->fd, &msgstruct, buff) < 0)
        {
            printf("Destinataire doesn't respond\n");
        }
        return;
    }

    case BROADCAST_SEND:
    {
        struct client_node *curr = client_list;
        while (curr != NULL)
        {
            if (curr->fd != fds[i].fd)
            { // on envoie pas à lui même
                send_message_to_client(curr->fd, &msgstruct, buff);
            }
            curr = curr->next;
        }
        break;
    }

    default:
        printf("Erreur: type de message inconnue de la part %d\n", fds[i].fd);
        disconnecte_client(fds, i);
        return;
    }
}

void echo_server(int sfd)
{

    struct pollfd fds[FDS_SIZE];
    fds[0].fd = sfd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    for (int i = 1; i < FDS_SIZE; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    while (1)
    {
        printf("Waiting for activity...\n");
        int nb_active = poll(fds, FDS_SIZE, -1);
        die(nb_active, "Error on poll");
        printf("New active nb_fd = %d\n", nb_active);
        for (size_t i = 0; i < FDS_SIZE; i++)
        {
            if (i == 0 && fds[0].revents & POLLIN)
            {
                fds[0].revents = 0;
                handle_new_connection(fds);
            }
            else if (fds[i].revents & POLLIN)
            {
                fds[i].revents = 0;
                handle_message(fds, i);
            }
        }
    }
}

int handle_bind(char *server_port)
{
    struct addrinfo hints, *result, *rp;
    int sfd;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, server_port, &hints, &result) != 0)
    {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
        {
            continue;
        }
        int yes = 1;
        int ret_value = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        die(ret_value, "On setting socket options");
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            break;
        }
        close(sfd);
    }
    if (rp == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        perror("explication");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);
    return sfd;
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        fprintf(stdout, "Error not enough arguments. Use :\n ./server2.c <server.port> \n<");
        exit(EXIT_FAILURE);
    }
    if (argc > 2)
    {
        fprintf(stdout, "Error to many arguments. Use :\n ./server2.c <server.port> \n<");
        exit(EXIT_FAILURE);
    }
    int sfd;
    sfd = handle_bind(argv[1]);
    if ((listen(sfd, SOMAXCONN)) != 0)
    {
        perror("listen()\n");
        exit(EXIT_FAILURE);
    }
    echo_server(sfd);

    destroy_list();
    close(sfd);
    return EXIT_SUCCESS;
}
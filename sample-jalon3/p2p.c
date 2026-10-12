#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h> // close
#include <sys/stat.h>

#include "msg_struct.h"
#include "common.h"

#define size_morceaux 1024

int receive_file(int listen_fd)
{

    int p2p_fd = accept(listen_fd, NULL, NULL);
    if (p2p_fd == -1)
    {
        perror("accept");
        return -1;
    }

    mkdir(".resa1", 0755);
    mkdir(".resa1/inbox", 0755);

    struct message msg;
    char morceau[size_morceaux];
    char path[MSG_LEN];
    FILE *f = NULL;
    int ret = -1;

    while (1)
    {

        if (read_on_socket(p2p_fd, &msg, sizeof(msg)) <= 0)
        {
            printf("Connection lost during reading\n");
            break;
        }
        if (msg.type != FILE_SEND || msg.pld_len < 0 || msg.pld_len > size_morceaux)
        {
            printf("Invalid message during reading\n");
            break;
        }
        if (f == NULL)
        {
            char *name = msg.infos;
            snprintf(path, sizeof(path), ".resa1/inbox/%s", name);
            f = fopen(path, "wb");
            if (f == NULL)
            {
                perror("fopen");
                break;
            }
            printf("Receiving the file from %s\n", msg.nick_sender);
        }

        if (msg.pld_len == 0)
        {
            ret = 0;
            break;
        }

        if (read_on_socket(p2p_fd, morceau, msg.pld_len) <= 0)
        {
            printf("Connection lost during transfer\n");
            break;
        }
        if (fwrite(morceau, 1, msg.pld_len, f) != (size_t)msg.pld_len)
        {
            perror("fwrite");
            break;
        }
    }

    if (f != NULL)
    {
        fclose(f);
    }
    if (ret == 0)
    {
        printf("File saved in %s\n", path);

        struct message ack;
        memset(&ack, 0, sizeof(struct message));
        ack.type = FILE_ACK;
        strncpy(ack.infos, msg.infos, INFOS_LEN - 1);
        ack.pld_len = 0;
        write_on_socket(p2p_fd, &ack, sizeof(msg));
    }

    close(p2p_fd);
    return ret;
}

int create_listen_sock(int *port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("create_listen_socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0); // 0 permet de choisir un port libre

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 1) == -1)
    { // on attend qu'une connexion
        perror("listen");
        close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) == -1)
    {
        perror("getsockname");
        close(fd);
        return -1;
    }

    *port = ntohs(addr.sin_port);
    return fd;
}

int send_file(int p2p_fd, char *path, char *my_pseudo)
{

    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        perror("fopen");
        return -1;
    }

    char *name = strrchr(path, '/'); // dernier '/' du chemin
    if (name == NULL)
    {
        name = path;
    }
    else
    {
        name += 1;
    }

    struct message msg;
    memset(&msg, 0, sizeof(struct message));
    msg.type = FILE_SEND;
    strncpy(msg.nick_sender, my_pseudo, NICK_LEN - 1);
    strncpy(msg.infos, name, INFOS_LEN - 1);

    char morceau[size_morceaux];
    size_t lus;

    while ((lus = fread(morceau, 1, size_morceaux, f)) > 0)
    {
        msg.pld_len = lus;

        if (write_on_socket(p2p_fd, &msg, sizeof(msg)) < 0)
        {
            perror("error write_on_socket from sending file");
            fclose(f);
            return -1;
        }
        if (write_on_socket(p2p_fd, morceau, msg.pld_len) < 0)
        {
            perror("error write_on_socket from sending file");
            fclose(f);
            return -1;
        }
    }
    msg.pld_len = 0;
    if (write_on_socket(p2p_fd, &msg, sizeof(msg)) < 0)
    {
        perror("error write_on_socket during sending file");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}
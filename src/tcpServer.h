#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "node.h"

typedef struct s_client {
    //File descriptor
    int fd;

    int id;

    //Message buffer
    char *msg;

    //Points to next client in linked list structure
    struct s_client *next;
} t_client;

typedef struct s_server {
    //File descriptor for active socket
    int sockfd;

    //Maximum file descriptor value in use for select function
    int max_fd;
    
    int port;

    //Counts connections to assign id to clients 
    int counter;

    fd_set readfds, writefds, active_fds;

    struct sockaddr_in addr;
    
    //First client in linked list structure, keeps track of clients connected to the server
    t_client *head;

    Node* localNode;

    pthread_mutex_t lock;
} t_server;

void fatalError(t_server *s);
void handle_command(t_server *s, t_client *cli, char *msg);
int extract_message(char **buf, char **msg);

char *str_join(char *buf, char *add);
void freeClient(t_client *cli);

t_client *addClient(t_server *s, int fd);
t_client *findClient(t_server *s, int fd);

void removeClient(t_server *s, int fd);
void deleteAll(t_server *s);

void fatalError(t_server *s);
void sendNotification(t_server *s, int fd, char *msg);

void sendMessage(t_server *s, t_client *cli);
void deregisterClient(t_server *s, int fd, int cli_id);

void processMessage(t_server *s, int fd);
void registerClient(t_server *s, int fd);

void acceptRegistration(t_server *s);
void monitorFDs(t_server *s);
void handleCon(t_server *s);
void bindAndListen(t_server *s);
void configAddr(t_server *s);
void createSock(t_server *s);

t_server *initServer(int port, Node* node);

void *server_loop(void *arg);

void handle_command(t_server *s, t_client *cli, char *msg);

/*
TCPServer* start_tcp_server(const char* ip, int port);

typedef char*  (*request_handle_t)(const char* request);

void stop_tcp_server(TCPServer* server);
void free_tcp_server(TCPServer* server);

char* handle_request(const char* request);

struct sockaddr_in address;
*/

#endif
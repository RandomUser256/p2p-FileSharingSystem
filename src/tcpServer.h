#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

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

    //Maximum file descriptor value in use for select()
    int max_fd;
    
    int port;

    //Counts connections to assign id to clients 
    int counter;

    fd_set readfds, writefds, active_fds;

    struct sockaddr_in addr;
    
    //First client in linked list structure, keeps track of clients connected to the server
    t_client *head;

    Node* localNode;
} t_server;

TCPServer* start_tcp_server(const char* ip, int port);

typedef char*  (*request_handle_t)(const char* request);

void stop_tcp_server(TCPServer* server);
void free_tcp_server(TCPServer* server);

char* handle_request(const char* request);

struct sockaddr_in address;

#endif
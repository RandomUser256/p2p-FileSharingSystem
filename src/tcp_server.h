#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "tcp_protocol.h"

/* TCP Server Configuration */

typedef struct {
    int server_socket;
    int port;
    int running;
} TCPServer;

/* Initialize TCP server
 * Returns TCPServer struct, server_socket will be -1 on failure
 */
TCPServer* tcp_server_init(int port);

/* Start accepting connections (blocking)
 * callback is called for each client connection
 */
typedef char* (*request_handler_t)(const char* request);

void tcp_server_run(TCPServer* server, request_handler_t handler);

/* Stop the server */
void tcp_server_stop(TCPServer* server);

/* Free server resources */
void tcp_server_free(TCPServer* server);

/* Handle a single client connection
 * Returns response to send back to client (caller must free)
 */
char* handle_request(const char* request);

#endif /* TCP_SERVER_H */

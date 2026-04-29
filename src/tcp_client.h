#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "tcp_protocol.h"

/* TCP Client Connection Management */

/* Connect to a remote node via TCP
 * Returns socket file descriptor on success, -1 on failure
 */
int tcp_connect(const char* ip, int port);

/* Send a request and receive a response
 * Handles socket communication and cleanup
 * Returns dynamically allocated response string (caller must free), NULL on failure
 */
char* tcp_request_response(const char* ip, int port, const char* request);

/* Close a TCP connection */
void tcp_close(int sock);

/* Utility: Send data over socket
 * Returns number of bytes sent, -1 on error
 */
int tcp_send(int sock, const char* data);

/* Utility: Receive data from socket
 * Returns dynamically allocated buffer (caller must free), NULL on error
 */
char* tcp_receive(int sock);

#endif /* TCP_CLIENT_H */

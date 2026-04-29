#include "tcp_server.h"
#include "logger.h"
#include <errno.h>

TCPServer* tcp_server_init(int port) {
    TCPServer* server = malloc(sizeof(TCPServer));
    if (!server) {
        log_error("tcp_server_init: Memory allocation failed\n");
        return NULL;
    }

    server->port = port;
    server->running = 0;

    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        log_error("tcp_server_init: Failed to create socket: %s\n", strerror(errno));
        free(server);
        return NULL;
    }

    /* Allow socket reuse */
    int opt = 1;
    if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_error("tcp_server_init: Failed to set socket options: %s\n", strerror(errno));
        close(server->server_socket);
        free(server);
        return NULL;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("tcp_server_init: Failed to bind socket: %s\n", strerror(errno));
        close(server->server_socket);
        free(server);
        return NULL;
    }

    if (listen(server->server_socket, 5) < 0) {
        log_error("tcp_server_init: Failed to listen: %s\n", strerror(errno));
        close(server->server_socket);
        free(server);
        return NULL;
    }

    server->running = 1;
    log_info("tcp_server_init: Server listening on port %d\n", port);
    return server;
}

typedef struct {
    int client_socket;
    request_handler_t handler;
} ClientArgs;

void* client_thread(void* args) {
    ClientArgs* cargs = (ClientArgs*)args;
    int client_socket = cargs->client_socket;
    request_handler_t handler = cargs->handler;

    char buffer[TCP_BUFFER_SIZE];
    memset(buffer, 0, TCP_BUFFER_SIZE);

    int received = recv(client_socket, buffer, TCP_BUFFER_SIZE - 1, 0);
    if (received < 0) {
        log_error("client_thread: Failed to receive: %s\n", strerror(errno));
        close(client_socket);
        free(cargs);
        return NULL;
    }

    buffer[received] = '\0';
    log_debug("client_thread: Received request: %s\n", buffer);

    char* response = handler(buffer);
    if (!response) {
        response = malloc(50);
        snprintf(response, 50, "%s|Internal server error\n", RESP_ERROR);
    }

    if (send(client_socket, response, strlen(response), 0) < 0) {
        log_error("client_thread: Failed to send response: %s\n", strerror(errno));
    }

    free(response);
    close(client_socket);
    free(cargs);
    return NULL;
}

void tcp_server_run(TCPServer* server, request_handler_t handler) {
    if (!server || server->server_socket < 0 || !handler) {
        log_error("tcp_server_run: Invalid arguments\n");
        return;
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    log_info("tcp_server_run: Starting server\n");

    while (server->running) {
        int client_socket = accept(server->server_socket, 
                                   (struct sockaddr*)&client_addr, 
                                   &client_addr_len);

        if (client_socket < 0) {
            if (server->running) {
                log_error("tcp_server_run: Accept failed: %s\n", strerror(errno));
            }
            continue;
        }

        log_debug("tcp_server_run: Client connected from %s:%d\n", 
                  inet_ntoa(client_addr.sin_addr), 
                  ntohs(client_addr.sin_port));

        ClientArgs* cargs = malloc(sizeof(ClientArgs));
        if (!cargs) {
            log_error("tcp_server_run: Memory allocation failed\n");
            close(client_socket);
            continue;
        }

        cargs->client_socket = client_socket;
        cargs->handler = handler;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, cargs) != 0) {
            log_error("tcp_server_run: Failed to create thread\n");
            close(client_socket);
            free(cargs);
        } else {
            pthread_detach(tid);
        }
    }
}

void tcp_server_stop(TCPServer* server) {
    if (server) {
        server->running = 0;
        if (server->server_socket >= 0) {
            shutdown(server->server_socket, SHUT_RDWR);
        }
    }
}

void tcp_server_free(TCPServer* server) {
    if (server) {
        if (server->server_socket >= 0) {
            close(server->server_socket);
        }
        free(server);
    }
}

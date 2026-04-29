#include "tcp_client.h"
#include "logger.h"
#include <errno.h>

int tcp_connect(const char* ip, int port) {
    struct sockaddr_in server_addr;
    int sock;
    struct timeval tv;

    if (!ip) {
        log_error("tcp_connect: Invalid IP address\n");
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_error("tcp_connect: Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    /* Set socket timeout */
    tv.tv_sec = TCP_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        log_error("tcp_connect: Invalid IP format: %s\n", ip);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("tcp_connect: Failed to connect to %s:%d: %s\n", ip, port, strerror(errno));
        close(sock);
        return -1;
    }

    log_debug("tcp_connect: Connected to %s:%d\n", ip, port);
    return sock;
}

int tcp_send(int sock, const char* data) {
    if (sock < 0 || !data) {
        return -1;
    }

    int sent = send(sock, data, strlen(data), 0);
    if (sent < 0) {
        log_error("tcp_send: Failed to send data: %s\n", strerror(errno));
        return -1;
    }

    log_debug("tcp_send: Sent %d bytes\n", sent);
    return sent;
}

char* tcp_receive(int sock) {
    if (sock < 0) {
        return NULL;
    }

    char* buffer = malloc(TCP_BUFFER_SIZE);
    if (!buffer) {
        log_error("tcp_receive: Memory allocation failed\n");
        return NULL;
    }

    memset(buffer, 0, TCP_BUFFER_SIZE);
    int received = recv(sock, buffer, TCP_BUFFER_SIZE - 1, 0);

    if (received < 0) {
        log_error("tcp_receive: Failed to receive data: %s\n", strerror(errno));
        free(buffer);
        return NULL;
    }

    if (received == 0) {
        log_warn("tcp_receive: Connection closed by remote\n");
        free(buffer);
        return NULL;
    }

    buffer[received] = '\0';
    log_debug("tcp_receive: Received %d bytes: %s\n", received, buffer);
    return buffer;
}

char* tcp_request_response(const char* ip, int port, const char* request) {
    if (!ip || !request) {
        log_error("tcp_request_response: Invalid arguments\n");
        return NULL;
    }

    int sock = tcp_connect(ip, port);
    if (sock < 0) {
        return NULL;
    }

    if (tcp_send(sock, request) < 0) {
        close(sock);
        return NULL;
    }

    char* response = tcp_receive(sock);
    close(sock);

    return response;
}

void tcp_close(int sock) {
    if (sock >= 0) {
        close(sock);
    }
}

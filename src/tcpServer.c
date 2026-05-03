#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/*
tutorial for code: https://medium.com/@oduwoledare/server-side-story-creating-a-multi-client-tcp-server-with-c-and-select-3692db1a8ca3
*/

#include "tcpServer.h"
#include "logger.h"
#include "node.h"

void fatalError(t_server *s);
void handle_command(t_server *s, t_client *cli, char *msg);

//Extract message from client's message buffer, returning 1 if a complete message was extracted, 0 if not and -1 if an error occurred
int extract_message(char **buf, char **msg) {
    char *newbuf;
    int i;

    *msg = 0;
    if (*buf == 0) return (0);
    i = 0;
    while ((*buf)[i]) {
        if ((*buf)[i] == '\n') {
            newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
            
            if (newbuf == 0) return (-1);
            strcpy(newbuf, *buf + i + 1);
            *msg = *buf;
            (*msg)[i + 1] = 0;
            *buf = newbuf;
            return (1);
        }
        i++;
    }
    return (0);
}

char *str_join(char *buf, char *add) {
    char *newbuf;
    int  len;

    if (buf == 0)
    len = 0;
    else
    len = strlen(buf);
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
    return (0);
    newbuf[0] = 0;
    if (buf != 0)
    strcat(newbuf, buf);
    free(buf);
    strcat(newbuf, add);
    return (newbuf);
}

void freeClient(t_client *cli) {
    if (cli) {
        if (cli->msg) free(cli->msg);
        if (cli->fd > 0) close(cli->fd);
        free(cli);
    }
}

//Add client to linked list of clients in the server structure
t_client *addClient(t_server *s, int fd) {
    t_client *cli = (t_client *)malloc(sizeof(t_client));
    if (!cli) fatalError(s);
    bzero(cli, sizeof(t_client));

    //Initialize client structure with file descriptor, unique ID and message buffer
    cli->fd = fd;
    cli->id = s->counter++;
    cli->msg = NULL;
    
    //Adds to linked list of clients
    cli->next = s->head;
    s->head = cli;
    return cli;
}

//Find a specific client based on its file descriptor
t_client *findClient(t_server *s, int fd) {
    t_client *tmp = s->head;

    while (tmp && tmp->fd != fd)
        tmp = tmp->next;
    return tmp;
}

void removeClient(t_server *s, int fd) {
    t_client *tmp = s->head;
    t_client *prev = NULL;

    while (tmp && tmp->fd != fd) {
        prev = tmp;
        tmp = tmp->next;
    }
    if (tmp) {
        if (prev)
            prev->next = tmp->next;
        else
            s->head = tmp->next;
        freeClient(tmp);
    }
}

//Frees all clients in linked list and closes server socket
void deleteAll(t_server *s) {
    t_client *tmp = s->head;

    while (tmp) {
        t_client *cache = tmp;
        tmp = tmp->next;
        freeClient(cache);
    }
    if (s->sockfd > 0) {
        close(s->sockfd);
        s->sockfd = -1;
    }
    free(s);
    s = NULL;
}

//Shutdowns whole server, closing all client connections and freeing all memory
void fatalError(t_server *s) {
    deleteAll(s);
    write(2, "Fatal error\n", 12);
    exit(1);
}

//Sends a message to all clients except the one specified by fd
void sendNotification(t_server *s, int fd, char *msg) {
    t_client *cli = s->head;
    while (cli) {
        if (FD_ISSET(cli->fd, &s->writefds) && cli->fd != fd)
            if (send(cli->fd, msg, strlen(msg), 0) < 0) fatalError(s);
        cli = cli->next;
    }
}

void sendMessage(t_server *s, t_client *cli) {
    char buf[127];
    char *msg;
    while (extract_message(&cli->msg, &msg))
    {
        if (FD_ISSET(cli->fd, &s->writefds)) {
            /*
            sprintf(buf, "client %d: ", cli->id);
            sendNotification(s, cli->fd, buf);
            sendNotification(s, cli->fd, msg);
            */

            handle_command(s, cli, msg);  // Handle the command received from the client
            free(msg);
        }
    }
}

//Removes client from linked list and sends a notification to all clients about the disconnection
void deregisterClient(t_server *s, int fd, int cli_id) {
    char buf[127];
    sprintf(buf, "server: client %d just left\n", cli_id);
    sendNotification(s, fd, buf);
    FD_CLR(fd, &s->active_fds);
    removeClient(s, fd);
}

//Processes a message received from a client, extracting complete messages and sends response to client
void processMessage(t_server *s, int fd) {
    char buf[4096];
    t_client *cli = findClient(s, fd);
    if (!cli) return;

    int read_bytes = recv(fd, buf, sizeof(buf) - 1, 0);
    
    if (read_bytes <= 0) {
        deregisterClient(s, fd, cli->id);
    } else {
        buf[read_bytes] = '\0';
        cli->msg = str_join(cli->msg, buf);
        sendMessage(s, cli);
    }
}


void registerClient(t_server *s, int fd) {
    t_client *cli = addClient(s, fd);
    char buf[127];
    if (!cli) fatalError(s);
    FD_SET(cli->fd, &s->active_fds);
    if (cli->fd > s->max_fd)
        s->max_fd = cli->fd;
    sprintf(buf, "server: client %d just arrived\n", cli->id);
    sendNotification(s, fd, buf);
}

//Accepts a new connection, adds it to the linked list of clients and sends a notification to all clients about the new connection
void acceptRegistration(t_server *s) {
    struct sockaddr_in  cli; 

    socklen_t len = sizeof(cli);
    int fd = accept(s->sockfd, (struct sockaddr *)&cli, &len);
    if (fd < 0) fatalError(s);
    registerClient(s, fd);
}

//Monitors file descriptors for incoming connections and messages, and processes them accordingly
void monitorFDs(t_server *s) {
    if (select(s->max_fd + 1, &s->readfds, &s->writefds, NULL, NULL) < 0) fatalError(s);
    int fd = 0;
    while (fd <= s->max_fd)
    {
        //Refresh readfds and writefds for each iteration of the loop, to determine which ones need attention
        if (FD_ISSET(fd, &s->readfds))
            //If the file descriptor is the server socket, it means a new connection is incoming, so we call acceptRegistration to handle it. Otherwise, it's an existing client sending a message, so we call processMessage to handle the message.
            (fd == s->sockfd) ? acceptRegistration(s) : processMessage(s, fd);
        fd++;
    }
}

//Main loop that continuously monitors file descriptors for incoming connections and messages, and processes them accordingly
void handleCon(t_server *s) {
    while (1)
    {
        s->readfds = s->active_fds;
        s->writefds = s->active_fds;
        monitorFDs(s);
    }
}

//Bind the server socket to the configured address and port, and start listening for incoming connections
void bindAndListen(t_server *s) {
    (void)s;
    if ((bind(s->sockfd, (const struct sockaddr *)&s->addr, sizeof(s->addr)))) fatalError(s);
    if (listen(s->sockfd, SOMAXCONN)) fatalError(s);
}

//Configure server address structure with specified port and localhost IP
void configAddr(t_server *s) {
 bzero(&s->addr, sizeof(s->addr)); 
 s->addr.sin_family = AF_INET; 
 s->addr.sin_addr.s_addr = INADDR_ANY;
 s->addr.sin_port = htons(s->port); 
}

//Create socket and add it to the set of active file descriptors for select()
void createSock(t_server *s) {
    s->sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (s->sockfd < 0)  fatalError(s);

    FD_SET(s->sockfd, &s->active_fds);
    s->max_fd = s->sockfd;
}

t_server *initServer(int port, Node* node) {
    t_server *s = (t_server *)malloc(sizeof(t_server));
    if (!s) fatalError(NULL);
    bzero(s, sizeof(t_server));
    FD_ZERO(&s->active_fds);
    FD_ZERO(&s->readfds);
    FD_ZERO(&s->writefds);
    s->port = port;

    s->localNode = node;

    pthread_mutex_init(&s->lock, NULL);
    return s;
}

void *server_loop(void *arg) {
    t_server *s = (t_server *)arg;

    while (1) {
        s->readfds = s->active_fds;
        s->writefds = s->active_fds;
        monitorFDs(s);
    }

    return NULL;
}

void handle_command(t_server *s, t_client *cli, char *msg) {
    char cmd[64];

    int defaultSock = cli->id;
    int defaultPort = 8080;

    if (sscanf(msg, "%63s", cmd) != 1) {
        log_error("Invalid command received by tcp server");
        return;
    }

    // --------------------
    // JOIN
    // --------------------
    if (strcmp(cmd, "JOIN") == 0) {
        char ip[64];

        if (sscanf(msg, "JOIN %63s", ip) == 1) {
            remote_join(ip, s->port, s);  // your existing function
            send(cli->fd, "OK\n", 3, 0);
        } else {
            send(cli->fd, "ERROR Invalid JOIN\n", 19, 0);
        }
    }

    else if (strcmp(cmd, "STABILIZE") == 0) {
        char existingIp[64];
        if (sscanf(msg, "STABILIZE %63s", existingIp) == 1) {
            remote_notify(s, defaultPort, existingIp);
            send(cli->fd, "OK\n", 3, 0);
        } else {
            send(cli->fd, "Invalid arguments given for stabilize\n", 3, 0);
        }
    }

    // --------------------
    // FIND_SUCCESSOR
    // --------------------
    else if (strcmp(cmd, "FIND_SUCCESSOR") == 0) {
        int id;

        if (sscanf(msg, "FIND_SUCCESSOR %d", &id) == 1) {
            pthread_mutex_lock(&s->lock);
            Node* succ = find_successor(s->localNode, id);
            int succ_id = succ->id;
            char succ_ip[64];
            strncpy(succ_ip, succ->Ip, sizeof(succ_ip) - 1);
            succ_ip[sizeof(succ_ip) - 1] = '\0';
            pthread_mutex_unlock(&s->lock);

            char buf[128];
            snprintf(buf, sizeof(buf), "NODE %d %s\n", succ_id, succ_ip);
            send(cli->fd, buf, strlen(buf), 0);
        } else {
            send(cli->fd, "ERROR Invalid FIND_SUCCESSOR\n", 31, 0);
        }
    }

    /*
    else if (strcmp(cmd, "FIND_PREDECESSOR") == 0) {
        int id;

        if (sscanf(msg, "FIND_PREDECESSOR %d", &id) == 1) {
            Node* temp = s->localNode;

            while (!half_left_open_interval(temp->id, temp->id, temp->successor->id)) {

                temp = remote_closest_preceding_finger(,temp->id)
            }

            send(cli->fd, "OK\n", 3, 0);
        } else {
            send(cli->fd, "ERROR Invalid FIND_PREDECESSOR\n", 31, 0);
        }
    }
        */

    else if (strcmp(cmd, "CLOSEST_PRECEDING_FINGER") == 0) {
        int id;

        if (sscanf(msg, "CLOSEST_PRECEDING_FINGER %d", &id) == 1) {
            pthread_mutex_lock(&s->lock);
            Node* cpf = closest_preceding_finger(s->localNode, id);
            int cpf_id = cpf->id;
            char cpf_ip[64];
            strncpy(cpf_ip, cpf->Ip, sizeof(cpf_ip) - 1);
            cpf_ip[sizeof(cpf_ip) - 1] = '\0';
            pthread_mutex_unlock(&s->lock);

            char buf[128];
            snprintf(buf, sizeof(buf), "NODE %d %s\n", cpf_id, cpf_ip);
            send(cli->fd, buf, strlen(buf), 0);
        } else {
            send(cli->fd, "ERROR Invalid CLOSEST_PRECEDING_FINGER\n", 31, 0);
        }
    }

    // --------------------
    // GET_NODE
    // --------------------
    else if (strcmp(cmd, "GET_NODE") == 0) {
        pthread_mutex_lock(&s->lock);
        int node_id = s->localNode->id;
        char node_ip[64];
        strncpy(node_ip, s->localNode->Ip, sizeof(node_ip) - 1);
        node_ip[sizeof(node_ip) - 1] = '\0';
        Node* succ_ref = s->localNode->successor ? s->localNode->successor : s->localNode;
        int succ_id = succ_ref->id;
        char succ_ip[64];
        strncpy(succ_ip, succ_ref->Ip, sizeof(succ_ip) - 1);
        succ_ip[sizeof(succ_ip) - 1] = '\0';
        Node* pred_ref = s->localNode->predecessor ? s->localNode->predecessor : s->localNode;
        int pred_id = pred_ref->id;
        char pred_ip[64];
        strncpy(pred_ip, pred_ref->Ip, sizeof(pred_ip) - 1);
        pred_ip[sizeof(pred_ip) - 1] = '\0';
        pthread_mutex_unlock(&s->lock);
        
        char buf[128];
        snprintf(buf, sizeof(buf), "NODE %d %.15s %d %.15s %d %.15s\n",
                 node_id, node_ip, succ_id, succ_ip, pred_id, pred_ip);
        send(cli->fd, buf, strlen(buf), 0);
    }

    // --------------------
    // CHECK_RING
    // --------------------
    else if (strcmp(cmd, "CHECK_RING") == 0) {
        int startingNode = 0;

        if (sscanf(msg, "CHECK_RING %d", &startingNode) == 1) {
            // Stop propagation when the walk returns to the originating node
            if (s->localNode->id == startingNode) {
                log_info("Completed ring check at node %d with IP: %s", s->localNode->id, s->localNode->Ip);
                return;
            }

            if (s->localNode->predecessor == NULL) {
                log_warn("CHECK_RING: predecessor is NULL at node %d, ring not fully formed", s->localNode->id);
                return;
            }

            Node* tempSucc = remote_get_node(s, s->port, s->localNode->successor->Ip);
            Node* tempPred = remote_get_node(s, s->port, s->localNode->predecessor->Ip);

            if (tempSucc == NULL || tempPred == NULL) {
                log_error("[ERROR] CHECK_RING: could not fetch neighbour info at node %d\n", s->localNode->id);
                if (tempSucc) freeNode(tempSucc);
                if (tempPred) freeNode(tempPred);
                return;
            }

            // Verify successor's predecessor points back to us
            if (tempSucc->predecessor == NULL || tempSucc->predecessor->id != s->localNode->id)
                log_error("[ERROR] successor->predecessor mismatch at node %d\n", s->localNode->id);

            // Verify predecessor's successor points back to us
            if (tempPred->successor == NULL || tempPred->successor->id != s->localNode->id)
                log_error("[ERROR] predecessor->successor mismatch at node %d\n", s->localNode->id);

            int sock = init_socket(s->localNode->successor->Ip, s->port);

            if (sock < 0) {
                log_error("CHECK_RING: could not connect to successor node %d at %s",
                          s->localNode->successor->id, s->localNode->successor->Ip);
                freeNode(tempSucc);
                freeNode(tempPred);
                return;
            }

            char request[64];
            snprintf(request, sizeof(request), "CHECK_RING %d\n", startingNode);

            if (send(sock, request, strlen(request), 0) < 0) {
                perror("send");
                close(sock);
                freeNode(tempSucc);
                freeNode(tempPred);
                return;
            }

            close(sock);
            freeNode(tempSucc);
            freeNode(tempPred);
        } else {
            log_warn("Invalid CHECK_RING command at node %d", s->localNode->id);
        }
    }
    else {
        send(cli->fd, "ERROR Unknown command\n", 23, 0);
    }
}

/*
int main(int ac, char **av) {
    if (ac != 2) {
        write(2, "Wrong number of argument\n", 26);
        exit(1);
    }
    int port = atoi(av[1]);
    if (port <= 0 || port > 65535) fatalError(NULL);
    t_server *serv = initServer(port);
    if (serv) {
        createSock(serv);
        configAddr(serv);
        bindAndListen(serv);
        handleCon(serv);
        deleteAll(serv);
    }
    return (0);
}
    */
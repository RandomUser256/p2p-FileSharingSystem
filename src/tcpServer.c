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
        t_client *next = cli->next; // save before possible removal
        if (FD_ISSET(cli->fd, &s->writefds) && cli->fd != fd) {
            if (send(cli->fd, msg, strlen(msg), 0) < 0) {
                // Peer disconnected mid-loop — clean up and continue instead
                // of crashing the server. The stale fd will be removed here.
                FD_CLR(cli->fd, &s->active_fds);
                removeClient(s, cli->fd);
            }
        }
        cli = next;
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

//Removes client from linked list and cleans up its file descriptor
void deregisterClient(t_server *s, int fd, int cli_id) {
    (void)cli_id;
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
    struct timeval tv = {1, 0};
    int ret = select(s->max_fd + 1, &s->readfds, &s->writefds, NULL, &tv);
    if (ret < 0) {
        if (errno == EINTR) return; // signal interrupted select — retry cleanly
        fatalError(s);
    }
    if (ret == 0) return; // timeout — nothing ready
    int fd = 0;
    while (fd <= s->max_fd)
    {
        if (FD_ISSET(fd, &s->readfds))
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

    // Allow immediate rebind after crash — prevents EADDRINUSE during TIME_WAIT
    int opt = 1;
    setsockopt(s->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

// ---- worker-thread infrastructure for outgoing TCP calls ----
// Keeps server_loop's select() thread non-blocking (Fix #3).

typedef struct {
    t_server *s;
    int       port;
    char      ip[64];
} notify_task_t;

typedef struct {
    t_server *s;
    int       port;
    char      ip[64];
} join_task_t;

typedef struct {
    t_server *s;
    int       port;
    int       startingNode;
    int       local_id;
    char      succ_ip[MAX_IP_LENGTH];
    char      pred_ip[MAX_IP_LENGTH];
} check_ring_task_t;

static void *notify_worker(void *arg) {
    notify_task_t *task = (notify_task_t *)arg;
    remote_notify(task->s, task->port, task->ip);
    free(task);
    return NULL;
}

static void *join_worker(void *arg) {
    join_task_t *task = (join_task_t *)arg;
    remote_join(task->ip, task->port, task->s);
    free(task);
    return NULL;
}

static void *check_ring_worker(void *arg) {
    check_ring_task_t *task = (check_ring_task_t *)arg;
    t_server *s    = task->s;
    int       port = task->port;

    Node *tempSucc = remote_get_node(s, port, task->succ_ip);
    Node *tempPred = remote_get_node(s, port, task->pred_ip);

    if (tempSucc == NULL || tempPred == NULL) {
        log_error("[ERROR] CHECK_RING: could not fetch neighbour info at node %d", task->local_id);
        if (tempSucc) freeNode(tempSucc);
        if (tempPred) freeNode(tempPred);
        free(task);
        return NULL;
    }

    if (tempSucc->predecessor == NULL || tempSucc->predecessor->id != task->local_id)
        log_error("[ERROR] successor->predecessor mismatch at node %d", task->local_id);

    if (tempPred->successor == NULL || tempPred->successor->id != task->local_id)
        log_error("[ERROR] predecessor->successor mismatch at node %d", task->local_id);

    int sock = init_socket(task->succ_ip, port);
    if (sock >= 0) {
        char request[64];
        snprintf(request, sizeof(request), "CHECK_RING %d\n", task->startingNode);
        if (send(sock, request, strlen(request), 0) < 0)
            perror("send CHECK_RING forward");
        close(sock);
    } else {
        log_error("CHECK_RING: could not connect to successor at %s", task->succ_ip);
    }

    freeNode(tempSucc);
    freeNode(tempPred);
    free(task);
    return NULL;
}

static void spawn_detached(void *(*fn)(void *), void *arg) {
    pthread_t      tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, fn, arg) != 0) {
        log_error("spawn_detached: pthread_create failed");
        free(arg);
    }
    pthread_attr_destroy(&attr);
}

// --------------------------------------------------------------

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
            send(cli->fd, "OK\n", 3, 0);
            join_task_t *task = malloc(sizeof(join_task_t));
            if (task) {
                task->s    = s;
                task->port = defaultPort;
                strncpy(task->ip, ip, sizeof(task->ip) - 1);
                task->ip[sizeof(task->ip) - 1] = '\0';
                spawn_detached(join_worker, task);
            }
        } else {
            send(cli->fd, "ERROR Invalid JOIN\n", 19, 0);
        }
    }

    else if (strcmp(cmd, "STABILIZE") == 0) {
        char existingIp[64];
        if (sscanf(msg, "STABILIZE %63s", existingIp) == 1) {
            send(cli->fd, "OK\n", 3, 0);
            notify_task_t *task = malloc(sizeof(notify_task_t));
            if (task) {
                task->s    = s;
                task->port = defaultPort;
                strncpy(task->ip, existingIp, sizeof(task->ip) - 1);
                task->ip[sizeof(task->ip) - 1] = '\0';
                spawn_detached(notify_worker, task);
            }
        } else {
            send(cli->fd, "Invalid arguments given for stabilize\n", 38, 0);
        }
    }

    else if (strcmp(cmd, "FINGER_TABLE_FALLBACK") == 0) {
        send(cli->fd, "Node succesfully reached\n", 28, 0);
    }

    // --------------------
    // FIND_SUCCESSOR
    // --------------------
    else if (strcmp(cmd, "FIND_SUCCESSOR") == 0) {
        int id;

        if (sscanf(msg, "FIND_SUCCESSOR %d", &id) == 1) {
            pthread_mutex_lock(&s->lock);
            Node* succ = find_successor(s->localNode, id);
            if (succ == NULL) {
                pthread_mutex_unlock(&s->lock);
                send(cli->fd, "ERROR No successor found\n", 25, 0);
                return;
            }
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
            pthread_mutex_lock(&s->lock);

            if (s->localNode->id == startingNode) {
                log_info("Completed ring check at node %d with IP: %s", s->localNode->id, s->localNode->Ip);
                pthread_mutex_unlock(&s->lock);
                return;
            }

            if (s->localNode->predecessor == NULL || s->localNode->successor == NULL) {
                log_warn("CHECK_RING: predecessor or successor is NULL at node %d, ring not fully formed", s->localNode->id);
                pthread_mutex_unlock(&s->lock);
                return;
            }

            // Snapshot all fields we need before releasing the lock.
            check_ring_task_t *task = malloc(sizeof(check_ring_task_t));
            if (!task) {
                pthread_mutex_unlock(&s->lock);
                return;
            }
            task->s            = s;
            task->port         = s->port;
            task->startingNode = startingNode;
            task->local_id     = s->localNode->id;
            strncpy(task->succ_ip, s->localNode->successor->Ip, sizeof(task->succ_ip) - 1);
            task->succ_ip[sizeof(task->succ_ip) - 1] = '\0';
            strncpy(task->pred_ip, s->localNode->predecessor->Ip, sizeof(task->pred_ip) - 1);
            task->pred_ip[sizeof(task->pred_ip) - 1] = '\0';
            pthread_mutex_unlock(&s->lock);

            spawn_detached(check_ring_worker, task);
        } else {
            pthread_mutex_lock(&s->lock);
            log_warn("Invalid CHECK_RING command at node %d", s->localNode->id);
            pthread_mutex_unlock(&s->lock);
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
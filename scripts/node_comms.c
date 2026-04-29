#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "../src/DHASH.h"
#include "../src/logger.h"
#include "../src/tcp_server.h"
#include "../src/tcp_protocol.h"

/*
TCP Server for Chord Node Communications
- Listens on port DEFAULT_TCP_PORT (9000)
- Accepts requests from remote nodes
- Executes Chord algorithm functions locally
- Sends responses back to clients

Previous version used SSH + CLI. This version uses TCP sockets.

Compilation: gcc -pthread node_comms.c ../src/DHASH.c ../src/logger.c ../src/node.c ../src/tcp_server.c ../src/tcp_client.c ../src/tcp_protocol.c -Wall -o node_comms -lm
*/

/* Global state */
static Node* local_node = NULL;
static TCPServer* server = NULL;

/* Signal handler for graceful shutdown */
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[INFO] Shutting down TCP server...\n");
        if (server) {
            tcp_server_stop(server);
        }
    }
}

/* Process a request and return response
 * Protocol: COMMAND|ARG1|ARG2|...\n
 */
char* process_request(const char* request) {
    if (!request || !local_node) {
        char* response = malloc(50);
        snprintf(response, 50, "%s|Local node not initialized\n", RESP_ERROR);
        return response;
    }

    /* Parse request */
    char buffer[512];
    strncpy(buffer, request, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    char* saveptr;
    char* command = strtok_r(buffer, "|", &saveptr);
    
    if (!command) {
        char* response = malloc(50);
        snprintf(response, 50, "%s|No command provided\n", RESP_ERROR);
        return response;
    }

    /* find_successor <target_id> */
    if (strcmp(command, CMD_FIND_SUCCESSOR) == 0) {
        char* arg = strtok_r(NULL, "|", &saveptr);
        if (!arg) {
            char* response = malloc(50);
            snprintf(response, 50, "%s|Missing target_id\n", RESP_ERROR);
            return response;
        }

        int targetId = atoi(arg);
        Node* result = find_successor(local_node, targetId);

        char* response = malloc(TCP_BUFFER_SIZE);
        if (result != NULL) {
            snprintf(response, TCP_BUFFER_SIZE, "%s|%d|%s\n", RESP_OK, result->id, result->Ip);
        } else {
            snprintf(response, TCP_BUFFER_SIZE, "%s|Failed to find successor for ID %d\n", RESP_ERROR, targetId);
        }
        return response;
    }

    /* get_successor */
    if (strcmp(command, CMD_GET_SUCCESSOR) == 0) {
        char* response = malloc(TCP_BUFFER_SIZE);
        if (local_node->successor != NULL) {
            snprintf(response, TCP_BUFFER_SIZE, "%s|%d|%s\n", RESP_OK, 
                     local_node->successor->id, local_node->successor->Ip);
        } else {
            snprintf(response, TCP_BUFFER_SIZE, "%s|No successor\n", RESP_ERROR);
        }
        return response;
    }

    /* closest_preceding_finger <target_id> */
    if (strcmp(command, CMD_CLOSEST_PRECEDING_FINGER) == 0) {
        char* arg = strtok_r(NULL, "|", &saveptr);
        if (!arg) {
            char* response = malloc(50);
            snprintf(response, 50, "%s|Missing target_id\n", RESP_ERROR);
            return response;
        }

        int targetId = atoi(arg);
        Node* result = closest_preceding_finger(local_node, targetId);

        char* response = malloc(TCP_BUFFER_SIZE);
        if (result != NULL) {
            snprintf(response, TCP_BUFFER_SIZE, "%s|%d|%s\n", RESP_OK, result->id, result->Ip);
        } else {
            snprintf(response, TCP_BUFFER_SIZE, "%s|No result\n", RESP_ERROR);
        }
        return response;
    }

    /* notify <predecessor_id> <predecessor_ip> */
    if (strcmp(command, CMD_NOTIFY) == 0) {
        char* pred_id_arg = strtok_r(NULL, "|", &saveptr);
        char* pred_ip_arg = strtok_r(NULL, "|", &saveptr);

        if (!pred_id_arg || !pred_ip_arg) {
            char* response = malloc(50);
            snprintf(response, 50, "%s|Missing notify arguments\n", RESP_ERROR);
            return response;
        }

        int pred_id = atoi(pred_id_arg);
        const char* pred_ip = pred_ip_arg;

        if (local_node->predecessor == NULL || 
            in_open_interval(pred_id, local_node->predecessor->id, local_node->id)) {
            
            if (local_node->predecessor != NULL) {
                freeNode(local_node->predecessor);
            }
            local_node->predecessor = createNode(pred_id, pred_ip, "");
            saveNodeToFile(local_node, "../nodeInfo/Node");
            
            log_info("[INFO] Predecessor updated to Node %d (IP: %s)\n", pred_id, pred_ip);
        }

        char* response = malloc(50);
        snprintf(response, 50, "%s|Predecessor updated\n", RESP_OK);
        return response;
    }

    /* print_finger_table */
    if (strcmp(command, CMD_PRINT_FINGER_TABLE) == 0) {
        char* response = malloc(TCP_BUFFER_SIZE);
        snprintf(response, TCP_BUFFER_SIZE, "%s|Finger table printed to console\n", RESP_OK);
        printFingerTable(local_node);
        return response;
    }

    /* save_finger_table */
    if (strcmp(command, CMD_SAVE_FINGER_TABLE) == 0) {
        saveFingerTableToFile(local_node, "../nodeInfo/FingerTable");
        char* response = malloc(50);
        snprintf(response, 50, "%s|Saved\n", RESP_OK);
        return response;
    }

    /* load_finger_table */
    if (strcmp(command, CMD_LOAD_FINGER_TABLE) == 0) {
        loadFingerTableFromFile(local_node, "../nodeInfo/FingerTable");
        char* response = malloc(50);
        snprintf(response, 50, "%s|Loaded\n", RESP_OK);
        return response;
    }

    /* check_ring */
    if (strcmp(command, CMD_CHECK_RING) == 0) {
        char* response = malloc(TCP_BUFFER_SIZE);
        if (local_node->successor && local_node->predecessor) {
            if (local_node->successor->predecessor != local_node) {
                snprintf(response, TCP_BUFFER_SIZE, 
                         "%s|successor->predecessor mismatch at node %d\n", RESP_ERROR, local_node->id);
            } else if (local_node->predecessor->successor != local_node) {
                snprintf(response, TCP_BUFFER_SIZE, 
                         "%s|predecessor->successor mismatch at node %d\n", RESP_ERROR, local_node->id);
            } else {
                snprintf(response, TCP_BUFFER_SIZE, "%s|%d|%s\n", RESP_OK, 
                         local_node->id, local_node->Ip);
            }
        } else {
            snprintf(response, TCP_BUFFER_SIZE, "%s|Node missing successor or predecessor\n", RESP_ERROR);
        }
        return response;
    }

    /* get_finger_entry <index> */
    if (strcmp(command, CMD_GET_FINGER_ENTRY) == 0) {
        char* arg = strtok_r(NULL, "|", &saveptr);
        if (!arg) {
            char* response = malloc(50);
            snprintf(response, 50, "%s|Missing index\n", RESP_ERROR);
            return response;
        }

        int index = atoi(arg);
        char* response = malloc(TCP_BUFFER_SIZE);

        if (index < 0 || index >= NODE_ID_LENGTH) {
            snprintf(response, TCP_BUFFER_SIZE, "%s|Index out of range\n", RESP_ERROR);
            return response;
        }

        FingerTableEntry* entry = &local_node->fingerTable[index];
        int succ_id = (entry->successor != NULL) ? entry->successor->id : -1;
        const char* succ_ip = (entry->Ip[0] != '\0') ? entry->Ip : "NONE";

        snprintf(response, TCP_BUFFER_SIZE, "%s|%d|%d|%d|%d|%d|%s\n", RESP_OK,
                 index, entry->start, entry->lowerIntervalLimit, 
                 entry->upperIntervalLimit, succ_id, succ_ip);
        return response;
    }

    /* Unknown command */
    char* response = malloc(100);
    snprintf(response, 100, "%s|Unknown command: %s\n", RESP_ERROR, command);
    return response;
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_TCP_PORT;

    /* Allow port override via command line */
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    /* Load local node configuration */
    local_node = loadNodeFromFile("../nodeInfo/Node");
    if (local_node == NULL) {
        fprintf(stderr, "[ERROR] Failed to load local node from file\n");
        return 1;
    }

    loadFingerTableFromFile(local_node, "../nodeInfo/FingerTable");

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize TCP server */
    server = tcp_server_init(port);
    if (!server || server->server_socket < 0) {
        fprintf(stderr, "[ERROR] Failed to initialize TCP server on port %d\n", port);
        if (local_node) freeNode(local_node);
        return 1;
    }

    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Chord Node %d TCP Server Started                         ║\n", local_node->id);
    printf("║  Listening on port %d                                      ║\n", port);
    printf("║  IP: %s                                                    ║\n", local_node->Ip);
    printf("╚════════════════════════════════════════════════════════════╝\n");

    /* Start server (blocking) */
    tcp_server_run(server, process_request);

    /* Cleanup */
    tcp_server_free(server);
    if (local_node) freeNode(local_node);

    printf("[INFO] TCP server shut down\n");
    return 0;
}

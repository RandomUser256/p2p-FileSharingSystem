#include "DHASH.h"
#include "logger.h"
#include "tcp_client.h"
#include "tcp_protocol.h"

/*
Pending changes:
    - Implement correct hashing of file identifiers
    - Delete 'destinationNodeId' from insert() and replace with hashing of the file identifier to obtain the destination node ID (within lookup() )
*/

// DISCLAIMER: In this function change arguments to incorporate appropriate hashing of files
Node* lookup(Node* node, int identifier) {
    //Hash the identifier to get the corresponding node ID
    //int nodeId = hash(identifier);

    //Find the node responsible for the given identifier
    Node* responsibleNode = find_successor(node, /*nodeId*/ identifier);

    if (responsibleNode != NULL) {
        return responsibleNode;
    } else {
        log_error("No responsible node found for identifier. Value of errno: %d\n", errno);
        return NULL;
    }
}

char* generateDestinationFilePath(Node* destinationNode, char* identifier) {
    static char destinationFilePath[MAX_FILE_PATH_LENGTH];
    int written = snprintf(destinationFilePath, MAX_FILE_PATH_LENGTH,
                           "%s/%s",
                           destinationNode->fileContentPath,
                           identifier);
    if (written < 0 || written >= MAX_FILE_PATH_LENGTH) {
        log_error("Error: Destination file path exceeds maximum length.\n");
        return NULL;
    }
    return destinationFilePath;
}

//Insert a file from the host node to the corresponding node for the given file identifier
void insert(Node* hostNode, char* identifier, char* sourceFilePath, int destinationNodeId) {
    //Hash the identifier and find the node responsible for the given identifier
    //Node* destinationNode1 = lookup(hostNode, identifier);

    Node* destinationNode = lookup(hostNode, destinationNodeId);

    if(destinationNode == NULL) {
        log_error("Error: Destination node not found\n");
        return;
    }

    // Build destination path safely
    char destinationFilePath[MAX_FILE_PATH_LENGTH];

    strncpy(destinationFilePath, destinationNode->fileContentPath, MAX_FILE_PATH_LENGTH - 1);
    destinationFilePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

    strncat(destinationFilePath, "/", MAX_FILE_PATH_LENGTH - strlen(destinationFilePath) - 1);
    strncat(destinationFilePath, identifier, MAX_FILE_PATH_LENGTH - strlen(destinationFilePath) - 1);

    log_info("Copying file to node %d (%s)\n",
           destinationNode->id,
           destinationNode->Ip); 
    //SCP COMMAND
    char command[512];

    sprintf(command, "scp %s %s:%s", sourceFilePath, destinationNode->Ip, destinationFilePath);

    log_info("Executing: %s\n", command);

    int result = system(command);

    if(result != 0) {
        log_error("Error: SCP failed\n");
    } else {
        log_info("File transferred successfully\n");
    }
}

//SSH into another machine to consult their node information, returning a Node object 
Node* remote_find_successor(const char* ip, int targetId) {
    if (!ip) {
        log_error("remote_find_successor: Invalid IP\n");
        return NULL;
    }

    /* Build request: find_successor|<targetId> */
    char target_id_str[16];
    snprintf(target_id_str, sizeof(target_id_str), "%d", targetId);
    
    const char* args[] = {target_id_str};
    char* request = build_request(CMD_FIND_SUCCESSOR, args, 1);
    
    if (!request) {
        return NULL;
    }

    /* Send request via TCP */
    char* response = tcp_request_response(ip, DEFAULT_TCP_PORT, request);
    free(request);

    if (!response) {
        log_error("remote_find_successor: No response from %s\n", ip);
        return NULL;
    }

    /* Parse response: OK|<id>|<ip> */
    char response_copy[TCP_BUFFER_SIZE];
    strncpy(response_copy, response, sizeof(response_copy) - 1);
    response_copy[sizeof(response_copy) - 1] = '\0';
    free(response);

    /* Remove trailing newline */
    size_t len = strlen(response_copy);
    if (len > 0 && response_copy[len - 1] == '\n') {
        response_copy[len - 1] = '\0';
    }

    char* saveptr;
    char* status = strtok_r(response_copy, "|", &saveptr);
    
    if (!status || strcmp(status, RESP_OK) != 0) {
        log_error("remote_find_successor: Error response from %s\n", ip);
        return NULL;
    }

    char* id_str = strtok_r(NULL, "|", &saveptr);
    char* resp_ip = strtok_r(NULL, "|", &saveptr);

    if (!id_str || !resp_ip) {
        log_error("remote_find_successor: Invalid response format\n");
        return NULL;
    }

    int id = atoi(id_str);
    return createNode(id, resp_ip, "");
}

Node* remote_get_successor(const char* ip) {
    if (!ip) {
        log_error("remote_get_successor: Invalid IP\n");
        return NULL;
    }

    /* Build request: get_successor */
    char* request = build_request(CMD_GET_SUCCESSOR, NULL, 0);
    
    if (!request) {
        return NULL;
    }

    /* Send request via TCP */
    char* response = tcp_request_response(ip, DEFAULT_TCP_PORT, request);
    free(request);

    if (!response) {
        log_error("remote_get_successor: No response from %s\n", ip);
        return NULL;
    }

    /* Parse response: OK|<id>|<ip> */
    char response_copy[TCP_BUFFER_SIZE];
    strncpy(response_copy, response, sizeof(response_copy) - 1);
    response_copy[sizeof(response_copy) - 1] = '\0';
    free(response);

    /* Remove trailing newline */
    size_t len = strlen(response_copy);
    if (len > 0 && response_copy[len - 1] == '\n') {
        response_copy[len - 1] = '\0';
    }

    char* saveptr;
    char* status = strtok_r(response_copy, "|", &saveptr);
    
    if (!status || strcmp(status, RESP_OK) != 0) {
        log_error("remote_get_successor: Error response from %s\n", ip);
        return NULL;
    }

    char* id_str = strtok_r(NULL, "|", &saveptr);
    char* resp_ip = strtok_r(NULL, "|", &saveptr);

    if (!id_str || !resp_ip) {
        log_error("remote_get_successor: Invalid response format\n");
        return NULL;
    }

    int id = atoi(id_str);
    return createNode(id, resp_ip, "shared/files");
}

Node* remote_closest_preceding_finger(const char* ip, int targetId) {
    if (!ip) {
        log_error("remote_closest_preceding_finger: Invalid IP\n");
        return NULL;
    }

    /* Build request: closest_preceding_finger|<targetId> */
    char target_id_str[16];
    snprintf(target_id_str, sizeof(target_id_str), "%d", targetId);
    
    const char* args[] = {target_id_str};
    char* request = build_request(CMD_CLOSEST_PRECEDING_FINGER, args, 1);
    
    if (!request) {
        return NULL;
    }

    /* Send request via TCP */
    char* response = tcp_request_response(ip, DEFAULT_TCP_PORT, request);
    free(request);

    if (!response) {
        log_error("remote_closest_preceding_finger: No response from %s\n", ip);
        return NULL;
    }

    /* Parse response: OK|<id>|<ip> */
    char response_copy[TCP_BUFFER_SIZE];
    strncpy(response_copy, response, sizeof(response_copy) - 1);
    response_copy[sizeof(response_copy) - 1] = '\0';
    free(response);

    /* Remove trailing newline */
    size_t len = strlen(response_copy);
    if (len > 0 && response_copy[len - 1] == '\n') {
        response_copy[len - 1] = '\0';
    }

    char* saveptr;
    char* status = strtok_r(response_copy, "|", &saveptr);
    
    if (!status || strcmp(status, RESP_OK) != 0) {
        log_error("remote_closest_preceding_finger: Error response from %s\n", ip);
        return NULL;
    }

    char* id_str = strtok_r(NULL, "|", &saveptr);
    char* resp_ip = strtok_r(NULL, "|", &saveptr);

    if (!id_str || !resp_ip) {
        log_error("remote_closest_preceding_finger: Invalid response format\n");
        return NULL;
    }

    int id = atoi(id_str);
    return createNode(id, resp_ip, "shared/files");
}
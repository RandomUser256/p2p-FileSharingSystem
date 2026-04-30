#include "DHASH.h"
#include "logger.h"

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

/*
//SSH into another machine to consult their node information, returning a Node object 
Node* remote_find_successor(const char* ip, int targetId) {
    char command[512];

    //Executes a system call, uses SSH to 
    snprintf(command, sizeof(command),
        "ssh %s \"cd /home/mmagallanes && ./scripts/node_comms find_successor %d\" 2>/dev/null",
        ip,
        targetId
    );

    FILE* fp = popen(command, "r");

    if (!fp) {
        log_error("SSH failed\n");
        return NULL;
    }

    char line[256];
    line[0] = '\0';
    char tmp[256];

    //Copies file to temporary char arrays
    while (fgets(tmp, sizeof(tmp), fp)) {
        if (tmp[0] != '\n' && tmp[0] != '\0')
            memcpy(line, tmp, sizeof(line));
    }

    int id;
    char remoteIp[16];

    //If it returns a value different to 2, it did not succesfully assign a value to the 2 variables
    if (sscanf(line, "%d %15s", &id, remoteIp) != 2) {
        log_error("Invalid response: %s\n", line);
        pclose(fp);
        return NULL;
    }

    pclose(fp);

    return createNode(id, remoteIp, "");
}
*/

//NOT FINISHESED, NOT IN USE CURRENTLY
Node* remote_find_successor(const char* ip, int port, int id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return NULL;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return NULL;
    }

    // 🔴 Send request
    char request[64];
    snprintf(request, sizeof(request), "FIND_SUCCESSOR %d\n", id);

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return NULL;
    }

    // 🔴 Receive response
    char response[128];

    //Waits for response from server
    int n = recv(sock, response, sizeof(response) - 1, 0);
    if (n <= 0) {
        perror("recv");
        close(sock);
        return NULL;
    }

    response[n] = '\0';

    // 🔴 Parse response: "NODE <id> <ip>"
    int node_id;
    char node_ip[64];

    if (sscanf(response, "NODE %d %63s", &node_id, node_ip) == 2) {
        close(sock);
        return createNode(node_id, node_ip, "");
    }

    // Handle error response
    if (strncmp(response, "ERROR", 5) == 0) {
        fprintf(stderr, "RPC error: %s\n", response);
    }

    close(sock);
    return NULL;
}

Node* remote_get_successor(const char* ip) {
    char command[256];

    //SSH command to external machine in network
    snprintf(command, sizeof(command),
        "ssh %s \"cd /home/mmagallanes && ./scripts/node_comms get_successor\" 2>/dev/null",
        ip
    );

    FILE* fp = popen(command, "r");
    if (!fp) return NULL;

    char line[128];
    line[0] = '\0';
    char tmp[128];

    // Keep reading until EOF, always preserving the last non-empty line
    while (fgets(tmp, sizeof(tmp), fp)) {
        if (tmp[0] != '\n' && tmp[0] != '\0')
            memcpy(line, tmp, sizeof(line));
    }

    int id;
    char remoteIp[16];

    if (sscanf(line, "%d %15s", &id, remoteIp) != 2) {
        pclose(fp);
        return NULL;
    }

    pclose(fp);
    return createNode(id, remoteIp, "shared/files");
}

 /*
Node* remote_closest_preceding_finger(const char* ip, int targetId) {
    char command[256];

    //SSH command to other machine in network
    snprintf(command, sizeof(command),
        "ssh %s \"cd /home/mmagallanes && ./scripts/node_comms closest_preceding_finger %d\" 2>/dev/null",
        ip,
        targetId
    );

    FILE* fp = popen(command, "r");
    if (!fp) return NULL;

    char line[128];
    line[0] = '\0';
    char tmp[128];

    // Keep reading until EOF, always preserving the last non-empty line
    while (fgets(tmp, sizeof(tmp), fp)) {
        if (tmp[0] != '\n' && tmp[0] != '\0')
            memcpy(line, tmp, sizeof(line));
    }

    int id;
    char remoteIp[16];

    //If it returns a value different to 2, it did not succesfully assign a value to the 2 variables
    if (sscanf(line, "%d %15s", &id, remoteIp) != 2) {
        pclose(fp);
        return NULL;
    }

    pclose(fp);
    return createNode(id, remoteIp, "shared/files");
}*/

Node* remote_find_predecessor()

Node* remote_find_successor(const char* ip, int port, int id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return NULL;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return NULL;
    }

    // 🔴 Send request
    char request[64];
    snprintf(request, sizeof(request), "CLOSEST_PRECEDING_FINGER %d\n", id);

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return NULL;
    }

     // 🔴 Receive response
    char response[128];

    //Waits for response from server
    int n = recv(sock, response, sizeof(response) - 1, 0);
    if (n <= 0) {
        perror("recv");
        close(sock);
        return NULL;
    }

    response[n] = '\0';

    // 🔴 Parse response: "NODE <id> <ip>"
    int node_id;
    char node_ip[64];

    if (sscanf(response, "NODE %d %63s", &node_id, node_ip) == 2) {
        close(sock);
        return createNode(node_id, node_ip, "");
    }

    // Handle error response
    if (strncmp(response, "ERROR", 5) == 0) {
        fprintf(stderr, "RPC error: %s\n", response);
    }

    close(sock);
    return NULL;
}
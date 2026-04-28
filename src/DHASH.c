#include "DHASH.h"
#include "logger.h"

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
}
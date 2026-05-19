#include <stdio.h>
#include <stdbool.h>


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <fcntl.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <errno.h>

#include "node.h"
#include "logger.h"
#include "tcpServer.h"
//#include "DHASH.h"

/*
TODO:
*/

/*
ERRORS

*/

//Helper functions for evaluating the position of a value within a number interval

int in_open_interval(int id, int start, int end) {
    if (start < end)
        return id > start && id < end;
    else
        return id > start || id < end;   // wraparound case
}

int half_left_open_interval(int id, int start, int end) {
    if (start < end)
        return id > start && id <= end;
    else
        return id > start || id <= end; // wraparound
}

int half_right_open_interval(int id, int start, int end) {
    if (start < end)
        return id >= start && id < end;
    else
        return id >= start || id < end; // wraparound
}

//Initializes a single FingerTableEntry for a table from the parent_node
FingerTableEntry* createFingerTableEntry(int entryNumber, Node* parent_node, Node* successor) {
    FingerTableEntry* entry = malloc(sizeof(FingerTableEntry));
    if (!entry) {
        fprintf(stderr, "Error allocating memory for FingerTableEntry\n");
        return NULL;
    }
    // Bit shift notation used to substitute exponent function
    entry->start = (parent_node->id + (1U << (entryNumber - 1))) % MAX_NUMBER_NODES; // Calculate the start of the interval
    //entry.parent_node = parent_node;
    entry->lowerIntervalLimit = entry->start;
    entry->upperIntervalLimit = (parent_node->id + (1U << entryNumber)) % MAX_NUMBER_NODES;

    //Add find succesor function
    entry->successor = successor;


    if (successor != NULL) {
        //Check if the successor's IP fits within the defined maximum length
        strncpy(entry->Ip, successor->Ip, MAX_IP_LENGTH - 1);
        entry->Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination

    } else {
        entry->Ip[0] = '\0'; // No successor, set IP to empty string
    }
    
    return entry;
}

//Forward declarations
Node* createNode(int id, const char* ip, const char* fileContentPath);
void saveFingerTableToFile(Node* node, const char* filepath);
void saveNodeToFile(Node* node, const char* filepath);


//Reads local Node text file to construct a Node object in the main proecss
Node* loadNodeFromFile(const char* filepath) {
    FILE* file = fopen(filepath, "r");

    int hasData = 0;

    if (!file) {
        printf("Error opening Node file\n");
        //fprintf(stderr, "Error opening Node file\n");
        return NULL;
    }

    //Variables for different node values
    int  id    = 0;
    char ip[MAX_IP_LENGTH]          = {0};
    char path[MAX_FILE_PATH_LENGTH] = {0};
    int  succId = -1; char succIp[MAX_IP_LENGTH] = {0};
    int  predId = -1; char predIp[MAX_IP_LENGTH] = {0};

    char line[512];

    //Scans text file structure and assigns values to variables
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (sscanf(line, "id=%d",               &id)             == 1)  {
            hasData = 1;
            continue;
        }
        if (sscanf(line, "ip=%15s",              ip)              == 1) {continue;}
        if (sscanf(line, "fileContentPath=%255s", path)           == 1) {continue;}
        if (sscanf(line, "successor=%d %15s",   &succId, succIp)  == 2) {continue;}
        if (sscanf(line, "predecessor=%d %15s", &predId, predIp)  == 2) {continue;}
    }

    fclose(file);

    //Checks that the file was not empty
    if (!hasData || ip[0] == '\0') {
        printf("No data read from node file\n");
        return NULL;
    }

    Node* node = createNode(id, ip, path);

    // createNode() defaults both to self-pointer; clear them so the
    // restore below reflects exactly what the file recorded.
    node->successor   = NULL;
    node->predecessor = NULL;

    if (succId != -1)
        node->successor   = createNode(succId, succIp, "shared/files");
    else
        node->successor   = node; // no successor on disk → treat as single-node ring

    if (predId != -1)
        node->predecessor = createNode(predId, predIp, "shared/files");
    // predId == -1 means NULL predecessor (e.g. just after join) — leave as NULL

    //Prints success of loading process
    fprintf(stderr, "Loaded node: ID=%d IP=%s PATH=%s succ=%d pred=%d\n",
            id, ip, path, succId, predId);

    return node;
}

//Saves the current state of the Node object to the Node text file
void saveNodeToFile(Node* node, const char* filepath) {
    FILE* file = fopen(filepath, "w");

    if (!file) {
        printf("Error saving node\n");
        return;
    }

    fprintf(file, "id=%d\n", node->id);
    fprintf(file, "ip=%s\n", node->Ip);
    fprintf(file, "fileContentPath=%s\n", node->fileContentPath);

    if (node->successor && node->successor != node)
        fprintf(file, "successor=%d %s\n", node->successor->id, node->successor->Ip);
    else if (node->successor == node)
        fprintf(file, "successor=%d %s\n", node->id, node->Ip);
    else
        fprintf(file, "successor=-1 NONE\n");

    if (node->predecessor && node->predecessor != node)
        fprintf(file, "predecessor=%d %s\n", node->predecessor->id, node->predecessor->Ip);
    else if (node->predecessor == node)
        fprintf(file, "predecessor=%d %s\n", node->id, node->Ip);
    else
        fprintf(file, "predecessor=-1 NONE\n");

    fclose(file);
}

//Creates a Node object from the given information
Node* createNode(int id, const char* ip, const char* fileContentPath) {
    Node* newNode = malloc(sizeof(Node));

    newNode->id = id;
    strncpy(newNode->Ip, ip, MAX_IP_LENGTH - 1);
    newNode->Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination
    strncpy(newNode->fileContentPath, fileContentPath, MAX_FILE_PATH_LENGTH - 1);
    newNode->fileContentPath[MAX_FILE_PATH_LENGTH - 1] = '\0'; // Ensure null-termination
    newNode->successor = newNode;
    newNode->predecessor = newNode;

    // Initialize finger table
    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        newNode->fingerTable[i].start =
            (id + (1U << i)) % MAX_NUMBER_NODES;

        newNode->fingerTable[i].lowerIntervalLimit = newNode->fingerTable[i].start;

        if (i < NODE_ID_LENGTH - 1) {
            newNode->fingerTable[i].upperIntervalLimit = (id + (1U << (i + 1))) % MAX_NUMBER_NODES;
        } else {
            newNode->fingerTable[i].upperIntervalLimit = newNode->fingerTable[0].start; // Wrap around for the last entry
        }

        //newNode->fingerTable[i].upperIntervalLimit = newNode->fingerTable[i+1].start;

        newNode->fingerTable[i].successor = newNode;

        newNode->fingerTable[i].Ip[0] = '\0';
    }
    return newNode;
}

//Obtains closest active node that precedes the given node
Node* closest_preceding_finger(Node* node, int targetId) {
    if (!node) {
        return NULL;
    }

    for (size_t i = NODE_ID_LENGTH; i > 0; i--)
    {
        //if the successor of the finger table entry is valid and between current node's id and the target id, return that successor
        Node* finger = node->fingerTable[i-1].successor;

        if (finger != NULL &&
            finger != node &&
            in_open_interval(finger->id, node->id, targetId))
        {
            return finger;
        }
    }
    return node; // If no successor is found in the finger table, return the current node
}

// Creates a shallow copy of a Node for thread-safe access
Node* copyNode(Node* node) {
    if (node == NULL) {
        return NULL;
    }
    
    Node* copy = malloc(sizeof(Node));
    if (copy == NULL) {
        return NULL;
    }
    
    copy->id = node->id;
    strncpy(copy->Ip, node->Ip, MAX_IP_LENGTH - 1);
    copy->Ip[MAX_IP_LENGTH - 1] = '\0';
    strncpy(copy->fileContentPath, node->fileContentPath, MAX_FILE_PATH_LENGTH - 1);
    copy->fileContentPath[MAX_FILE_PATH_LENGTH - 1] = '\0';
    
    // Copy successor and predecessor pointers
    copy->successor = node->successor != NULL ? createNode(node->successor->id, node->successor->Ip, node->successor->fileContentPath) : NULL;
    copy->predecessor = node->predecessor != NULL ? createNode(node->predecessor->id, node->predecessor->Ip, node->predecessor->fileContentPath) : NULL;
    
    // Don't copy finger table - not needed for RPC operations
    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        copy->fingerTable[i].start = 0;
        copy->fingerTable[i].lowerIntervalLimit = 0;
        copy->fingerTable[i].upperIntervalLimit = 0;
        copy->fingerTable[i].successor = NULL;
        copy->fingerTable[i].Ip[0] = '\0';
    }
    
    return copy;
}

//Free dynamically allocated node structures
void freeNode(Node* node) {
    if (node != NULL) {
        free(node);
    }
}

//Chord function for finding predecessor of a given node, starting from any other node
Node* find_predecessor(Node* node, int targetId) {
    if (!node) {
        return NULL;
    }

    Node* n = node;

    //Loops until the target id is in the range of given node and its successor
    while (!half_left_open_interval(targetId, n->id, n->successor->id)) {
        Node* next = closest_preceding_finger(n, targetId);

        if (next == n) break;  // prevent infinite loop

        n = next;
    }

    return n;
}

//Chord function for finding successor of a given node, starting from any other node
Node* find_successor(Node* node, int targetId) {
    if (!node) {
        return NULL;
    }

    Node* pred = find_predecessor(node, targetId);
    if (pred != NULL) {
        return pred->successor; // The successor of the predecessor is the successor of the target ID
    }
    return NULL; // If no predecessor is found, return NULL
} 

//Functions for inserting a new node into the network and updating the finger tables of existing nodes
Node* init_finger_table(Node* existingNode, Node* newNode) {
    if (!existingNode || !existingNode->successor) {
        return NULL;
    }

    newNode->fingerTable[0].successor = find_successor(existingNode, newNode->fingerTable[0].start); // Initialize the second entry in the finger table based on the existing node's information

    newNode->predecessor = newNode->fingerTable[0].successor->predecessor; // Set the predecessor of the existing node to the predecessor of its successor

    newNode->fingerTable[0].successor->predecessor = newNode; // Update the predecessor of the existing node's successor to point to the existing node

    //Initialize the finger table of the new node based on an existing node in the network
    for (size_t i = 2; i <= NODE_ID_LENGTH; i++)
    {
        if (in_open_interval(newNode->fingerTable[i].start, newNode->id, newNode->fingerTable[i-1].successor->id)) {
            newNode->fingerTable[i-1] = *createFingerTableEntry(i, newNode, newNode->fingerTable[i-1].successor); // If the start of the interval for the current entry in the existing node's finger table falls within a certain range, copy that entry to the new node's finger table
        }
        else {
            existingNode->fingerTable[i-1] = *createFingerTableEntry(i, newNode, find_successor(newNode, newNode->fingerTable[i-2].start)); // Otherwise, create a new entry in the new node's finger table based on the existing node's information
        }
    }
    return newNode; // Return the initialized node with its finger table set up
}

//Update finger tables when the structure of the chord ring changes
void update_finger_table(Node* existingNode, Node* newNode, int tableEntryNumber) {
    if (!existingNode || !newNode) {
        return;
    }

    //Update the finger table of the existing node to include the new node
    if (in_open_interval(newNode->id, existingNode->id, existingNode->fingerTable[tableEntryNumber-1].successor->id)) {
        existingNode->fingerTable[tableEntryNumber-1].successor = newNode;
        
        Node* p = existingNode->predecessor;

        strncpy(existingNode->fingerTable[tableEntryNumber-1].Ip, newNode->Ip, MAX_IP_LENGTH - 1);

        existingNode->fingerTable[tableEntryNumber-1].Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination

        update_finger_table(p, newNode, tableEntryNumber); //Recursively update the finger tables of the predecessor nodes
    }
}

//Not in use currently
void update_others(Node* currentNode) {
    if (!currentNode) {
        return;
    }

    //Update the finger tables of existing nodes to include the new node
    for (size_t i = 1; i <= NODE_ID_LENGTH; i++)
    {
        int id = (currentNode->id - (1U << (i - 1)) + MAX_NUMBER_NODES) % MAX_NUMBER_NODES;
        //Obtain the ID corresponding to the successor of the node we want
        Node* predecessor = find_predecessor(currentNode, id); // Calculate the ID of the predecessor node for the current finger table entry

        update_finger_table(predecessor, currentNode, i); // Update the finger table of the predecessor node to include the new node
    }
} 

//Second version of Chord algorithm join, supports concurrent node joins
void join(Node* existingNode, Node* newNode) {
    if (!newNode) {
        return;
    }

    if (existingNode == NULL) {
        // Initializing first node when it is the only one in the network
        newNode->successor = newNode;
        newNode->predecessor = newNode;

    } else {
        Node* successor = find_successor(existingNode, newNode->id); // Find the successor of the existing node using the new node as a reference point

        newNode->successor = successor; // Update the successor of the existing node to point to the new node

        newNode->predecessor = successor->predecessor; // Update the predecessor of the existing node to point to the new node

        if (successor->predecessor != NULL) {
            successor->predecessor->successor = newNode;
        }

        successor->predecessor = newNode; // Update the predecessor of the existing node's successor to point to the new node
    }

    saveNodeToFile(existingNode, "nodeInfo/Node"); //Saves changes in local node to disk for persistence
}

//First version of fix_fingers
//Functions for maintaining the integrity of the finger tables and ensuring that they are up to date with the current state of the network
void fix_fingers(Node* node) {
    if (!node) {
        return;
    }

    //Periodically check and update the finger tables to ensure they are accurate
    //This function can be called at regular intervals to maintain the integrity of the finger tables
    //Implementation would involve checking each entry in the finger table and updating it if necessary
    int i = 1 + rand() % NODE_ID_LENGTH; // Randomly select an entry in the finger table to check
    node->fingerTable[i-1].successor = find_successor(node, node->fingerTable[i-1].start); // Update the successor for the selected entry in the finger table

    //Update file FingerTable
    saveFingerTableToFile(node, "nodeInfo/FingerTable");
}
    
//Checks if a given node has become the new predecessor of an existing node, updates the old nodes pointers
void notify(Node* node, Node* potentialPredecessor) {
    if (!node || !potentialPredecessor) {
        return;
    }

    if (node->predecessor == NULL || in_open_interval(potentialPredecessor->id, node->predecessor->id, node->id)) {
        node->predecessor = potentialPredecessor;
        
        // Save the updated node state to disk
        saveNodeToFile(node, "nodeInfo/Node");
        log_info("[INFO] Node %d predecessor updated to Node %d, saved to disk\n", 
               node->id, potentialPredecessor->id);
    }
}

//Verifies correctness of a nodes succesor
void stabilize(Node* node) {
    if (!node || !node->successor) {
        return;
    }

    Node* x = node->successor->predecessor; // Get the predecessor of the current node's successor

    if (x != NULL && in_open_interval(x->id, node->id, node->successor->id)) {
        node->successor = x;
        saveNodeToFile(node, "nodeInfo/Node");
        log_info("[INFO] Node %d successor updated to Node %d, saved to disk\n", 
                  node->id, x->id);
    }

    notify(node->successor, node); // Notify the successor about the current node as a potential predecessor
}

/* --------------------------------
* Troublshooting functions for centralized Chord algorithm implementations
*/ --------------------------------

//Prints information of current node
void nodePrint(Node* node) {
    if (node == NULL) {
        printf("[INFO] Node is NULL.\n");
        return;
    }
    printf("[INFO] Node ID: %d\n", node->id);
    printf("[INFO] Node IP: %s\n", node->Ip);
    if (node->successor != NULL) {
        printf("[INFO] Node Successor ID: %d %s\n", node->successor->id, node->successor->Ip);
    } else {
        printf("[INFO] Node Successor: NULL\n");
    }
    if (node->predecessor != NULL) {
        printf("[INFO] Node Predecessor ID: %d %s\n", node->predecessor->id, node->predecessor->Ip);
    } else {
        printf("[INFO] Node Predecessor: NULL\n");
    }
    if (node->fileContentPath[0] != '\0') {
        printf("[INFO] Node File Content Path: %s\n", node->fileContentPath);
    } else {
        printf("[INFO] Node File Content Path: None\n");
    }
}

void fingerTablePrint(Node* node) {
    for (int i=0; i < NODE_ID_LENGTH; i++) {
        int succ_id = node->fingerTable[i].successor ? node->fingerTable[i].successor->id : -1;
        const char* succ_ip = node->fingerTable[i].successor ? node->fingerTable[i].successor->Ip : "NULL";
        printf("Start: %d, L.interval: %d, U.interval: %d, Successor_node: %d %s\n",
               node->fingerTable[i].start, node->fingerTable[i].lowerIntervalLimit,
               node->fingerTable[i].upperIntervalLimit, succ_id, succ_ip);
    }
}

void printNodeList(Node* head) {
    Node* start = head;
    Node* current = head;

    do {
        nodePrint(current);
        current = current->successor;
    } while (current != start);
}

//Checks integrity of ring structure
void check_ring(Node* start) {
    Node* curr = start;

    do {
        if (curr->successor->predecessor != curr) {
            log_error("[ERROR] successor->predecessor mismatch at node %d\n", curr->id);
        }
        if (curr->predecessor->successor != curr) {
            log_error("[ERROR] predecessor->successor mismatch at node %d\n", curr->id);
        }

        curr = curr->successor;
    } while (curr != start);
}


/*
   FINGER TABLE PERSISTENCE 
   - Save/load finger table to/from disk
*/
//Saves current state of finger table to local FingerTable file
void saveFingerTableToFile(Node* node, const char* filepath) {
    if (!node) {
        printf("Error: node is NULL\n");
        return;
    }

    FILE* file = fopen(filepath, "w");
    if (!file) {
        printf("Error: cannot open file %s for writing\n", filepath);
        return;
    }

    //File header
    fprintf(file, "# Finger Table for Node %d\n", node->id);
    fprintf(file, "# Format: entry=<idx>,start=<start>,lower=<lower>,upper=<upper>,successor_id=<id>,successor_ip=<ip>\n\n");

    //Loops through every line in the file and extracts information from each FingerTableEntry object
    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        FingerTableEntry* entry = &node->fingerTable[i];
        
        int succ_id = (entry->successor != NULL) ? entry->successor->id : -1;
        const char* succ_ip = (entry->successor != NULL && entry->successor->Ip[0] != '\0') ? entry->successor->Ip : "NONE";

        fprintf(file, "entry=%d,start=%d,lower=%d,upper=%d,successor_id=%d,successor_ip=%s\n",
                i,
                entry->start,
                entry->lowerIntervalLimit,
                entry->upperIntervalLimit,
                succ_id,
                succ_ip);
    }

    fclose(file);

    log_info("[INFO] Finger table for node %d saved to %s\n", node->id, filepath);
}

//Loads ingertable from local text file and saves it to the Node object 
void loadFingerTableFromFile(Node* node, const char* filepath) {
    if (!node) {
        log_error("[ERROR] Error: node is NULL\n");
        return;
    }

    FILE* file = fopen(filepath, "r");
    if (!file) {
        log_warn("[WARNING] cannot open file %s for reading, finger table will use defaults\n", filepath);
        return;
    }

    char line[512];
    int entry_idx = 0;

    //Loops through every line in the text file and updates the fingerTable
    while (fgets(line, sizeof(line), file) && entry_idx < NODE_ID_LENGTH) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        int idx, start, lower, upper, succ_id;
        char succ_ip[MAX_IP_LENGTH] = {0};

        int parsed = sscanf(line, 
                           "entry=%d,start=%d,lower=%d,upper=%d,successor_id=%d,successor_ip=%15s",
                           &idx, &start, &lower, &upper, &succ_id, succ_ip);

        if (parsed != 6) {
            log_warn("[WARNING] skipping malformed line: %s\n", line);
            continue;
        }

        // Validate entry index
        if (idx < 0 || idx >= NODE_ID_LENGTH) {
            log_warn("[WARNING] invalid entry index %d, skipping\n", idx);
            continue;
        }

        node->fingerTable[idx].start = start;
        node->fingerTable[idx].lowerIntervalLimit = lower;
        node->fingerTable[idx].upperIntervalLimit = upper;

        // Update successor reference
        //Checks if successor ID and IP are valid before updating the finger table entry
        if (succ_id != -1 && strcmp(succ_ip, "NONE") != 0) {
            // Reuse existing successor if IDs match, otherwise create new node
            if (node->fingerTable[idx].successor == NULL || 
                node->fingerTable[idx].successor->id != succ_id) {
                if (node->fingerTable[idx].successor != NULL && 
                    node->fingerTable[idx].successor != node) {
                    freeNode(node->fingerTable[idx].successor);
                }
                node->fingerTable[idx].successor = createNode(succ_id, succ_ip, "");
            }
            strncpy(node->fingerTable[idx].Ip, succ_ip, MAX_IP_LENGTH - 1);
            node->fingerTable[idx].Ip[MAX_IP_LENGTH - 1] = '\0';
        } else {
            // No valid successor, point to self
            node->fingerTable[idx].successor = node;
            node->fingerTable[idx].Ip[0] = '\0';
        }

        entry_idx++;
    }

    fclose(file);
    log_info("[INFO] Finger table for node %d loaded from %s\n", node->id, filepath);
}

//Prints Node information to console
void printFingerTable(Node* node) {
    if (!node) {
        log_error("[ERROR] Error: node is NULL\n");
        return;
    }

    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║         Finger Table for Node %d (IP: %s)          ║\n", node->id, node->Ip);
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Idx │ Start │ Lower │ Upper │ Successor ID │ Successor IP  ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");

    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        FingerTableEntry* entry = &node->fingerTable[i];
        int succ_id = (entry->successor != NULL) ? entry->successor->id : -1;
        const char* succ_ip = (entry->Ip[0] != '\0') ? entry->Ip : "N/A";

        printf("║ %2d  │  %2d  │  %2d  │  %2d  │      %2d      │ %-13s ║\n",
               i, entry->start, entry->lowerIntervalLimit, 
               entry->upperIntervalLimit, succ_id, succ_ip);
    }

    printf("╚════════════════════════════════════════════════════════════╝\n\n");
}

/* ======================================================================
   Chord functions for remote communication 
   ====================================================================== */

//Initiates socket connection between server and client
int init_socket(const char* ip, int port) {
    //Creates IPv4 (AF_INET) TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_error("socket: %s", strerror(errno));
        return -1;
    }

    // Set non-blocking communication to implement custom timeout
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        log_error("fcntl F_GETFL: %s", strerror(errno));
        close(sock);
        return -1;
    }

    //O_NONBLOCK indicates that if connect() takes to long, return immediately
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("fcntl F_SETFL: %s", strerror(errno));
        close(sock);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        log_error("inet_pton: %s", strerror(errno));
        close(sock);
        return -1;
    }

    //Tries connect(), immediately returns with error due to non-blocking state
    int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (res < 0) {
        if (errno != EINPROGRESS) {
            log_error("connect: %s", strerror(errno));
            close(sock);
            return -1;
        }

        // Wait up to 3 seconds
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);

        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        //Waits for three seconds before timeout
        res = select(sock + 1, NULL, &wfds, NULL, &tv);

        if (res == 0) {
            log_error("connect timeout (3s)");
            close(sock);
            return -1;
        }
        if (res < 0) {
            log_error("select: %s", strerror(errno));
            close(sock);
            return -1;
        }

        // Check if connection actually succeeded
        int so_error;
        socklen_t len = sizeof(so_error);

        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
            log_error("getsockopt: %s", strerror(errno));
            close(sock);
            return -1;
        }

        if (so_error != 0) {
            log_error("connect failed: %s", strerror(so_error));
            close(sock);
            return -1;
        }
    }

    // Restore blocking mode
    if (fcntl(sock, F_SETFL, flags) < 0) {
        log_warn("failed to restore blocking mode");
    }

    // send/recv process timeouts of 3 seconds to avoid indefinite waits
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    return sock;
}

//Allows you to retrieve full node information from an Ip
Node* remote_get_node(t_server* s, int port, const char* ip) {
    // Avoid TCP self-connection deadlock: serve the request locally
    pthread_mutex_lock(&s->lock);
    //If searched node is the same as local node
    if (s->localNode && strcmp(ip, s->localNode->Ip) == 0) {
        Node* copy = copyNode(s->localNode);
        pthread_mutex_unlock(&s->lock);
        return copy;
    }
    pthread_mutex_unlock(&s->lock);

    int sock = init_socket(ip, port);

    if (sock < 0) {
        log_warn("Invalid node accessed with remote_get_node() ip %s", ip);
        return NULL;
    }

    //  Send request for retrieven node data
    char request[64];
    snprintf(request, sizeof(request), "GET_NODE\n");

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return NULL;
    }

    //  Receive response with timeout protection
    char response[256];
    int total = 0;
    int n = 0;

    //Extract response from server
    while (total < sizeof(response) - 1) {
        //Stores received data in 'response' and tracks amount of bytes written to the buffer
        n = recv(sock, response + total, sizeof(response) - total - 1, 0);

        if (n < 0) {
            log_error("recv failed: %s", strerror(errno));
            break;
        }

        //Empty node response, treat as error
        if (n == 0) {
            log_error("recv: connection closed by server");
            break; 
        }

        //Updates starting position for writing to buffer
        total += n;
        response[total] = '\0';

        if (strchr(response, '\n')) break; // end of message
    }

    //If no data was written
    if (total == 0) {
        //perror("recv");
        log_error("Failed to receive response from node at IP %s", ip);
        close(sock);
        return NULL;
    }

    //Null terminator for string
    response[total] = '\0';

    int node_id;
    char node_ip[64];
    int succ_id;
    char succ_ip[64];
    int pred_id;
    char pred_ip[64];

    Node* temp;

    //Parses information from response into temp variables
    if (sscanf(response, "NODE %d %63s %d %63s %d %63s", &node_id, node_ip, &succ_id, succ_ip, &pred_id, pred_ip) == 6) {
        temp = createNode(node_id, node_ip, "shared/files");
        temp->successor = createNode(succ_id, succ_ip, "shared/files");
        temp->predecessor = createNode(pred_id, pred_ip, "shared/files");
    } else {
        fprintf(stderr, "RPC error: %s\n", response);
        close(sock);
        return NULL;
    }

    close(sock);

    return temp;
}

//Distributed implementation of Chord algorithm find_predecessor()
Node* remote_find_predecessor(t_server* s, int port, int targetId) {
    // Get a copy of the local node while holding the lock to avoid race conditions
    pthread_mutex_lock(&s->lock);
    Node* predecessor = copyNode(s->localNode);
    int localNodeId = s->localNode->id;
    char localNodeIp[64];
    strncpy(localNodeIp, s->localNode->Ip, sizeof(localNodeIp) - 1);
    localNodeIp[sizeof(localNodeIp) - 1] = '\0';
    pthread_mutex_unlock(&s->lock);

    //Avoids cicling ring structure more than once
    int max_iters = MAX_NUMBER_NODES + 1;

    while (!half_left_open_interval(targetId, predecessor->id, predecessor->successor->id)) {
        if (--max_iters < 0) {
            log_warn("remote_find_predecessor: iteration limit reached for target %d", targetId);
            break;
        }
        //Checks loop condition where node's predecessor points to itself
        if (strcmp(localNodeIp, predecessor->Ip) == 0) {
            pthread_mutex_lock(&s->lock);
            Node* cpf = closest_preceding_finger(s->localNode, targetId);
            Node* cpf_copy = copyNode(cpf);
            pthread_mutex_unlock(&s->lock);

            // No-progress guard: finger table can't advance further, exit loop
            if (cpf_copy->id == predecessor->id) {
                freeNode(cpf_copy);
                break;
            }

            freeNode(predecessor);
            predecessor = cpf_copy;

        } else {
            //Checks if init socket fails and returns NULL if it does
            int sock = init_socket(predecessor->Ip, port);
            if (sock < 0) {
                freeNode(predecessor);
                return NULL;
            }
            
            //  Send request
            char request[64];
            snprintf(request, sizeof(request), "CLOSEST_PRECEDING_FINGER %d\n", targetId);

            if (send(sock, request, strlen(request), 0) < 0) {
                perror("send");
                close(sock);
                freeNode(predecessor);
                return NULL;
            }

            //  Receive response with timeout protection
            char response[128];
            int total = 0;
            int n = 0;

            //Extract response from server
            while (total < sizeof(response) - 1) {
                n = recv(sock, response + total, sizeof(response) - total - 1, 0);
                if (n <= 0) break;

                //Tracks position for writing to buffer
                total += n;
                response[total] = '\0';

                if (strchr(response, '\n')) break; // end of message
            }

            if (n <= 0) {
                perror("recv");
                close(sock);
                freeNode(predecessor);
                return NULL;
            }

            response[total] = '\0';

            int node_id;
            char node_ip[64];

            //Parse information into node structures
            if (sscanf(response, "NODE %d %63s", &node_id, node_ip) == 2) {
                freeNode(predecessor);
                Node* tmpNode = createNode(node_id, node_ip, "shared/files");
                predecessor = remote_get_node(s, port, tmpNode->Ip);
                freeNode(tmpNode);
            } else {
                fprintf(stderr, "RPC error: %s\n", response);
                close(sock);
                freeNode(predecessor);
                return NULL;
            }

            //Checks if predecessor is a valid node
            if (predecessor == NULL) {
                log_error("Failed to create node from closes finger response: %s\n", response);
                break;
            }

            close(sock);
        }
    }
    
    return predecessor;
}

//Distributed implementation of Chord algorithm find_successor()
Node* remote_find_successor(t_server* s, int port, int targetId) {
    Node* temp = remote_find_predecessor(s, port, targetId);

    if (temp == NULL) {
        log_error("remote_find_successor: remote_find_predecessor returned NULL for target %d\n", targetId);
        return NULL;
    }

    if (temp->successor == NULL) {
        log_error("remote_find_successor: predecessor has NULL successor\n");
        freeNode(temp);
        return NULL;
    }

    Node* succ = remote_get_node(s, port, temp->successor->Ip);

    pthread_mutex_lock(&s->lock);
    log_info("Succesfull find_successor process completed at node %d %s", s->localNode->id, s->localNode->Ip);
    pthread_mutex_unlock(&s->lock);

    freeNode(temp);
    return succ;
}

//Distributed implementation of Chord algorithm join()
void remote_join(const char* existingIp, int port, t_server* s) {
    //Copy relevant node information
    pthread_mutex_lock(&s->lock);
    s->localNode->predecessor = NULL;
    int local_id = s->localNode->id;
    pthread_mutex_unlock(&s->lock);

    int sock = init_socket(existingIp, port);
    if (sock < 0) {
        return;
    }

    char request[64];

    //Asks for successor of node where we join from, will be assigned as this nodes successor
    snprintf(request, sizeof(request), "FIND_SUCCESSOR %d\n", local_id);
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return;
    }

    char response[128] = {0};
    int total = 0, n;
    while (total < (int)sizeof(response) - 1) {
        n = recv(sock, response + total, sizeof(response) - total - 1, 0);
        if (n <= 0) break;
        total += n;
        response[total] = '\0';
        if (strchr(response, '\n')) break;
    }
    close(sock);

    int succ_id;
    char succ_ip[64];

    //Verify that ID and IP from successor node is recovered
    if (sscanf(response, "NODE %d %63s", &succ_id, succ_ip) == 2) {
        Node* tempSucc = createNode(succ_id, succ_ip, "shared/files");
        pthread_mutex_lock(&s->lock);

        s->localNode->predecessor = NULL;

        // Update localNode->successor
        Node* old_succ = s->localNode->successor;
        if (old_succ != NULL && old_succ != s->localNode) freeNode(old_succ);
        s->localNode->successor = tempSucc;

        // Keep fingerTable[0] in sync (separate allocation — remote_stabilize
        // owns and frees localNode->successor independently)
        Node* old_f0 = s->localNode->fingerTable[0].successor;
        if (old_f0 != NULL && old_f0 != s->localNode) freeNode(old_f0);
        s->localNode->fingerTable[0].successor = createNode(succ_id, succ_ip, "shared/files");
        strncpy(s->localNode->fingerTable[0].Ip, succ_ip, MAX_IP_LENGTH - 1);
        s->localNode->fingerTable[0].Ip[MAX_IP_LENGTH - 1] = '\0';

        //Persist changes to disk
        saveNodeToFile(s->localNode, "nodeInfo/Node");
        saveFingerTableToFile(s->localNode, "nodeInfo/FingerTable");

        pthread_mutex_unlock(&s->lock);
        log_info("remote_join: joined ring, successor is node %d %s", succ_id, succ_ip);
    } else {
        log_error("remote_join: failed to parse FIND_SUCCESSOR response: %s", response);
    }
}

//Distributed implementation of Chord algorithm notify()
void remote_notify(t_server* s, int port, const char* existingIp) {
    //Copy relevant node information
    pthread_mutex_lock(&s->lock);
    if (s->localNode == NULL) {
        pthread_mutex_unlock(&s->lock);
        log_error("In remote_notify() Local node is NULL");
        return;
    }

    int local_id = s->localNode->id;
    char local_ip[64];
    strncpy(local_ip, s->localNode->Ip, sizeof(local_ip) - 1);
    local_ip[sizeof(local_ip) - 1] = '\0';
    pthread_mutex_unlock(&s->lock);

    // Self-notify (single-node ring) — no TCP needed, handle directly
    if (strcmp(existingIp, local_ip) == 0) {
        pthread_mutex_lock(&s->lock);
        if (s->localNode->predecessor == NULL ||
            in_open_interval(s->localNode->id, s->localNode->predecessor->id, s->localNode->id)) {
            s->localNode->predecessor = s->localNode;
        }
        pthread_mutex_unlock(&s->lock);
        return;
    }

    // remote_get_node already avoids TCP self-connection via its self-detection guard
    Node* otherNode = remote_get_node(s, port, existingIp);
    if (otherNode == NULL) {
        log_warn("remote_notify: could not reach node at %s", existingIp);
        return;
    }

    pthread_mutex_lock(&s->lock);
    if (s->localNode != NULL &&
        (s->localNode->predecessor == NULL ||
         in_open_interval(otherNode->id, s->localNode->predecessor->id, s->localNode->id))) {
        // Free old predecessor before overwriting — matches notify()'s saveNodeToFile step
        Node* old_pred = s->localNode->predecessor;
        if (old_pred != NULL && old_pred != s->localNode) freeNode(old_pred);
        s->localNode->predecessor = otherNode;
        saveNodeToFile(s->localNode, "nodeInfo/Node");
    } else {
        freeNode(otherNode);
    }
    pthread_mutex_unlock(&s->lock);

    log_info("Succesfull notify process completed at node %d %s", local_id, local_ip);
}

//Fixes successor pointer of a node adjacent to another node that failed or shutdown
//Grabs first valid node as succesor
Node* finger_table_fallback(t_server* s, int port) {
    //Cicles every finger table entry 
    for (int i = 0; i < NODE_ID_LENGTH; i++) {

        pthread_mutex_lock(&s->lock);                                                                                                                                                                                       
        Node* finger = s->localNode->fingerTable[i].successor;
        //Passes table entries that point to self, not useful for updating successor
        if (finger == NULL || finger == s->localNode || finger->id == s->localNode->id) {
            pthread_mutex_unlock(&s->lock);
            continue;
        }

        char finger_ip[MAX_IP_LENGTH];                                                                                                                                                                                      
        strncpy(finger_ip, s->localNode->fingerTable[i].Ip, MAX_IP_LENGTH - 1);                                                                                                                                             
        finger_ip[MAX_IP_LENGTH - 1] = '\0';                                                                                                                                                                                
        int finger_id = finger->id;    
                                                                                                                                                                                             
        pthread_mutex_unlock(&s->lock);

        //Checks for empty or invalid IP
        if (finger_ip[0] == '\0') continue;

        int sock = init_socket(finger_ip, port);                                                                                                                                                                            
        if (sock < 0) {
            log_warn("finger_table_fallback: could not reach finger %d at %s", finger_id, finger_ip);
            continue;
        }                                                                                                                                                                                                                   
        close(sock);

        //Gets copy of possible successor
        Node* candidate = remote_get_node(s, port, finger_ip);                                                                                                                                                              
        if (candidate == NULL) continue;

        pthread_mutex_lock(&s->lock);  
        //Checks that previously registered succesor is valid, free node if necessary
        Node* old_succ = s->localNode->successor;                                                                                                                                                                           
        if (old_succ != NULL && old_succ != s->localNode) freeNode(old_succ);                                                                                                                                               
        s->localNode->successor = candidate;

        // old_succ (freed above) and fingerTable[0].successor alias the same pointer — do NOT free again.
        s->localNode->fingerTable[0].successor = createNode(candidate->id, candidate->Ip, candidate->fileContentPath);
        strncpy(s->localNode->fingerTable[0].Ip, candidate->Ip, MAX_IP_LENGTH - 1);
        s->localNode->fingerTable[0].Ip[MAX_IP_LENGTH - 1] = '\0';
        
        //Persist changes to disk
        saveNodeToFile(s->localNode, "nodeInfo/Node");                                                                                                                                                                      
        saveFingerTableToFile(s->localNode, "nodeInfo/FingerTable");   

        pthread_mutex_unlock(&s->lock);                                                                                                                                                                                     
                                                                                                                                                                                                                            
        log_info("finger_table_fallback: promoted finger %d (%s) as new successor", candidate->id, candidate->Ip);
        Node* ret = copyNode(candidate);

        // Null out predecessor so remote_stabilize's in_open_interval check cannot
        // immediately override the just-installed successor with a stale remote value.
        if (ret != NULL) { freeNode(ret->predecessor); ret->predecessor = NULL; }
        return ret;
    }

    log_error("finger_table_fallback: no reachable finger found");                                                                                                                                                          
    return NULL; 
}

//Distributed implementation of Chord algorithm stabilize()
void remote_stabilize(t_server* s, int port) {
    // Get successor IP and node ID while holding lock
    pthread_mutex_lock(&s->lock);
    if (s->localNode == NULL || s->localNode->successor == NULL) {
        pthread_mutex_unlock(&s->lock);
        log_warn("[WARN] Local node or successor is NULL in remote_stabilize\n");
        return;
    }
    char successor_ip[64];
    strncpy(successor_ip, s->localNode->successor->Ip, sizeof(successor_ip) - 1);
    successor_ip[sizeof(successor_ip) - 1] = '\0';
    int local_id = s->localNode->id;
    char local_ip[64];
    strncpy(local_ip, s->localNode->Ip, sizeof(local_ip) - 1);
    local_ip[sizeof(local_ip) - 1] = '\0';
    int local_port = s->port;
    pthread_mutex_unlock(&s->lock);

    //Indicates if succesor node failed and fallback process was necessary
    int used_fallback = 0;

    Node* temp = remote_get_node(s, local_port, successor_ip);

    if (temp == NULL) {
        log_warn("[WARN] Could not get successor node info\n");

        //Returns if the fallback was unsuccessfull
        temp = finger_table_fallback(s, local_port);
        if (temp == NULL) {
            return;
        }
        used_fallback = 1;
    }

    Node* x = temp->predecessor;

    // Skip the successor-override step when the fallback already installed the best
    // available live node — using temp->predecessor here would immediately undo the
    // fallback with whatever stale/self-referential predecessor the remote peer reports.
    if (!used_fallback && x != NULL && in_open_interval(x->id, local_id, temp->id)) {
        Node* new_successor = createNode(x->id, x->Ip, x->fileContentPath);
        if (new_successor == NULL) {
            perror("Failed to allocate memory for new successor");
            freeNode(temp);
            return;
        }

        pthread_mutex_lock(&s->lock);
        if (s->localNode != NULL) {
            Node* old_succ = s->localNode->successor;
            if (old_succ != NULL && old_succ != s->localNode) freeNode(old_succ);
            s->localNode->successor = new_successor;

            // Keep fingerTable[0] in sync — matching stabilize()'s saveNodeToFile step
            Node* old_f0 = s->localNode->fingerTable[0].successor;
            if (old_f0 != NULL && old_f0 != s->localNode) freeNode(old_f0);
            s->localNode->fingerTable[0].successor = createNode(x->id, x->Ip, x->fileContentPath);
            strncpy(s->localNode->fingerTable[0].Ip, x->Ip, MAX_IP_LENGTH - 1);
            s->localNode->fingerTable[0].Ip[MAX_IP_LENGTH - 1] = '\0';

            //Persist changes to disk
            saveNodeToFile(s->localNode, "nodeInfo/Node");
            saveFingerTableToFile(s->localNode, "nodeInfo/FingerTable");
        } else {
            freeNode(new_successor);
        }
        pthread_mutex_unlock(&s->lock);
    }

    pthread_mutex_lock(&s->lock);
    if (s->localNode == NULL || s->localNode->successor == NULL) {
        pthread_mutex_unlock(&s->lock);
        freeNode(temp);
        return;
    }
    strncpy(successor_ip, s->localNode->successor->Ip, sizeof(successor_ip) - 1);
    successor_ip[sizeof(successor_ip) - 1] = '\0';
    pthread_mutex_unlock(&s->lock);


    /*
       =============================================================================
        Calls remote communications to initiate remote_notify() in the succesor node
       ============================================================================= 
    */
    int sock = init_socket(successor_ip, local_port);

    if (sock < 0) {
        freeNode(temp);
        return;
    }

    // Send request
    char request[64];
    //TCP server call that initiates a background notify process in remote node
    snprintf(request, sizeof(request), "STABILIZE %.15s\n", local_ip);

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        freeNode(temp);
        return;
    }

    log_info("Stabilize request sent, awaiting response. Sent from node %d %s", local_id, local_ip);

    // Receive response with timeout
    char response[128];
    int total = 0;
    int n = 0;

    //Extract response from server
    while (total < sizeof(response) - 1) {
        n = recv(sock, response + total, sizeof(response) - total - 1, 0);
        if (n <= 0) break;

        total += n;
        response[total] = '\0';

        if (strchr(response, '\n')) break; // end of message
    }

    //Checks if no response was received
    if (n <= 0) {
        perror("recv");
        close(sock);
        freeNode(temp);
        return;
    }

    response[total] = '\0';

    //Checks if response message is not 'OK'
    if (strncmp(response, "OK", 2) != 0) {
        log_error(response);
    }

    close(sock);

    log_info("Succesfull stabilize process completed at node %d %s", local_id, local_ip);

    freeNode(temp);

    return;
}

//Debugging tool to check Chord ring structure in a distributed network
void remote_check_ring(t_server* s) {
    // Get local node info copy
    pthread_mutex_lock(&s->lock);
    if (s->localNode == NULL || s->localNode->successor == NULL) {
        pthread_mutex_unlock(&s->lock);
        log_error("In remote_check_ring() Local node or successor is NULL");
        return;
    }
    char successor_ip[64];
    strncpy(successor_ip, s->localNode->successor->Ip, sizeof(successor_ip) - 1);
    successor_ip[sizeof(successor_ip) - 1] = '\0';
    int local_id = s->localNode->id;
    char local_ip[64];
    strncpy(local_ip, s->localNode->Ip, sizeof(local_ip) - 1);
    local_ip[sizeof(local_ip) - 1] = '\0';
    int port = s->port;
    pthread_mutex_unlock(&s->lock);

    int sock = init_socket(successor_ip, port);

    if (sock < 0) {
        log_error("In remote_check_ring() Unable to establish socket to %s in node %d %s", successor_ip, local_id, local_ip);
        return;
    }
    
    //  Send request
    char request[64];
    //TCP server action (checks predecessor and successor matching)
    //Initiates check function through every node and stops when looping back to source node
    snprintf(request, sizeof(request), "CHECK_RING %d\n", local_id);

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        return;
    }

    close(sock);

    log_info("Check ring process started in node %d %s", local_id, local_ip);

    return;
}

//Distributed implementation of Chord algorithm fix_fingers()
void remote_fix_fingers(t_server* s) {
    //Generates a random index to choose a finger table entry
    int i = 1 + rand() % NODE_ID_LENGTH;

    //Copy node information to temp variables
    pthread_mutex_lock(&s->lock);
    int start = s->localNode->fingerTable[i-1].start;
    int local_id = s->localNode->id;
    pthread_mutex_unlock(&s->lock);

    Node* temp = remote_find_successor(s, s->port, start);

    if (temp != NULL) {
        pthread_mutex_lock(&s->lock);

        // Each slot owns a separate allocation — finger table and localNode->successor
        // must never share the same pointer, or remote_stabilize will double-free.
        Node* old_f = s->localNode->fingerTable[i-1].successor;
        if (old_f != NULL && old_f != s->localNode) freeNode(old_f);
        s->localNode->fingerTable[i-1].successor = createNode(temp->id, temp->Ip, temp->fileContentPath);
        strncpy(s->localNode->fingerTable[i-1].Ip, temp->Ip, MAX_IP_LENGTH - 1);
        s->localNode->fingerTable[i-1].Ip[MAX_IP_LENGTH - 1] = '\0';

        // finger[0] == direct successor; keep localNode->successor in sync
        if (i == 1) {
            Node* old_succ = s->localNode->successor;
            if (old_succ != NULL && old_succ != s->localNode) freeNode(old_succ);
            s->localNode->successor = createNode(temp->id, temp->Ip, temp->fileContentPath);
            saveNodeToFile(s->localNode, "nodeInfo/Node");
        }

        saveFingerTableToFile(s->localNode, "nodeInfo/FingerTable");
        pthread_mutex_unlock(&s->lock);
        freeNode(temp);
    } else {
        log_error("Could not succesfully update fingerTable entry %d in node %d", i, local_id);
    }

    return;
}
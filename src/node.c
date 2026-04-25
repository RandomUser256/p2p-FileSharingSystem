/*
Considerations for node.c:
*/

/*
ERRORS
    - When looking for the successor of an ID that is active in the ring, it returns that same ID instead of its successor 
*/


#include <stdio.h>
#include <stdbool.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>


#define MAX_IP_LENGTH 16
#define MAX_FILE_PATH_LENGTH 256
#define NODE_ID_LENGTH 4 //in bits, equal to 16 total nodes
#define MAX_NUMBER_NODES (1U << NODE_ID_LENGTH) //Maximum number of nodes in the network

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

//Forward declaration of Node struct
struct Node;

//Finger table

typedef struct FingerTableEntry {
    int start;
    //Node* parent_node;
    int lowerIntervalLimit;
    int upperIntervalLimit;
    struct Node* successor;
    char Ip[MAX_IP_LENGTH];
} FingerTableEntry;

//Node 
typedef struct Node {
    int id;
    char Ip[MAX_IP_LENGTH];
    struct Node* successor;
    struct Node* predecessor;

    char fileContentPath[MAX_FILE_PATH_LENGTH];  

    //Finger table
    struct FingerTableEntry fingerTable[NODE_ID_LENGTH];
} Node;

bool nullCheckNode(Node* node) {
    if (node) {
        return true;
    }
    return false;
}

bool nullCheckFingerTable(FingerTableEntry* entry) {
    return nullCheckNode(entry->successor);
}

FingerTableEntry createFingerTableEntry(int entryNumber, Node* parent_node, Node* successor) {
    FingerTableEntry entry;
    // Bit shift notation used to substitute exponent function
    entry.start = (parent_node->id + (1U << (entryNumber - 1))) % MAX_NUMBER_NODES; // Calculate the start of the interval
    //entry.parent_node = parent_node;
    entry.lowerIntervalLimit = entry.start;
    entry.upperIntervalLimit = (parent_node->id + (1U << entryNumber)) % MAX_NUMBER_NODES;

    //Add find succesor function
    entry.successor = successor;


    if (successor != NULL) {
        //Check if the successor's IP fits within the defined maximum length
        strncpy(entry.Ip, successor->Ip, MAX_IP_LENGTH - 1);
        entry.Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination

    } else {
        entry.Ip[0] = '\0'; // No successor, set IP to empty string
    }
    
    return entry;
}

void updateValuesFingerTable(Node* node) {
    for (int i = 1; i <= NODE_ID_LENGTH; i++) {
        node->fingerTable[i-1].start =
            (node->id + (1U << (i-1))) % MAX_NUMBER_NODES;

        node->fingerTable[i-1].lowerIntervalLimit = node->fingerTable[i-1].start;

        if (i < NODE_ID_LENGTH - 1) {
            node->fingerTable[i-1].upperIntervalLimit = node->fingerTable[i].start;
        } else {
            node->fingerTable[i-1].upperIntervalLimit = node->fingerTable[0].start; // Wrap around for the last entry
        }

        //node->fingerTable[i].upperIntervalLimit = node->fingerTable[i+1].start;

        node->fingerTable[i-1].successor = node;

        node->fingerTable[i-1].Ip[0] = '\0';
    }
}

//Forward declaration
Node* createNode(int id, const char* ip, const char* fileContentPath);

Node* loadNodeFromFile(const char* filepath) {
    FILE* file = fopen(filepath, "r");

    if (!file) {
        fprintf(stderr, "Error opening Node file\n");
        return NULL;
    }

    int  id    = 0;
    char ip[MAX_IP_LENGTH]          = {0};
    char path[MAX_FILE_PATH_LENGTH] = {0};
    int  succId = -1; char succIp[MAX_IP_LENGTH] = {0};
    int  predId = -1; char predIp[MAX_IP_LENGTH] = {0};

    char line[512];

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (sscanf(line, "id=%d",               &id)             == 1) continue;
        if (sscanf(line, "ip=%15s",              ip)              == 1) continue;
        if (sscanf(line, "fileContentPath=%255s", path)           == 1) continue;
        if (sscanf(line, "successor=%d %15s",   &succId, succIp)  == 2) continue;
        if (sscanf(line, "predecessor=%d %15s", &predId, predIp)  == 2) continue;
    }

    fclose(file);

    Node* node = createNode(id, ip, path);

    if (succId != -1)
        node->successor   = createNode(succId, succIp, "");
    if (predId != -1)
        node->predecessor = createNode(predId, predIp, "");

    fprintf(stderr, "Loaded node: ID=%d IP=%s PATH=%s succ=%d pred=%d\n",
            id, ip, path, succId, predId);

    return node;
}

void saveNodeToFile(Node* node, const char* filepath) {
    FILE* file = fopen(filepath, "w");

    if (!file) {
        printf("Error saving node\n");
        return;
    }

    fprintf(file, "id=%d\n", node->id);
    fprintf(file, "ip=%s\n", node->Ip);
    fprintf(file, "fileContentPath=%s\n", node->fileContentPath);

    if (node->successor)
        fprintf(file, "successor=%d %s\n", node->successor->id, node->successor->Ip);

    if (node->predecessor)
        fprintf(file, "predecessor=%d %s\n", node->predecessor->id, node->predecessor->Ip);

    fclose(file);
}

//Review function as node properties change
Node* createNode(int id, const char* ip, const char* fileContentPath) {
    Node* newNode = malloc(sizeof(Node));
    newNode->id = id;
    strncpy(newNode->Ip, ip, MAX_IP_LENGTH - 1);
    newNode->Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination
    strncpy(newNode->fileContentPath, fileContentPath, MAX_FILE_PATH_LENGTH - 1);
    newNode->fileContentPath[MAX_FILE_PATH_LENGTH - 1] = '\0'; // Ensure null-termination
    newNode->successor = newNode;
    newNode->predecessor = newNode;

    //newNode->fingerTable[0] = createFingerTableEntry(1, newNode, newNode); // Initialize the first entry in the finger table with the new node as its own successor
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

Node* remote_find_successor(const char* ip, int targetId);
Node* remote_get_successor(const char* ip);
Node* remote_closest_preceding_finger(const char* ip, int targetId);
void freeNode(Node* node);

/*
Node* find_predecessor(Node* node, int targetId) {
    if (!node) {
        return NULL;
    }

    Node* n = node;

    while (!half_left_open_interval(targetId, n->id, n->successor->id)) {
        Node* next = closest_preceding_finger(n, targetId);

        if (next == n) break;  // prevent infinite loop

        n = next;
    }

    return n;
}*/

Node* find_predecessor(Node* startNode, int id) {
    if (!startNode || !startNode->successor) {
        printf("Local node has no successor loaded\n");
        return NULL;
    }

    // Early exit using local struct data (no SSH to self):
    // if id ∈ (startNode, startNode->successor] then startNode is the predecessor.
    // Skip when id == startNode->id — that requires a full wraparound walk.
    if (id != startNode->id) {
        if (half_left_open_interval(id, startNode->id, startNode->successor->id)) {
            // Always return a fresh heap allocation so callers can freeNode safely.
            return createNode(startNode->id, startNode->Ip, "");
        }
    }

    // Seed the walk. If the local finger table is empty (all entries point to
    // self), closest_preceding_finger will return startNode itself — in that
    // case advance directly to startNode's successor so the walk moves forward.
    Node* cpf = remote_closest_preceding_finger(startNode->Ip, id);
    if (cpf == NULL) return NULL;

    Node* n;
    if (cpf->id == startNode->id) {
        // Finger table gave us nothing useful — step to successor instead.
        freeNode(cpf);
        n = createNode(startNode->successor->id, startNode->successor->Ip, "");
    } else {
        n = cpf;
    }

    // Walk the ring hop by hop, advancing through successors when no better
    // finger is available. Every pointer except startNode is heap-allocated
    // by createNode/remote_* so it is safe to freeNode on every path.
    int maxHops = MAX_NUMBER_NODES;
    while (maxHops-- > 0) {
        Node* nSucc = remote_get_successor(n->Ip);
        if (nSucc == NULL) {
            printf("Error getting successor during traversal\n");
            freeNode(n);
            return NULL;
        }

        // id ∈ (n, nSucc] → n is the predecessor
        if (half_left_open_interval(id, n->id, nSucc->id)) {
            freeNode(nSucc);
            return n;
        }

        Node* next = remote_closest_preceding_finger(n->Ip, id);
        freeNode(nSucc);

        if (next == NULL) {
            printf("Error getting closest preceding finger\n");
            freeNode(n);
            return NULL;
        }

        if (next->id == n->id) {
            // Finger table on this node is also empty — advance to its successor
            // rather than stopping, so the walk keeps making progress.
            freeNode(next);
            Node* succ = remote_get_successor(n->Ip);
            if (succ == NULL || succ->id == n->id) {
                // Truly stuck — return best guess
                freeNode(succ);
                return n;
            }
            freeNode(n);
            n = succ;
            continue;
        }

        freeNode(n);
        n = next;
    }

    printf("find_predecessor: max hops reached\n");
    freeNode(n);
    return NULL;
}

/*
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
*/

Node* find_successor(Node* node, int id) {
    Node* pred = find_predecessor(node, id);

    if (pred == NULL) return NULL;

    return remote_get_successor(pred->Ip);
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
            newNode->fingerTable[i-1] = createFingerTableEntry(i, newNode, newNode->fingerTable[i-1].successor); // If the start of the interval for the current entry in the existing node's finger table falls within a certain range, copy that entry to the new node's finger table
        }
        else {
            existingNode->fingerTable[i-1] = createFingerTableEntry(i, newNode, find_successor(newNode, newNode->fingerTable[i-2].start)); // Otherwise, create a new entry in the new node's finger table based on the existing node's information
        }
    }
    return newNode; // Return the initialized node with its finger table set up
}

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

//Second version of join, supports concurrent node joins
void join(Node* existingNode, Node* newNode) {
    if (!newNode) {
        return;
    }

    if (existingNode == NULL) {
        // Initializing first node when it is the only one in the network
        newNode->successor = newNode;
        newNode->predecessor = newNode;

        /*
        for (int i = 0; i < NODE_ID_LENGTH; i++) {
            newNode->fingerTable[i].successor = newNode;
        }
        */
        return;
    } else {
        Node* successor = find_successor(existingNode, newNode->id); // Find the successor of the existing node using the new node as a reference point

        newNode->successor = successor; // Update the successor of the existing node to point to the new node

        newNode->predecessor = successor->predecessor; // Update the predecessor of the existing node to point to the new node

        if (successor->predecessor != NULL) {
            successor->predecessor->successor = newNode;
        }

        successor->predecessor = newNode; // Update the predecessor of the existing node's successor to point to the new node
    }
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

    //update_finger_table(node, node->fingerTable[i-1].successor, i); // Update the start, lower interval limit, and upper interval limit for the selected entry in the finger table
}
    
//Second version of notify, supports concurrent node joins
void notify(Node* node, Node* potentialPredecessor) {
    if (!node || !potentialPredecessor) {
        return;
    }

    if ( (node->predecessor == NULL || in_open_interval(potentialPredecessor->id, node->predecessor->id, node->id))) {
        node->predecessor = potentialPredecessor; // Update the predecessor of the node to the new potential predecessor if it does not already have a predecessor
    }
}

//Second version of stabilize, supports concurrent node joins
void stabilize(Node* node) {
    if (!node || !node->successor) {
        return;
    }

    Node* x = node->successor->predecessor; // Get the predecessor of the current node's successor

    if (x!= NULL && in_open_interval(x->id, node->id, node->successor->id)) {
        node->successor = x;
    }

    notify(node->successor, node); // Notify the successor about the current node as a potential predecessor
}

void freeNode(Node* node) {
    if (node != NULL) {
        free(node);
    }
}

//Troublshooting functions

void nodePrint(Node* node) {
    if (node == NULL) {
        printf("Node is NULL.\n");
        return;
    }
    printf("Node ID: %d\n", node->id);
    printf("Node IP: %s\n", node->Ip);
    if (node->successor != NULL) {
        printf("Node Succesor ID: %d\n", node->successor->id);
    } else {
        printf("Node Succesor: NULL\n");
    }
    if (node->fileContentPath[0] != '\0') {
        printf("Node File Content Path: %s\n", node->fileContentPath);
    } else {
        printf("Node File Content Path: None\n");
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

void check_ring(Node* start) {
    Node* curr = start;

    do {
        if (curr->successor->predecessor != curr) {
            printf("ERROR: successor->predecessor mismatch at node %d\n", curr->id);
        }
        if (curr->predecessor->successor != curr) {
            printf("ERROR: predecessor->successor mismatch at node %d\n", curr->id);
        }

        curr = curr->successor;
    } while (curr != start);
}

void executeSSH(const char* ip, const char* command) {
    char fullCommand[512];

    sprintf(fullCommand, "ssh %s \"%s\"", ip, command);

    printf("SSH EXEC: %s\n", fullCommand);

    system(fullCommand);
}

/*
   FINGER TABLE PERSISTENCE 
   - Save/load finger table to/from disk
*/

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

    fprintf(file, "# Finger Table for Node %d\n", node->id);
    fprintf(file, "# Format: entry=<idx>,start=<start>,lower=<lower>,upper=<upper>,successor_id=<id>,successor_ip=<ip>\n\n");

    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        FingerTableEntry* entry = &node->fingerTable[i];
        
        int succ_id = (entry->successor != NULL) ? entry->successor->id : -1;
        const char* succ_ip = (entry->successor != NULL && entry->Ip[0] != '\0') ? entry->Ip : "NONE";

        fprintf(file, "entry=%d,start=%d,lower=%d,upper=%d,successor_id=%d,successor_ip=%s\n",
                i,
                entry->start,
                entry->lowerIntervalLimit,
                entry->upperIntervalLimit,
                succ_id,
                succ_ip);
    }

    fclose(file);
    printf("[INFO] Finger table for node %d saved to %s\n", node->id, filepath);
}

void loadFingerTableFromFile(Node* node, const char* filepath) {
    if (!node) {
        printf("Error: node is NULL\n");
        return;
    }

    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("Warning: cannot open file %s for reading, finger table will use defaults\n", filepath);
        return;
    }

    char line[512];
    int entry_idx = 0;

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
            printf("Warning: skipping malformed line: %s\n", line);
            continue;
        }

        // Validate entry index
        if (idx < 0 || idx >= NODE_ID_LENGTH) {
            printf("Warning: invalid entry index %d, skipping\n", idx);
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
    printf("[INFO] Finger table for node %d loaded from %s\n", node->id, filepath);
}

void printFingerTable(Node* node) {
    if (!node) {
        printf("Error: node is NULL\n");
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

/* REMOTE FINGER TABLE OPERATIONS
   - Query finger tables from remote nodes via SSH
   - Rebuild complete finger table structures from remote machine
*/
void remote_print_finger_table(const char* ip) {
    char command[256];
    snprintf(command, sizeof(command),
        "ssh %s \"./scripts/node_comms print_finger_table\" 2>/dev/null",
        ip);

    system(command);
}

void remote_load_and_update_finger_table(Node* node, const char* remote_ip) {
    if (!node) {
        printf("Error: node is NULL\n");
        return;
    }

    printf("[INFO] Loading finger table from remote node at %s\n", remote_ip);

    char command[512];
    snprintf(command, sizeof(command),
        "ssh %s \"cat nodeInfo/FingerTable 2>/dev/null\" 2>/dev/null",
        remote_ip);

    FILE* fp = popen(command, "r");
    if (!fp) {
        printf("Warning: could not connect to remote node at %s\n", remote_ip);
        return;
    }

    char line[512];
    int entry_idx = 0;

    while (fgets(line, sizeof(line), fp) && entry_idx < NODE_ID_LENGTH) {
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
            continue;
        }

        if (idx < 0 || idx >= NODE_ID_LENGTH) {
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
            node->fingerTable[idx].successor = node;
            node->fingerTable[idx].Ip[0] = '\0';
        }

        entry_idx++;
    }

    pclose(fp);
    printf("[INFO] Remote finger table loaded successfully\n");
}

/* ======================================================================
   OPTIMIZED LOOKUP USING FINGER TABLES
   - Uses finger tables for O(log n) instead of O(n) lookups
   - Traverses nodes with finger table guidance
   ====================================================================== */

Node* find_successor_with_finger_table(Node* node, int id) {
    if (!node) {
        return NULL;
    }

    // Step 1: Quick local check - if id is in interval (node_id, successor_id]
    if (node->successor != NULL && 
        half_left_open_interval(id, node->id, node->successor->id)) {
        return createNode(node->successor->id, node->successor->Ip, "");
    }

    // Step 2: Find closest preceding finger using the finger table
    Node* cpf = closest_preceding_finger(node, id);
    if (cpf == NULL) {
        return node; // return self if non valid finger found
    }

    // Step 3: If local node is closest preceding node, proceed with regular lookup
    if (cpf->id == node->id) {
        return find_successor(node, id);
    }

    // Step 4: Remote lookup on the closest preceding finger
    return remote_find_successor(cpf->Ip, id);
}

Node* find_predecessor_with_finger_table(Node* node, int id) {
    if (!node || !node->successor) {
        printf("Error: node or successor is NULL\n");
        return NULL;
    }

    // Quick local check
    if (id != node->id && 
        half_left_open_interval(id, node->id, node->successor->id)) {
        //Return self in case of being the predecessor fo Target ID 
        return createNode(node->id, node->Ip, "");
    }

    // Use finger table to navigate efficiently
    Node* current = node;
    Node* cpf = closest_preceding_finger(current, id);
    
    if (cpf == NULL) {
        return NULL;
    }

    if (cpf->id == node->id) {
        // Finger table points to self, use remote traversal
        return find_predecessor(node, id);
    }

    // Perform remote lookup chain using finger table guidance
    int max_hops = NODE_ID_LENGTH + 2; // Log(n) hops expected
    
    while (max_hops-- > 0) {
        Node* next_cpf = remote_closest_preceding_finger(cpf->Ip, id);
        
        if (next_cpf == NULL) {
            return cpf; // Return best guess so far
        }

        Node* succ = remote_get_successor(cpf->Ip);
        if (succ == NULL) {
            freeNode(next_cpf);
            return cpf;
        }

        // Check if predecessor found
        if (half_left_open_interval(id, cpf->id, succ->id)) {
            freeNode(next_cpf);
            freeNode(succ);
            return cpf;
        }

        freeNode(succ);
        
        // Move to next node indicated by finger table
        if (next_cpf->id != cpf->id && cpf != node) {
            freeNode(cpf);
        }
        
        cpf = next_cpf;
    }

    printf("Warning: max hops reached in find_predecessor_with_finger_table\n");
    return cpf;
}

void remote_stabilize(Node *node) {
    char command[256];

    Node* succesor = remote_find_successor(node->Ip, node->id);

    Node* predecessor = remote_closest_preceding_finger()

    snprintf(command, sizeof(command), "ssh %s \"./node_comms stabilize\"", node->Ip);
    system(command);
}
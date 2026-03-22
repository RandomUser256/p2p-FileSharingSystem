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

//Review function as node properties change
Node* createNode(int id, const char* ip) {
    Node* newNode = malloc(sizeof(Node));
    newNode->id = id;
    strncpy(newNode->Ip, ip, MAX_IP_LENGTH - 1);
    newNode->Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination
    newNode->successor = newNode;
    newNode->predecessor = newNode;
    newNode->fileContentPath[0] = '\0'; // Initialize file content path to empty string

    //newNode->fingerTable[0] = createFingerTableEntry(1, newNode, newNode); // Initialize the first entry in the finger table with the new node as its own successor
    // Initialize finger table
    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        newNode->fingerTable[i].start =
            (id + (1U << (i-1))) % MAX_NUMBER_NODES;

        newNode->fingerTable[i].lowerIntervalLimit = newNode->fingerTable[i].start;

        if (i < NODE_ID_LENGTH - 2) {
            newNode->fingerTable[i].upperIntervalLimit = (id + (1U << i)) % MAX_NUMBER_NODES;
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
}

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

void freeNode(Node* node) {
    if (node != NULL) {
        free(node);
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
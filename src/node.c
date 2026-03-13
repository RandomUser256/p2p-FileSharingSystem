/*
Considerations for node.c:
- Finger table: in fucntions regarding the finger table, sections that call "fingerTableEntry[i].successor" may need to be change to "fingerTableEntry[i].start". Have to check algorithm
    - In sudo code it is sections that use 'finger[i].node'
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
    entry.start = ((parent_node->id + (1U << (entryNumber - 1))) % MAX_NUMBER_NODES); // Calculate the start of the interval
    //entry.parent_node = parent_node;
    entry.lowerIntervalLimit = entry.start;
    entry.upperIntervalLimit = ((parent_node->id + (1U << entryNumber)) % MAX_NUMBER_NODES);

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
            (id + (1U << i)) % MAX_NUMBER_NODES;

        newNode->fingerTable[i].lowerIntervalLimit = 0;
        newNode->fingerTable[i].upperIntervalLimit = 0;

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
        //ERROR: successor->id is not readable, cuases memery exception

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

    Node* node2 = node;
    
    //Traverse network until we find a range between two nodes where the targetId fits
    while (node2->successor != NULL && !in_open_interval(targetId, node2->id, node2->successor->id)) {
        //node2 = closest_preceding_finger(node2, targetId); // Move to the closest preceding finger to continue searching for the predecessor
        Node* next = closest_preceding_finger(node2, targetId);

        if (next == node2)
            break;

        node2 = next;
    }

    return node2; // Return the predecessor node
}

Node* find_successor(Node* node, int targetId) {
    if (!node) {
        return NULL;
    }

    Node* node2 = find_predecessor(node, targetId);
    if (node2 != NULL) {
        return node2->successor; // The successor of the predecessor is the successor of the target ID
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
            existingNode->fingerTable[i-1] = createFingerTableEntry(i, newNode, existingNode->fingerTable[i-1].successor); // If the start of the interval for the current entry in the existing node's finger table falls within a certain range, copy that entry to the new node's finger table
        }
        else {
            existingNode->fingerTable[i-1] = createFingerTableEntry(i, newNode, find_successor(newNode, existingNode->fingerTable[i-2].start)); // Otherwise, create a new entry in the new node's finger table based on the existing node's information
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

void join(Node* existingNode, Node* newNode) {
    if (!existingNode) {
        return;
    }

    //Join a new node to the network using an existing node as a reference point
    if (newNode != NULL) {
        init_finger_table(existingNode, newNode); // Initialize the finger table of the new node based on an existing node in the network
        update_others(newNode); // Update the finger tables of existing nodes to include the new node
    }
    else {
        for (size_t i = 1; i <= NODE_ID_LENGTH; i++)
        {
            existingNode->fingerTable[i-1].successor = existingNode; // If there are no existing nodes in the network, initialize the finger table of the new node with itself as the successor for all entries
        }
        existingNode->predecessor = existingNode; // Set the predecessor of the existing node to itself if no valid predecessor exists
    }
}

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
}

void notify(Node* node, Node* potentialPredecessor) {
    if (!node || !potentialPredecessor) {
        return;
    }

    //Notify a node about a potential predecessor
    //This function can be used to inform a node about a new potential predecessor, allowing it to update its finger table and maintain the integrity of the network
    if (node->predecessor == NULL || (potentialPredecessor->id > node->predecessor->id && potentialPredecessor->id < node->id)) {
        node->predecessor = potentialPredecessor; // Update the predecessor of the node to the new potential predecessor
    }
}

void stabilize(Node* node) {
    if (!node || !node->successor) {
        return;
    }

    //Periodically check and update the successor and predecessor pointers to maintain the integrity of the network
    //This function can be called at regular intervals to ensure that the successor and predecessor pointers are accurate and up to date
    Node* x = node->successor->predecessor; // Get the predecessor of the current node's successor
    if (x != NULL && x->id > node->id && x->id < node->successor->id) {
        node->successor = x; // Update the successor pointer if a closer predecessor is found
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
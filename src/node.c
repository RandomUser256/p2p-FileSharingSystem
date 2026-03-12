/*
Considerations for node.c:
- Finger table: in fucntions regarding the finger table, sections that call "fingerTableEntry[i].successor" may need to be change to "fingerTableEntry[i].start". Have to check algorithm
    - In sudo code it is sections that use 'finger[i].node'
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_IP_LENGTH 16
#define MAX_FILE_PATH_LENGTH 256
#define NODE_ID_LENGTH 8 //in bits, equal to 16 total nodes
#define MAX_NUMBER_NODES (int)pow(2, NODE_ID_LENGTH) //Maximum number of nodes in the network

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

FingerTableEntry createFingerTableEntry(int entryNumber, Node* parent_node, Node* successor) {
    FingerTableEntry entry;
    entry.start = parent_node->id +  ((int)pow(2, entryNumber - 1)% (int)pow(2, MAX_NUMBER_NODES)); // Calculate the start of the interval
    //entry.parent_node = parent_node;
    entry.lowerIntervalLimit = entryNumber;
    entry.upperIntervalLimit = parent_node->id +  ((int)pow(2, entryNumber)% (int)pow(2, MAX_NUMBER_NODES));

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
Node createNode(int id, const char* ip) {
    Node newNode;
    newNode.id = id;
    strncpy(newNode.Ip, ip, MAX_IP_LENGTH - 1);
    newNode.Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination
    newNode.successor = NULL;
    newNode.predecessor = NULL;
    newNode.fileContentPath[0] = '\0'; // Initialize file content path to empty string

    newNode.fingerTable[0] = createFingerTableEntry(1, &newNode, NULL); // Initialize the first entry in the finger table with the new node as its own successor
    return newNode;
}

Node* find_predecessor(Node* node, int targetId) {
    Node* currentNode = node;
    //Traverse network until we find a range between two nodes where the targetId fits
    while (currentNode->successor != NULL && targetId < currentNode->id && targetId > currentNode->successor->id) {
        currentNode = currentNode->successor;
    }
    return currentNode; // Return the predecessor node
}

Node* find_successor(Node* node, int targetId) {
    Node* predecessor = find_predecessor(node, targetId);
    if (predecessor != NULL) {
        return predecessor->successor; // The successor of the predecessor is the successor of the target ID
    }
    return NULL; // If no predecessor is found, return NULL
} 

Node* closest_preceding_finger(Node* node, int targetId) {
    for (size_t i = MAX_NUMBER_NODES; i > 0; i--)
    {
        //if the successor of the finger table entry is valid and between current node's id and the target id, return that successor
        if (node->fingerTable[i-1].successor != NULL && node->fingerTable[i-1].successor->id > node->id && node->fingerTable[i-1].successor->id < targetId) {
            return node->fingerTable[i-1].successor;
        }
    }
    return node; // If no successor is found in the finger table, return the current node
}

//Functions for inserting a new node into the network and updating the finger tables of existing nodes
Node* init_finger_table(Node* existingNode, Node* newNode) {
    existingNode->fingerTable[1].successor = find_successor(existingNode, existingNode->fingerTable[1].start); // Initialize the second entry in the finger table based on the existing node's information

    //Initialize the finger table of the new node based on an existing node in the network
    for (size_t i = 1; i <= MAX_NUMBER_NODES; i++)
    {
        newNode->fingerTable[i-1] = createFingerTableEntry(i, newNode, find_successor(existingNode, newNode->fingerTable[i-1].start)); // Create and initialize each entry in the finger table based on the existing node's information
    }
    return newNode; // Return the initialized node with its finger table set up
}

void update_finger_table(Node* existingNode, Node* newNode, int tableEntryNumber) {
    //Update the finger table of the existing node to include the new node
    if (newNode->id >= existingNode->id && newNode->id <= existingNode->fingerTable[tableEntryNumber-1].upperIntervalLimit) {
        existingNode->fingerTable[tableEntryNumber-1].successor = newNode;
        
        Node* p = existingNode->predecessor;

        update_finger_table(p, newNode, tableEntryNumber); //Recursively update the finger tables of the predecessor nodes

        strncpy(existingNode->fingerTable[tableEntryNumber-1].Ip, newNode->Ip, MAX_IP_LENGTH - 1);

        existingNode->fingerTable[tableEntryNumber-1].Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination
    }
}

void update_others(Node* currentNode) {
    //Update the finger tables of existing nodes to include the new node
    for (size_t i = 1; i <= MAX_NUMBER_NODES; i++)
    {
        //Obtain the ID corresponding to the successor of the node we want
        Node* predecessor = find_predecessor(currentNode, (currentNode->id - (int)pow(2, i - 1) + MAX_NUMBER_NODES) % MAX_NUMBER_NODES); // Calculate the ID of the predecessor node for the current finger table entry
        Node* predNode = find_predecessor(currentNode, predecessor->id); // Find the predecessor node for the calculated ID
        update_finger_table(predNode, currentNode, i); // Update the finger table of the predecessor node to include the new node
    }
} 

void join(Node* existingNode, Node* newNode) {
    //Join a new node to the network using an existing node as a reference point
    if (newNode != NULL) {
        init_finger_table(existingNode, newNode); // Initialize the finger table of the new node based on an existing node in the network
        update_others(newNode); // Update the finger tables of existing nodes to include the new node
    }
    else {
        for (size_t i = MAX_NUMBER_NODES; i > 0; i--)
        {
            newNode->fingerTable[i-1].successor = existingNode; // If there are no existing nodes in the network, initialize the finger table of the new node with itself as the successor for all entries
        }
        existingNode->predecessor = newNode; // Set the predecessor of the existing node to the new node
    }
}

//Functions for maintaining the integrity of the finger tables and ensuring that they are up to date with the current state of the network
void fix_fingers(Node* node) {
    //Periodically check and update the finger tables to ensure they are accurate
    //This function can be called at regular intervals to maintain the integrity of the finger tables
    //Implementation would involve checking each entry in the finger table and updating it if necessary
    int i = rand() % MAX_NUMBER_NODES + 1; // Randomly select an entry in the finger table to check
    Node* fingerNode = node->fingerTable[i-1].successor;
    fingerNode->fingerTable[i-1].successor = find_successor(node, fingerNode->fingerTable[i-1].start); // Update the successor for the selected entry in the finger table
}

void notify(Node* node, Node* potentialPredecessor) {
    //Notify a node about a potential predecessor
    //This function can be used to inform a node about a new potential predecessor, allowing it to update its finger table and maintain the integrity of the network
    if (node->predecessor == NULL || (potentialPredecessor->id > node->predecessor->id && potentialPredecessor->id < node->id)) {
        node->predecessor = potentialPredecessor; // Update the predecessor of the node to the new potential predecessor
    }
}

void stabilize(Node* node) {
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
    Node* current = head;
    while (current != NULL) {
        nodePrint(current);
        current = current->successor;
    }
}
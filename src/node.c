#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_IP_LENGTH 16
#define MAX_FILE_PATH_LENGTH 256
#define NODE_ID_LENGTH 8 //in bits
#define MAX_NUMBER_NODES pow(2, NODE_ID_LENGTH) //Maximum number of nodes in the network

//Finger table

typedef struct FingerTableEntry {
    int start;
    //Node* parent_node;
    int lowerIntervalLimit;
    int upperIntervalLimit;
    Node* successor;
    char Ip[MAX_IP_LENGTH];
} FingerTableEntry;

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

//Node 

typedef struct Node {
    int id;
    char Ip[MAX_IP_LENGTH];
    struct Node* succesor;
    char fileContentPath[MAX_FILE_PATH_LENGTH];  

    //Finger table
    FingerTableEntry fingerTable[NODE_ID_LENGTH]; // Assuming a maximum of 8 entries for the finger table

} Node;

//Review function as node properties change
Node* createNode(int id, const char* ip) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed for new node.\n");
        return NULL;
    }
    newNode->id = id;
    strncpy(newNode->Ip, ip, MAX_IP_LENGTH - 1);
    newNode->Ip[MAX_IP_LENGTH - 1] = '\0'; // Ensure null-termination
    newNode->succesor = NULL;
    newNode->fileContentPath[0] = '\0'; // Initialize file content path to empty string
    return newNode;
}

Node* find_successor(Node* node, int targetId) {
    Node* predecessor = find_predecessor(node, targetId);
    if (predecessor != NULL) {
        return predecessor->succesor; // The successor of the predecessor is the successor of the target ID
    }
    return NULL; // If no predecessor is found, return NULL
} 

Node* find_predecessor(Node* node, int targetId) {
    Node* currentNode = node;
    //Traverse network until we find a range between two nodes where the targetId fits
    while (targetId < currentNode->id && targetId > currentNode->succesor->id && currentNode->succesor != NULL) {
        currentNode = currentNode->succesor;
    }
    return currentNode; // Return the predecessor node
}

Node* closest_preceding_finger(Node* node, int targetId) {
    for (size_t i = MAX_NUMBER_NODES; i > 0; i--)
    {
        //if the successor of the finger table entry is valid and between current node's id and the target id, return that successor
        if (node->fingerTable[i].successor != NULL && node->fingerTable[i].successor->id > node->id && node->fingerTable[i].successor->id < targetId) {
            return node->fingerTable[i].successor;
        }
    }
    return node; // If no successor is found in the finger table, return the current node
}


//Funciones para fines de desarrollo


void nodePrint(Node* node) {
    if (node == NULL) {
        printf("Node is NULL.\n");
        return;
    }
    printf("Node ID: %d\n", node->id);
    printf("Node IP: %s\n", node->Ip);
    if (node->succesor != NULL) {
        printf("Node Succesor ID: %d\n", node->succesor->id);
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
        current = current->succesor;
    }
}
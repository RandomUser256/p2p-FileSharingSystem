#include <stdio.h>
#include <stdbool.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//Cambiar a header file
#include "node.c"

/*
Pending changes:
    - Implement correct hashing of file identifiers
    - Add SSH querying to retrieve file content from the responsible node and store
    - Delete 'destinationNodeId' from inert() and replace with hashing of the file identifier to obtain the destination node ID (within lookup() )
*/

// DISCLAIMER: In this function change arguments to incorporate appropriate hashing of files
Node* lookup(Node* node, /*char* identifier*/ int identifier) {
    //Hash the identifier to get the corresponding node ID
    //int nodeId = hash(identifier);

    //Find the node responsible for the given identifier
    Node* responsibleNode = find_successor(node, /*nodeId*/ identifier);

    if (responsibleNode != NULL) {
        return responsibleNode;
    } else {
        printf("No responsible node found for identifier. Value of errno: %d\n", errno);
        return NULL;
    }
}

char* generateDestinationFilePath(Node* destinationNode, char* identifier) {
    char* destinationFilePath = destinationNode->fileContentPath;
    destinationFilePath = strcat(destinationFilePath, "/");
    destinationFilePath = strcat(destinationFilePath, identifier);

    if (strlen(destinationFilePath) >= MAX_FILE_PATH_LENGTH) {
        printf("Error: Destination file path exceeds maximum length. Value of errno: %d\n", errno);
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
        printf("Error: Destination node not found\n");
        return;
    }

    // Build destination path safely
    char destinationFilePath[MAX_FILE_PATH_LENGTH];

    strncpy(destinationFilePath, destinationNode->fileContentPath, MAX_FILE_PATH_LENGTH - 1);
    destinationFilePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

    strncat(destinationFilePath, "/", MAX_FILE_PATH_LENGTH - strlen(destinationFilePath) - 1);
    strncat(destinationFilePath, identifier, MAX_FILE_PATH_LENGTH - strlen(destinationFilePath) - 1);

    printf("Copying file to node %d (%s)\n",
           destinationNode->id,
           destinationNode->Ip); 
    //SCP COMMAND
    char command[512];

    sprintf(command, "scp %s %s:%s", sourceFilePath, destinationNode->Ip, destinationFilePath);

    printf("Executing: %s\n", command);

    int result = system(command);

    if(result != 0) {
        printf("Error: SCP failed\n");
    } else {
        printf("File transferred successfully\n");
    }
}

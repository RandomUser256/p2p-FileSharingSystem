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

    Node* destinationNode1 = lookup(hostNode, destinationNodeId);

    if (destinationNode1 == NULL) {
        return;
    }

    //Is missing SSH connectivity

    //Store the file path in the responsible node
    // Order of arguments: destination path, source path, maximum number of characters to copy

    FILE* file = fopen(sourceFilePath, "r");

    if (file == NULL) {
        printf("Error opening file: %s. Value of errno: %d\n", hostNode->fileContentPath+*identifier, errno);
        return;
    }

    //Missing SSH connectivity to copy the file content from the host node to the destination node

    char* destinationFilePath = generateDestinationFilePath(destinationNode1, identifier);

    if (destinationFilePath == NULL) {
        fclose(file);
        return;
    } 

    printf("Host node file content path: %s\n", destinationFilePath);

    FILE* destinationFile = fopen(destinationFilePath, "w");

    if (destinationFile == NULL) {
        printf("Error opening destination file: %s. Value of errno: %d\n", destinationNode1->fileContentPath+*identifier, errno);
        fclose(file);
        return;
    }

    //Reads the characters from the source file
    int c;

    //Copy the file content from the host node to the destination node
    while ((c=fgetc(file)) != EOF) {
        fputc(c, destinationFile);
    }


    //Close files
    fclose(file);
    fclose(destinationFile);

    return;
}
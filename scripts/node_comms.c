#include <stdio.h>
#include <stdlib.h> 
#include <string.h>

#include "../src/DHASH.c"

// Called from another node to read the contents of the node it's accessing
// Basically communication to traverse the ring

int main(int argc, char* argv[]) {

    Node* node = loadNodeFromFile("nodeInfo/Node");

    if(node == NULL) {
        fprintf(stderr, "Error, couldn't load node correctly\n");
        return 1;
    }

    if(argc < 2) {
        fprintf(stderr, "Error, not enough arguments in the call");
        return 1;
    }

    //find_successor <id>
    if(strcmp(argv[1], "find_successor") == 0) {
        if(argc < 3) {
            fprintf(stderr, "Error, not enough arguments in the call");
            return 1;
        }

        int targetId = atoi(argv[2]);

        Node* result = find_successor(node, targetId);

        if(result != NULL) {
            printf("%d %s\n", result->id, result->Ip);
        } else {
            fprintf(stderr, "ERROR, process didn't execute fully or correctly\n");
        }
    }

    // get_successor
    if (strcmp(argv[1], "get_successor") == 0) {
        if (node->successor != NULL) {
            printf("%d %s\n", node->successor->id, node->successor->Ip);
        }
        return 0;
    }

    // closest_preceding_finger <id>
    if (strcmp(argv[1], "closest_preceding_finger") == 0) {
        int targetId = atoi(argv[2]);

        Node* result = closest_preceding_finger(node, targetId);

        if (result != NULL) {
            printf("%d %s\n", result->id, result->Ip);
        }
        return 0;
    }

    return 0;
}

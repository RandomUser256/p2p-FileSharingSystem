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

    // Gets succesor of local node and prints it
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

    // print_finger_table
    if (strcmp(argv[1], "print_finger_table") == 0) {
        printFingerTable(node);
        return 0;
    }

    // save_finger_table
    if (strcmp(argv[1], "save_finger_table") == 0) {
        saveFingerTableToFile(node, "nodeInfo/FingerTable");
        printf("Finger table saved successfully\n");
        return 0;
    }

    // load_finger_table
    if (strcmp(argv[1], "load_finger_table") == 0) {
        loadFingerTableFromFile(node, "nodeInfo/FingerTable");
        printf("Finger table loaded successfully\n");
        return 0;
    }

    // get_finger_entry <index>
    if (strcmp(argv[1], "get_finger_entry") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: get_finger_entry requires index argument\n");
            return 1;
        }
        
        int index = atoi(argv[2]);
        if (index < 0 || index >= NODE_ID_LENGTH) {
            fprintf(stderr, "Error: index out of range [0, %d)\n", NODE_ID_LENGTH);
            return 1;
        }

        FingerTableEntry* entry = &node->fingerTable[index];
        int succ_id = (entry->successor != NULL) ? entry->successor->id : -1;
        const char* succ_ip = (entry->Ip[0] != '\0') ? entry->Ip : "NONE";

        printf("%d %d %d %d %d %s\n",
               index,
               entry->start,
               entry->lowerIntervalLimit,
               entry->upperIntervalLimit,
               succ_id,
               succ_ip);
        return 0;
    }

    //Stabilize
    if (strcmp(argv[1], "stabilize") == 0) {
        if(argc < 3) {
            fprintf(stderr, "Error, not enough arguments in the call");
            return 1;
        }

        int targetId = atoi(argv[2]);

        Node* predecessor = remote_closest_preceding_finger(node->Ip, node->id);
        //Node* predecessor = node->predecessor;

        Node* successor = remote_find_successor(node->Ip, )

        if (in_open_interval(predecessor->id, node->id, node->successor->id)) {
            node->successor = predecessor;
        }

        //NOTIFY phase
        Node* succPred = find_predecessor(node, node->successor->id)


        if (predecessor == NULL || in_open_interval(node->id, predecessor->id, node->successor->id)) {
            predecessor = node;
        }

        return 0;
    }

    return 0;
}

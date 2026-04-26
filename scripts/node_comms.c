#include <stdio.h>
#include <stdlib.h> 
#include <string.h>

#include "../src/DHASH.h"
#include "../src/logger.h"

// Called from another node to read the contents of the node it's accessing (which is the local node running this script at a given time))
// Basically communication to traverse the ring

//Always provided an IP address of the node to query, and the ID of the target node for the query when applicable

/*
TODO
    - Check that remote join links predecessors and successors correctly
        - Maybe the predecessor of the local node is being incorrectly updated
*/

// Compiling commnad: gcc node_comms.c ../src/DHASH.c ../src/logger.c ../src/node.c -Wall -o node_comms -lm

int main(int argc, char* argv[]) {

    Node* node = loadNodeFromFile("../nodeInfo/Node");
    loadFingerTableFromFile(node, "../nodeInfo/FingerTable");

    if(node == NULL) {
        log_error("Failed to load local node from file\n");
        //fprintf(stderr, "Error, couldn't load node correctly\n");
        return 1;
    }

    if(argc < 2) {
        log_error("[ERROR] No command provided for node_comms\n");
        //fprintf(stderr, "Error, not enough arguments in the call");
        return 1;
    }

    //find_successor <id>
    if(strcmp(argv[1], "find_successor") == 0) {
        if(argc < 3) {
            log_error("[ERROR] Not enough arguments for find_successor\n");
            //fprintf(stderr, "Error, not enough arguments in the call");
            return 1;
        }

        int targetId = atoi(argv[2]);

        Node* result = find_successor(node, targetId);

        if(result != NULL) {
            printf("%d %s\n", result->id, result->Ip);
            return 1;
        } else {
            log_error("[ERROR] Failed to find successor for ID %d with node %d\n", targetId, node->id);
            //fprintf(stderr, "ERROR, process didn't execute fully or correctly\n");
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
        saveFingerTableToFile(node, "../nodeInfo/FingerTable");
        log_info("[INFO] Finger table saved successfully\n");
        return 0;
    }

    // load_finger_table
    if (strcmp(argv[1], "load_finger_table") == 0) {
        loadFingerTableFromFile(node, "../nodeInfo/FingerTable");
        log_info("[INFO] Finger table loaded successfully\n");
        return 0;
    }

    //check Chord ring integrity remotely
    //only checks local node's successor and predecessor links, but can be called on each node in the ring to verify overall integrity
    // Returns node's ID and IP if check passes, must be called recursively to check the whole ring
    if (strcmp(argv[1], "check_ring") == 0) {
        if (node->successor->predecessor != node) {
            log_error("ERROR: successor->predecessor mismatch at node %d\n", node->id);
            return -1;
        }
        if (node->predecessor->successor != node) {
            log_error("ERROR: predecessor->successor mismatch at node %d\n", node->id);
            return -1;
        }

        /*
        char result[50];
        snprintf(result, sizeof(result), "%d %s\n", node->id, node->Ip);
        */
        return node->id; // Return ID to indicate success
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

    // notify <predecessor_id> <predecessor_ip>
    // Updates local node's predecessor and saves to disk
    if (strcmp(argv[1], "notify") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: notify requires predecessor_id and predecessor_ip\n");
            return 1;
        }

        int pred_id = atoi(argv[2]);
        const char* pred_ip = argv[3];

        if (node->predecessor == NULL || 
            in_open_interval(pred_id, node->predecessor->id, node->id)) {
            
            // Update predecessor
            if (node->predecessor != NULL) {
                freeNode(node->predecessor);
            }
            node->predecessor = createNode(pred_id, pred_ip, "");
            
            // Save changes to disk
            saveNodeToFile(node, "../nodeInfo/Node");
            
            log_info("[INFO] Predecessor updated to Node %d (IP: %s)\n", pred_id, pred_ip);
        }
        return 0;
    }

    // stabilize - Performs stabilization on local node
    if (strcmp(argv[1], "stabilize") == 0) {
        if (!node->successor) {
            fprintf(stderr, "Error: Node has no successor\n");
            return 1;
        }

        // Get successor's predecessor
        Node* x = remote_get_successor(node->successor->Ip);
        if (x != NULL && in_open_interval(x->id, node->id, node->successor->id)) {
            node->successor = x;
            saveNodeToFile(node, "../nodeInfo/Node");
            log_info("[INFO] Updated successor to Node %d\n", x->id);
        }

        // Notify successor about this node
        log_info("[INFO] Notifying successor Node %d about Node %d\n", 
                  node->successor->id, node->id);
        
        if (x != NULL) {
            freeNode(x);
        }
        return 0;
    }

    //join - Remotely sync predecessor and successor links with another node in the ring, used when joining an existing ring
    //Meant to be sent from new node to an existing node
    if (strcmp(argv[1], "join") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: join requires target node IP\n");
            return 1;
        }

        //Consults the IP of the machine that wishes to join the ring, recreates the node structure of the target
        const char* target_ip = argv[2];
        Node* target_node = remote_get_successor(target_ip); // Get successor of target node to find correct position in ring

        if (target_node == NULL) {
            fprintf(stderr, "Error: Could not contact target node at %s\n", target_ip);
            return 1;
        }

        // Update local node's successor and predecessor based on target node's successor
        node->successor = createNode(target_node->id, target_node->Ip, "");
        node->predecessor = createNode(target_node->predecessor->id, target_node->predecessor->Ip, "");

        // Save changes to disk
        saveNodeToFile(node, "../nodeInfo/Node");

        log_info("[INFO] Joined ring via Node %d (IP: %s)\n", target_node->id, target_node->Ip);
        
        freeNode(target_node);
        return 0;
    }


    return 0;
}

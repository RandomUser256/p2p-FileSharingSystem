#include <math.h>
#include <time.h>
#include "src/DHASH.h"
#include "src/node.h"
#include "src/maintenance.h"
#include "src/logger.h"

//COMPILING COMMAND: gcc -pthread main.c src/node.c src/DHASH.c src/maintenance.c -o main -lm

/*
NOTE: Compile all source files together:
gcc -pthread main.c src/node.c src/DHASH.c src/maintenance.c src/logger.c -o main -lm

This compiles:
  - main.c (your program)
  - src/node.c (core Chord functions)
  - src/DHASH.c (DHT operations)
  - src/maintenance.c (background maintenance)
  
And links against libm (math library) with pthread support
*/

/*
TODO
    - Gets stuck on an infinite "Error, couldnt load node correctly"
*/

int main() {
    set_log_level(LOG_NONE); // Set to LOG_INFO or LOG_DEBUG for more detailed output

    char input[100];

    Node* localNode = loadNodeFromFile("nodeInfo/Node");

    if (localNode == NULL) {
        fprintf(stderr, "Error, couldn't load node correctly\n");
        
        printf("Enter IP of this node: ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            return 0; // Handle Ctrl+D (EOF)
        }

        char newIP[16];
        strncpy(newIP, input, sizeof(newIP) - 1);
        newIP[sizeof(newIP) - 1] = '\0'; // Read the IP address of an existing node in the network

        localNode = createNode(0, newIP, "shared/files");
    } else {
        printf("Node loaded successfully: ID=%d IP=%s\n", localNode->id, localNode->Ip);
        loadFingerTableFromFile(localNode, "nodeInfo/FingerTable");
    }

    

    MaintenanceThread* mt = start_maintenance_thread(localNode, 200, 8000); // Maintenance thread with 200ms stabilize interval and 8000ms fix fingers interval

    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Chord Node %d Started with Background Maintenance       ║\n", localNode->id);
    printf("║  stabilize: 200ms | fix_fingers: 8000ms                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    while (true) {
        // 1. Print a prompt
        printf("Enter command: join node to network [n] (only if new to the network), test chord ring structure [t], get current node information [g], toggle system logs [l], toggle error/warning logs [w], or exit [e]): ");
        
        // 2. Read input from user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // Handle Ctrl+D (EOF)
        }

        // Remove trailing newline character
        input[strcspn(input, "\r\n")] = '\0';

        // Process the command
        if (strcmp(input, "n") == 0) {
            // Join a new node to the network
            // Implementation for joining a new node would go here

            printf("Enter IP of existing node to join: ");

            if (fgets(input, sizeof(input), stdin) == NULL) {
                break; // Handle Ctrl+D (EOF)
            }

            char existingIp[16];
            strncpy(existingIp, input, sizeof(existingIp) - 1);
            existingIp[sizeof(existingIp) - 1] = '\0'; // Read the IP address of an existing node in the network

            remote_join(existingIp, localNode);
        }
        else if (strcmp(input, "t") == 0) {
            // Test chord ring structure
            remote_check_ring(localNode, localNode->successor->Ip);
        } else if (strcmp(input, "g") == 0) {
            // Get current node information
            nodePrint(localNode);
        } else if (strcmp(input, "e") == 0) {
            // Exit
            printf("Exiting...\n");
            freeNode(localNode);
            break;
        }  else if (strcmp(input, "l") == 0) {
            // Toggle system logs
            set_log_level(get_log_level() == LOG_NONE ? LOG_INFO : LOG_NONE);
            printf("System logs %s\n", get_log_level() == LOG_NONE ? "disabled" : "enabled");
        } else if (strcmp(input, "w") == 0) {
            // Toggle error/warning logs
            set_log_level(get_log_level() == LOG_NONE ? LOG_WARN : LOG_NONE);
            printf("Error/warning logs %s\n", get_log_level() == LOG_NONE ? "disabled" : "enabled");
        }
        else {
            printf("Invalid command. Please try again.\n");
        }
    }

    return 0;
}
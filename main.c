#include <pthread.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "src/DHASH.h"
#include "src/node.h"
#include "src/maintenance.h"
#include "src/logger.h"
#include "src/tcpServer.h"


/*
NOTE: Compile all source files together:

gcc main.c src/node.c src/DHASH.c src/maintenance.c src/logger.c src/tcpServer.c src/sha1.c -o main -pthread -lm

This compiles:
  - main.c (your program)
  - src/node.c (core Chord functions)
  - src/DHASH.c (DHT operations)
  - src/maintenance.c (background maintenance)
  
And links against libm (math library) with pthread support
*/

/*
TODO
    - Error when checking ring structure
        - In node.c: remote_get_successor() called within remote_stabilize() is not working correctly, causing the ring structure check to fail when it tries to get the successor of the predecessor of a node
        - In DHASH.c: remote_find_successor() is not working, 
            - the relative path in the remote ssh commands may be the problem
            - in the SSH command it does not specify which user to use, check version in remote_join() to change all other ssh commands
    - In node.c and DHASH.c, changed the ssh commmand section 'cd /home/mmagallanes' to accept arguments to change the user name depending on the machine
*/

static void list_shared_files(void) {
    DIR *dir = opendir("shared/files");
    if (!dir) {
        printf("  (could not open shared/files)\n\n");
        return;
    }
    printf("\n  Files available to insert:\n");
    printf("  ──────────────────────────\n");
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        printf("  • %s\n", entry->d_name);
        count++;
    }
    if (count == 0) printf("  (no files found)\n");
    printf("\n");
    closedir(dir);
}

static void list_ring_files(void) {
    FILE *f = fopen("shared/ChordRingFiles", "r");
    if (!f) {
        printf("  (could not open shared/ChordRingFiles)\n\n");
        return;
    }
    printf("\n  Files stored in the Chord ring:\n");
    printf("  ────────────────────────────────\n");
    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        char *comma = strchr(line, ',');
        if (comma) *comma = '\0';
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        printf("  • %s\n", line);
        count++;
    }
    if (count == 0) printf("  (no files found)\n");
    printf("\n");
    fclose(f);
}

int main() {
    set_log_level(LOG_NONE); // Set to LOG_INFO or LOG_DEBUG for more detailed output

    Node* localNode = loadNodeFromFile("nodeInfo/Node");

    char input[100];

    if (localNode == NULL) {
        printf("Error, couldn't load node correctly\n");
        //fprintf(stderr, "Error, couldn't load node correctly\n");
        
        printf("Enter IP of this node: ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            return 0; // Handle Ctrl+D (EOF)
        }
        input[strcspn(input, "\r\n")] = '\0';

        char newIP[16];
        strncpy(newIP, input, sizeof(newIP) - 1);
        newIP[sizeof(newIP) - 1] = '\0';

        localNode = createNode(0, newIP, "shared/files");
    } else {
        printf("Node loaded successfully: ID=%d IP=%s\n", localNode->id, localNode->Ip);
        loadFingerTableFromFile(localNode, "nodeInfo/FingerTable");
    }

    int defaultPort = 8080;
    if (defaultPort <= 0 || defaultPort > 65535) fatalError(NULL);
    t_server *serv = initServer(defaultPort, localNode);

    if (serv) {
        createSock(serv);
        configAddr(serv);
        bindAndListen(serv);
    }

    pthread_t server_tid;
    pthread_create(&server_tid, NULL, server_loop, serv);

    // Background service that maintains chord ring integrity
    MaintenanceThread* mt = start_maintenance_thread(serv, 200, 800); // Maintenance thread with 200ms stabilize interval and 8000ms fix fingers interval

    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Chord Node %d Started with Background Maintenance       ║\n", localNode->id);
    printf("║  stabilize: 200ms | fix_fingers: 8000ms                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    while (true) {
        // 1. Print a prompt
        printf("\n  ┌─────────────────────────────────────────┐\n");
        printf("  │            Chord Node Commands          │\n");
        printf("  ├─────────────────────────────────────────┤\n");
        printf("  │  n  — Join an existing network          │\n");
        printf("  │  f  — Find successor of an ID           │\n");
        printf("  │  p  — Find predecessor of this node     │\n");
        printf("  │  t  — Test ring structure               │\n");
        printf("  │  g  — Show this node's info             │\n");
        printf("  │  i  — Insert a file into the ring       │\n");
        printf("  │  r  — Retrieve a file from the ring     │\n");
        printf("  │  l  — Toggle info logs                  │\n");
        printf("  │  w  — Toggle warning logs               │\n");
        printf("  │  e  — Exit                              │\n");
        printf("  └─────────────────────────────────────────┘\n");
        printf("  > ");
        
        // 2. Read input from user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // Handle Ctrl+D (EOF)
        }

        // Remove trailing newline character
        input[strcspn(input, "\r\n")] = '\0';

        // Process the command
        if (strcmp(input, "n") == 0) {
            // Join a new node to the network
            printf("Enter IP of existing node to join: ");

            if (fgets(input, sizeof(input), stdin) == NULL) {
                break; // Handle Ctrl+D (EOF)
            }
            input[strcspn(input, "\r\n")] = '\0';

            char existingIp[16];
            strncpy(existingIp, input, sizeof(existingIp) - 1);
            existingIp[sizeof(existingIp) - 1] = '\0';

            printf("Joining node at %s...\n", existingIp);


            remote_join(existingIp, 8080, serv);  // Empty string for username (not used with TCP)
        }
        else if (strcmp(input, "f") == 0) {
            int targetId = 0;

            printf("Enter ID to find its successor node: ");

            if (scanf("%d", &targetId) != 1) {
                printf("Invalid input\n");

                while (getchar() != '\n' && getchar() != EOF);
                continue;
            }

            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            Node* succ = remote_find_successor(serv, serv->port, targetId);

            if (succ != NULL) {
                printf("Successor for node found, id: %d IP: %s \n", succ->id, succ->Ip);
            } else {
                printf("Problem occured with locating successor.\n");
            }
        }
        else if (strcmp(input, "p") == 0) {
            Node* pred = remote_find_predecessor(serv, serv->port, serv->localNode->id);

            if (pred != NULL) {
                printf("Predecessor for node found, id: %d IP: %s \n", pred->id, pred->Ip);
            } else {
                printf("Problem occured with locating predecessor. \n");
            }
        }
        else if (strcmp(input, "t") == 0) {
            // Test chord ring structure
            remote_check_ring(serv);
        } else if (strcmp(input, "g") == 0) {
            pthread_mutex_lock(&serv->lock);
            nodePrint(serv->localNode);
            fingerTablePrint(serv->localNode);
            pthread_mutex_unlock(&serv->lock);
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
        else if (strcmp(input, "i") == 0) {
            list_shared_files();
            char filename[256];
            printf("Enter filename to insert: ");

            if (fgets(filename, sizeof(filename), stdin)) {
                // fgets keeps the newline character '\n', so we usually strip it:
                filename[strcspn(filename, "\n")] = 0;
            }

            printf("Read file name\n");

            char filePath[512] = {0}; 
            snprintf(filePath, sizeof(filePath), "shared/files/%s", filename);

            insert_chunked(serv, filePath);
        }
        else if (strcmp(input, "r") == 0) {
            list_ring_files();
            char filename[256];
            printf("Enter filename to retrieve: ");

            if (fgets(filename, sizeof(filename), stdin)) {
                // fgets keeps the newline character '\n', so we usually strip it:
                filename[strcspn(filename, "\n")] = 0;
            }

            printf("Read file name: %s\n", filename);

            /*
            char filePath[512] = {0}; 
            snprintf(filePath, sizeof(filePath), "shared/files/%s", filename);
            */
            retrieve_file(serv, filename , "shared/files");
        }
        else {
            printf("Invalid command. Please try again.\n");
        }
    }

    
    return 0;
}
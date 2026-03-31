#include <stdio.h>
#include <math.h>
#include <time.h>

#include "src/DHASH.c"

    Node* localNode = loadNodeFromFile("nodeInfo/Node");

    if (localNode == NULL) {
        printf("Error: Could not load local node\n");
        return 1;
    }

    //Nodo 5 aka 10.11.20.41
    Node* remoteNode = createNode(5, "10.11.20.41", "shared/files");

    printf("\n--- Nodes ---\n");
    printf("Local Node -> ID: %d | IP: %s | PATH: %s\n",
           localNode->id, localNode->Ip, localNode->fileContentPath);

    printf("Remote Node -> ID: %d | IP: %s | PATH: %s\n",
           remoteNode->id, remoteNode->Ip, remoteNode->fileContentPath);

    // Manually connect them (simple ring of 2 nodes)
    localNode->successor = remoteNode;
    localNode->predecessor = remoteNode;

    remoteNode->successor = localNode;
    remoteNode->predecessor = localNode;

    printf("\n--- Testing SCP File Transfer ---\n");

    // TEST INSERT (this triggers SCP)
    insert(localNode,
           "experiment.txt",                    // filename
           "shared/files/experiment.txt",       // source file (LOCAL)
           5);                                  // destination node ID

    printf("\n=== TEST COMPLETE ===\n");

    // Cleanup
    freeNode(localNode);
    freeNode(remoteNode);
    return 0;
}

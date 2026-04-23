#include <math.h>
#include <time.h>
#include "../src/DHASH.c"

int main() {
    srand(time(NULL));

     srand(time(NULL));
    
    Node* nodeArray[16];

    nodeArray[0] = createNode(0, "", "shared/files");
    nodeArray[1] = createNode(1, "", "shared/files");
    nodeArray[2] = createNode(2, "", "shared/files");
    nodeArray[3] = createNode(3, "", "shared/files");
    nodeArray[4] = createNode(4, "", "shared/files");
    nodeArray[5] = createNode(5, "", "shared/files");
    nodeArray[6] = createNode(6, "", "shared/files");
    nodeArray[7] = createNode(7, "", "shared/files");
    nodeArray[8] = createNode(8, "", "shared/files");
    nodeArray[9] = createNode(9, "", "shared/files");
    nodeArray[10] = createNode(10, "", "shared/files");
    nodeArray[11] = createNode(11, "", "shared/files");
    nodeArray[12] = createNode(12, "", "shared/files");
    nodeArray[13] = createNode(13, "", "shared/files");
    nodeArray[14] = createNode(14, "", "shared/files");
    nodeArray[15] = createNode(15, "", "shared/files");

    join (nodeArray[0], NULL); // First node joins the network by itself

    join (nodeArray[0], nodeArray[1]);

    stabilize (nodeArray[0]);
    fix_fingers (nodeArray[0]);

    join (nodeArray[1], nodeArray[2]);

    join (nodeArray[2], nodeArray[3]);

    stabilize (nodeArray[1]);
    fix_fingers (nodeArray[1]);

    join (nodeArray[3], nodeArray[4]);

    
    stabilize (nodeArray[0]);
    fix_fingers (nodeArray[0]);

    join (nodeArray[4], nodeArray[5]);

    
    stabilize (nodeArray[3]);
    fix_fingers (nodeArray[3]);


    join(nodeArray[5], nodeArray[6]);

    stabilize(nodeArray[4]);
    fix_fingers(nodeArray[4]);

    join(nodeArray[6], nodeArray[7]);

    stabilize(nodeArray[5]);
    fix_fingers(nodeArray[5]);

    join(nodeArray[7], nodeArray[8]);

    stabilize(nodeArray[6]);
    fix_fingers(nodeArray[6]);

    join(nodeArray[8], nodeArray[9]);

    stabilize(nodeArray[7]);
    fix_fingers(nodeArray[7]);

    join(nodeArray[9], nodeArray[10]);

    stabilize(nodeArray[8]);
    fix_fingers(nodeArray[8]);

    join(nodeArray[10], nodeArray[11]);

    stabilize(nodeArray[9]);
    fix_fingers(nodeArray[9]);

    join(nodeArray[12], nodeArray[13]);

    stabilize(nodeArray[10]);
    fix_fingers(nodeArray[10]);

    join(nodeArray[13], nodeArray[14]);

    stabilize(nodeArray[11]);
    fix_fingers(nodeArray[11]);

    join(nodeArray[14], nodeArray[15]);

    stabilize(nodeArray[12]);
    fix_fingers(nodeArray[12]);

    join(nodeArray[15], nodeArray[16]);

    stabilize(nodeArray[0]);
    fix_fingers(nodeArray[0]);

    stabilize(nodeArray[1]);
    fix_fingers(nodeArray[1]);

    stabilize(nodeArray[2]);
    fix_fingers(nodeArray[2]);

    stabilize(nodeArray[3]);
    fix_fingers(nodeArray[3]);

    stabilize(nodeArray[4]);
    fix_fingers(nodeArray[4]);

    stabilize(nodeArray[5]);
    fix_fingers(nodeArray[5]);

    stabilize(nodeArray[6]);
    fix_fingers(nodeArray[6]);

    stabilize(nodeArray[7]);
    fix_fingers(nodeArray[7]);

    stabilize(nodeArray[8]);
    fix_fingers(nodeArray[8]);

    stabilize(nodeArray[11]);
    fix_fingers(nodeArray[11]);

    stabilize(nodeArray[12]);
    fix_fingers(nodeArray[12]);

    stabilize(nodeArray[13]);
    fix_fingers(nodeArray[13]);

    stabilize(nodeArray[14]);
    fix_fingers(nodeArray[14]);

    stabilize(nodeArray[15]);
    fix_fingers(nodeArray[15]);

    stabilize(nodeArray[16]);
    fix_fingers(nodeArray[16]);

     // Cleanup and print final state
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║    Final FingerTable State for All Nodes                     ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    for (int i = 0; i < 16; i++) {
        printf("\n--- Final State - Node %d ---\n", nodeArray[i]->id);
        printf("start: %d, lowerIntervalLimit: %d, upperIntervalLimit: %d\n",
               nodeArray[i]->fingerTable[0].start,
               nodeArray[i]->fingerTable[0].lowerIntervalLimit,
               nodeArray[i]->fingerTable[0].upperIntervalLimit);
        printf("Node %d successor: id=%d ip=%s\n", nodeArray[i]->id, nodeArray[i]->successor->id, nodeArray[i]->successor->Ip);
        printf("Node %d predecessor: id=%d ip=%s\n", nodeArray[i]->id, nodeArray[i]->predecessor->id, nodeArray[i]->predecessor->Ip);
    }
}
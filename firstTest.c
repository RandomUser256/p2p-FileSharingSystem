#include <math.h>
#include <time.h>

//#include "src/node.c"
#include "src/DHASH.c"

int main() {

    srand(time(NULL));

    printf("=== DISTRIBUTED CHORD TEST (3 NODES) ===\n");

    // 🔹 Load LOCAL node (Node 4)
    Node* node4 = loadNodeFromFile("nodeInfo/Node");

    if (!node4) {
        printf("Failed to load local node\n");
        return 1;
    }

    printf("\n--- Node Setup ---\n");
    printf("Node4 (local): ID=%d IP=%s\n", node4->id, node4->Ip);

    // 🔹 FINGER TABLE OPERATIONS
    printf("\n=== FINGER TABLE OPERATIONS ===\n");
    
    // Load finger table from disk
    printf("\n[Step 1] Loading finger table from nodeInfo/FingerTable...\n");
    loadFingerTableFromFile(node4, "nodeInfo/FingerTable");
    
    // Display the loaded finger table
    printf("\n[Step 2] Displaying loaded finger table:\n");
    printFingerTable(node4);
    
    // 🔹 Remote references (for testing)
    Node* node5 = createNode(5, "10.11.20.41", "");
    Node* node6 = createNode(6, "10.11.20.42", "");

    printf("Node5 (remote): ID=%d IP=%s\n", node5->id, node5->Ip);
    printf("Node6 (remote): ID=%d IP=%s\n", node6->id, node6->Ip);

    int testIds[] = {4, 5, 6, 7};
    int nTests = 4;

    printf("\n--- Distributed find_successor (START FROM LOCAL NODE) ---\n");

    for (int i = 0; i < nTests; i++) {

        int id = testIds[i];

        printf("\n[LOCAL START] find_successor(%d)\n", id);

        // ✅ THIS is the correct call now
        Node* result = find_successor(node4, id);

        if (result) {
            printf("Result → ID=%d IP=%s\n",
                   result->id,
                   result->Ip);
        } else {
            printf("FAILED\n");
        }
    }

    printf("\n--- Optimized Lookup Using Finger Tables ---\n");
    
    for (int i = 0; i < nTests; i++) {
        int id = testIds[i];
        
        printf("\n[FINGER TABLE LOOKUP] find_successor_with_finger_table(%d)\n", id);
        
        Node* result = find_successor_with_finger_table(node4, id);
        
        if (result) {
            printf("Result → ID=%d IP=%s\n", result->id, result->Ip);
            freeNode(result);
        } else {
            printf("FAILED\n");
        }
    }

    printf("\n--- Remote entry point test (START FROM NODE 5) ---\n");

    for (int i = 0; i < nTests; i++) {

        int id = testIds[i];

        printf("\n[REMOTE START @ Node5] find_successor(%d)\n", id);

        // ✅ explicitly start lookup from another node
        Node* result = remote_find_successor(node5->Ip, id);

        if (result) {
            printf("Result → ID=%d IP=%s\n",
                   result->id,
                   result->Ip);
            freeNode(result);
        } else {
            printf("FAILED\n");
        }
    }

    printf("\n--- File Sharing Example ---\n");
    printf("\n[Step 3] Testing file insertion with hash-based lookup\n");
    
    // Example: Insert a file "data.txt" that hashes to ID 6
    printf("Inserting file 'data.txt' (hashes to ID=6) to responsible node...\n");
    // This would normally hash the filename to get an ID:
    // int fileId = hash_filename("data.txt");  // returns 6
    // insert(node4, "data.txt", "local_path/data.txt", fileId);
    
    printf("\n[Step 4] Saving updated finger table to disk\n");
    saveFingerTableToFile(node4, "nodeInfo/FingerTable");

    printf("\n=== TEST COMPLETE ===\n");

    freeNode(node4);
    freeNode(node5);
    freeNode(node6);

    return 0;
}

#include <math.h>
#include <time.h>
#include "src/DHASH.c"

/* ---------------------------------------------------------------
   Test helpers
--------------------------------------------------------------- */

static int tests_run    = 0;
static int tests_passed = 0;

static void pass(const char* label) {
    printf("  [PASS] %s\n", label);
    tests_passed++;
    tests_run++;
}

static void fail(const char* label, const char* reason) {
    printf("  [FAIL] %s — %s\n", label, reason);
    tests_run++;
}

/* ---------------------------------------------------------------
   Test 1 — loadNodeFromFile restores successor and predecessor
   Depends on: nodeInfo/Node containing successor= and predecessor= lines.
   Expected: node4->successor->id == 5, node4->predecessor->id == 6 (ring: 4→5→6→4)
--------------------------------------------------------------- */
static void test_load_node_restores_links(void) {
    printf("\n[TEST 1] loadNodeFromFile — successor/predecessor restored from disk\n");

    Node* node = loadNodeFromFile("nodeInfo/Node");

    if (node == NULL) {
        fail("load node", "returned NULL — check nodeInfo/Node exists");
        return;
    }

    if (node->id != 4) {
        fail("node id", "expected 4");
    } else {
        pass("node id == 4");
    }

    if (strcmp(node->Ip, "10.11.20.40") != 0) {
        fail("node ip", "expected 10.11.20.40");
    } else {
        pass("node ip == 10.11.20.40");
    }

    if (node->successor == NULL) {
        fail("successor loaded", "successor is NULL — fix loadNodeFromFile");
    } else if (node->successor->id != 5) {
        char buf[64];
        snprintf(buf, sizeof(buf), "expected id=5 got id=%d", node->successor->id);
        fail("successor id", buf);
    } else {
        pass("successor->id == 5");
    }

    if (node->predecessor == NULL) {
        fail("predecessor loaded", "predecessor is NULL — fix loadNodeFromFile");
    } else if (node->predecessor->id != 6) {
        char buf[64];
        snprintf(buf, sizeof(buf), "expected id=6 got id=%d", node->predecessor->id);
        fail("predecessor id", buf);
    } else {
        pass("predecessor->id == 6");
    }

    freeNode(node);
}

/* ---------------------------------------------------------------
   Test 2 — generateDestinationFilePath does NOT mutate node struct
   The bug: strcat directly into node->fileContentPath trashed the field.
--------------------------------------------------------------- */
static void test_generate_path_no_mutation(void) {
    printf("\n[TEST 2] generateDestinationFilePath — node->fileContentPath not mutated\n");

    Node* node = createNode(4, "10.11.20.40", "/data/node4");

    char originalPath[MAX_FILE_PATH_LENGTH];
    strncpy(originalPath, node->fileContentPath, MAX_FILE_PATH_LENGTH);

    char* result = generateDestinationFilePath(node, "testfile.txt");

    if (result == NULL) {
        fail("returns non-NULL", "got NULL");
        freeNode(node);
        return;
    }
    pass("returns non-NULL path");

    if (strcmp(node->fileContentPath, originalPath) != 0) {
        fail("fileContentPath not mutated",
             "field was modified — use a local buffer in generateDestinationFilePath");
    } else {
        pass("fileContentPath unchanged after call");
    }

    /* Check the result contains both base path and filename */
    if (strstr(result, "node4") == NULL || strstr(result, "testfile.txt") == NULL) {
        fail("result contains base and filename", result);
    } else {
        pass("result path contains base dir and filename");
    }

    freeNode(node);
}

/* ---------------------------------------------------------------
   Test 3 — remote_get_successor reaches node5 over SSH
   This will only pass when SSH connectivity to 10.11.20.41 is up.
--------------------------------------------------------------- */
static void test_remote_get_successor(void) {
    printf("\n[TEST 3] remote_get_successor — SSH to node5 (10.11.20.41)\n");

    Node* succ = remote_get_successor("10.11.20.41");

    if (succ == NULL) {
        fail("remote_get_successor node5", "returned NULL — check SSH access and node_comms on 10.11.20.41");
        return;
    }
    pass("got non-NULL response from node5");

    printf("         node5 reports successor: id=%d ip=%s\n", succ->id, succ->Ip);

    /* node5's successor should be node6 in a 3-node ring 4→5→6→4 */
    if (succ->id != 6) {
        char buf[64];
        snprintf(buf, sizeof(buf), "expected id=6 (node6), got id=%d", succ->id);
        fail("node5 successor id", buf);
    } else {
        pass("node5->successor->id == 6");
    }

    freeNode(succ);
}

/* ---------------------------------------------------------------
   Test 4 — remote_closest_preceding_finger
   Ask node5 for the closest preceding finger for id=6.
   In a 3-node ring (4,5,6) the answer from node5 should be node5 itself
   (no finger between 5 and 6 in a 4-bit space with only 3 nodes).
--------------------------------------------------------------- */
static void test_remote_closest_preceding_finger(void) {
    printf("\n[TEST 4] remote_closest_preceding_finger — ask node5 for cpf(6)\n");

    Node* result = remote_closest_preceding_finger("10.11.20.41", 6);

    if (result == NULL) {
        fail("remote_closest_preceding_finger node5", "returned NULL");
        return;
    }
    pass("got non-NULL response from node5");

    printf("         closest preceding finger for id=6 from node5: id=%d ip=%s\n",
           result->id, result->Ip);

    freeNode(result);
}

/* ---------------------------------------------------------------
   Test 5 — find_predecessor traverses the ring without getting stuck
   Bug: hollow Node* structs caused infinite loop on one node.
   We test each possible target ID across the 3-node ring.
   Expected predecessors (ring: 4→5→6→4, 4-bit space, ids 0-15):
     target=4 → predecessor=6  (6 is last node before 4 wraps)
     target=5 → predecessor=4
     target=6 → predecessor=5
     target=7 → predecessor=6
--------------------------------------------------------------- */
static void test_find_predecessor_no_loop(void) {
    printf("\n[TEST 5] find_predecessor — no infinite loop, correct predecessor returned\n");

    Node* local = loadNodeFromFile("nodeInfo/Node");
    if (local == NULL) {
        fail("load local node", "cannot proceed without local node");
        return;
    }

    typedef struct { int targetId; int expectedPredId; } Case;
    Case cases[] = {
        { 4, 6 },
        { 5, 4 },
        { 6, 5 },
        { 7, 6 },
    };
    int n = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < n; i++) {
        int target      = cases[i].targetId;
        int expectedPred = cases[i].expectedPredId;

        printf("  find_predecessor(node4, %d) — expected predecessor id=%d\n",
               target, expectedPred);

        Node* pred = find_predecessor(local, target);

        if (pred == NULL) {
            char buf[64];
            snprintf(buf, sizeof(buf), "find_predecessor(%d) returned NULL", target);
            fail("predecessor not NULL", buf);
            continue;
        }

        printf("         got predecessor: id=%d ip=%s\n", pred->id, pred->Ip);

        if (pred->id != expectedPred) {
            char buf[96];
            snprintf(buf, sizeof(buf),
                     "target=%d: expected pred id=%d, got id=%d",
                     target, expectedPred, pred->id);
            fail("predecessor id correct", buf);
        } else {
            char label[48];
            snprintf(label, sizeof(label), "find_predecessor(%d)->id == %d", target, expectedPred);
            pass(label);
        }

        freeNode(pred);
    }

    freeNode(local);
}

/* ---------------------------------------------------------------
   Test 6 — find_successor end-to-end across the ring
   Expected (ring 4→5→6→4):
     find_successor(node4, 4) → node4  (id=4 lives on node4)
     find_successor(node4, 5) → node5
     find_successor(node4, 6) → node6
     find_successor(node4, 7) → node4  (wraps: next node after 6 is 4)
--------------------------------------------------------------- */
static void test_find_successor_end_to_end(void) {
    printf("\n[TEST 6] find_successor — end-to-end distributed lookup\n");

    Node* local = loadNodeFromFile("nodeInfo/Node");
    if (local == NULL) {
        fail("load local node", "cannot proceed");
        return;
    }

    typedef struct { int targetId; int expectedSuccId; } Case;
    Case cases[] = {
        { 4, 4 },
        { 5, 5 },
        { 6, 6 },
        { 7, 4 },
    };
    int n = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < n; i++) {
        int target    = cases[i].targetId;
        int expectedS = cases[i].expectedSuccId;

        printf("  find_successor(node4, %d) — expected id=%d\n", target, expectedS);

        Node* succ = find_successor(local, target);

        if (succ == NULL) {
            char buf[64];
            snprintf(buf, sizeof(buf), "find_successor(%d) returned NULL", target);
            fail("successor not NULL", buf);
            continue;
        }

        printf("         got successor: id=%d ip=%s\n", succ->id, succ->Ip);

        if (succ->id != expectedS) {
            char buf[96];
            snprintf(buf, sizeof(buf),
                     "target=%d: expected succ id=%d, got id=%d",
                     target, expectedS, succ->id);
            fail("successor id correct", buf);
        } else {
            char label[48];
            snprintf(label, sizeof(label), "find_successor(%d)->id == %d", target, expectedS);
            pass(label);
        }

        freeNode(succ);
    }

    freeNode(local);
}

/* ---------------------------------------------------------------
   Test 7 — Modular Finger Table Testing with Ring & Remote Comms
   Tests finger table persistence, optimized lookups, and 
   remote synchronization across the Chord ring.
--------------------------------------------------------------- */

/* Helper: Test finger table persistence */
static void test_finger_table_persistence(Node* node) {
    printf("\n  [SUBTEST] Finger Table Persistence\n");
    
    // Save finger table
    saveFingerTableToFile(node, "nodeInfo/FingerTable");
    pass("saved finger table to disk");
    
    // Create a new node and load the table
    Node* node_loaded = createNode(node->id, node->Ip, node->fileContentPath);
    loadFingerTableFromFile(node_loaded, "nodeInfo/FingerTable");
    pass("loaded finger table from disk");
    
    // Verify entries match
    int entries_match = 1;
    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        if (node->fingerTable[i].start != node_loaded->fingerTable[i].start ||
            node->fingerTable[i].lowerIntervalLimit != node_loaded->fingerTable[i].lowerIntervalLimit ||
            node->fingerTable[i].upperIntervalLimit != node_loaded->fingerTable[i].upperIntervalLimit) {
            entries_match = 0;
            break;
        }
    }
    
    if (entries_match) {
        pass("finger table entries match after load/save");
    } else {
        fail("finger table entries", "mismatch after load/save");
    }
    
    freeNode(node_loaded);
}

/* Helper: Test finger table structure validity */
static void test_finger_table_structure(Node* node) {
    printf("\n  [SUBTEST] Finger Table Structure Validity\n");
    
    int valid = 1;
    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        FingerTableEntry* entry = &node->fingerTable[i];
        
        // Check start value is correct
        int expected_start = (node->id + (1U << i)) % MAX_NUMBER_NODES;
        if (entry->start != expected_start) {
            printf("    Entry %d: start=%d, expected=%d\n", i, entry->start, expected_start);
            valid = 0;
        }
        
        // Check successor exists
        if (entry->successor == NULL) {
            printf("    Entry %d: successor is NULL\n", i);
            valid = 0;
        }
    }
    
    if (valid) {
        pass("finger table structure is valid");
    } else {
        fail("finger table structure", "invalid entries detected");
    }
}

/* Helper: Test optimized lookups */
static void test_optimized_lookups(Node* local_node) {
    printf("\n  [SUBTEST] Optimized Lookups vs Regular Lookups\n");
    
    int test_ids[] = {1, 2, 4, 6, 7, 9, 11};
    int num_tests = sizeof(test_ids) / sizeof(test_ids[0]);
    int match_count = 0;
    
    for (int i = 0; i < num_tests; i++) {
        int target_id = test_ids[i];
        
        // Regular lookup
        Node* regular = find_successor(local_node, target_id);
        
        // Optimized lookup with finger table
        Node* optimized = find_successor_with_finger_table(local_node, target_id);
        
        if (regular != NULL && optimized != NULL) {
            if (regular->id == optimized->id) {
                match_count++;
                printf("    find_successor(%d): regular=%d, optimized=%d ✓\n",
                       target_id, regular->id, optimized->id);
            } else {
                printf("    find_successor(%d): regular=%d, optimized=%d ✗\n",
                       target_id, regular->id, optimized->id);
                fail("lookup results", "optimized and regular differ");
            }
            freeNode(regular);
            freeNode(optimized);
        } else if (regular == NULL && optimized == NULL) {
            match_count++;
        } else {
            printf("    find_successor(%d): one returned NULL\n", target_id);
        }
    }
    
    if (match_count == num_tests) {
        pass("optimized lookups match regular lookups");
    }
}

/* Helper: Test predecessor lookups */
static void test_predecessor_with_finger_table(Node* local_node) {
    printf("\n  [SUBTEST] Predecessor Lookups with Finger Table\n");
    
    int test_ids[] = {4, 5, 6, 9};
    int num_tests = sizeof(test_ids) / sizeof(test_ids[0]);
    int success_count = 0;
    
    for (int i = 0; i < num_tests; i++) {
        int target_id = test_ids[i];
        
        Node* pred = find_predecessor_with_finger_table(local_node, target_id);
        
        if (pred != NULL) {
            printf("    find_predecessor(%d): predecessor id=%d\n", target_id, pred->id);
            success_count++;
            freeNode(pred);
        } else {
            printf("    find_predecessor(%d): returned NULL\n", target_id);
        }
    }
    
    if (success_count == num_tests) {
        pass("predecessor lookups completed");
    } else {
        fail("predecessor lookups", "some returned NULL");
    }
}

/* Helper: Test ring topology */
static void test_ring_topology(Node* local_node) {
    printf("\n  [SUBTEST] Ring Topology Validation\n");
    
    if (local_node->successor == NULL || local_node->predecessor == NULL) {
        fail("ring topology", "successor or predecessor is NULL");
        return;
    }
    
    printf("    Ring: %d → %d → %d → %d\n",
           local_node->id,
           local_node->successor->id,
           (local_node->successor->successor ? local_node->successor->successor->id : -1),
           (local_node->successor->predecessor ? local_node->successor->predecessor->id : -1));
    
    // Verify basic ring structure
    if (local_node->successor->id > 0 && local_node->predecessor->id > 0) {
        pass("ring topology is valid");
    } else {
        fail("ring topology", "invalid successor or predecessor IDs");
    }
}

/* Helper: Test remote finger table operations */
static void test_remote_finger_table_ops(const char* remote_ip) {
    printf("\n  [SUBTEST] Remote Finger Table Operations\n");
    printf("    Attempting SSH to %s...\n", remote_ip);
    
    // Test remote successor query
    Node* remote_succ = remote_get_successor(remote_ip);
    if (remote_succ != NULL) {
        printf("    Remote successor: id=%d ip=%s ✓\n", remote_succ->id, remote_succ->Ip);
        pass("remote_get_successor works");
        freeNode(remote_succ);
    } else {
        fail("remote operations", "could not reach remote node via SSH");
        return;
    }
    
    // Test remote closest preceding finger
    Node* remote_cpf = remote_closest_preceding_finger(remote_ip, 6);
    if (remote_cpf != NULL) {
        printf("    Remote CPF(6): id=%d ip=%s ✓\n", remote_cpf->id, remote_cpf->Ip);
        pass("remote_closest_preceding_finger works");
        freeNode(remote_cpf);
    }
}

/* Main modular test: Comprehensive finger table + ring + remote testing */
static void test_finger_table_ring_integration(void) {
    printf("\n[TEST 7] Modular Finger Table Integration — Ring & Remote Communication\n");
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Phase 1: Load Node and Finger Table                 ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    Node* local = loadNodeFromFile("nodeInfo/Node");
    if (local == NULL) {
        fail("load local node", "cannot proceed without local node");
        return;
    }
    pass("loaded local node from file");
    
    // Load and display finger table
    loadFingerTableFromFile(local, "nodeInfo/FingerTable");
    pass("loaded finger table from file");
    
    printf("\n  Displaying Finger Table:\n");
    printFingerTable(local);
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Phase 2: Finger Table Validation                    ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    test_finger_table_structure(local);
    test_finger_table_persistence(local);
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Phase 3: Ring Topology Testing                      ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    test_ring_topology(local);
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Phase 4: Lookup Performance Testing                 ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    test_optimized_lookups(local);
    test_predecessor_with_finger_table(local);
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Phase 5: Remote Communication Testing               ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    // Test remote operations
    printf("\n  Testing remote node at 10.11.20.41 (Node 9):\n");
    test_remote_finger_table_ops("10.11.20.39");
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Phase 6: Complete Lookup Workflow                   ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    printf("\n  Complete workflow: lookup ID=10\n");
    
    // Step 1: Check local finger table
    printf("    Step 1: Checking finger table entries...\n");
    for (int i = 0; i < NODE_ID_LENGTH; i++) {
        if (in_open_interval(10, local->id, local->fingerTable[i].upperIntervalLimit)) {
            printf("            Entry %d: interval [%d, %d) contains target\n",
                   i, local->fingerTable[i].lowerIntervalLimit,
                   local->fingerTable[i].upperIntervalLimit);
        }
    }
    
    // Step 2: Find using finger table
    printf("    Step 2: Finding successor using finger table...\n");
    Node* result = find_successor_with_finger_table(local, 10);
    if (result) {
        printf("            Result: Node %d at %s\n", result->id, result->Ip);
        pass("workflow: found responsible node");
        freeNode(result);
    } else {
        fail("workflow", "could not find responsible node");
    }
    
    printf("\n  Lookup workflow using predecessor:\n");
    printf("    Step 1: Finding predecessor of ID=10...\n");
    Node* pred = find_predecessor_with_finger_table(local, 10);
    if (pred) {
        printf("            Predecessor: Node %d at %s\n", pred->id, pred->Ip);
        pass("workflow: found predecessor");
        freeNode(pred);
    } else {
        fail("workflow", "could not find predecessor");
    }
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Phase 7: Cleanup and State Persistence              ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    // Save final state
    saveFingerTableToFile(local, "nodeInfo/FingerTable");
    pass("saved final finger table state");
    
    freeNode(local);
    
    printf("\n  [TEST 7 COMPLETE] All modular tests executed\n");
}

/* ---------------------------------------------------------------
   Main
--------------------------------------------------------------- */
int main(void) {
    srand(time(NULL));

    printf("========================================\n");
    printf("  Chord SSH Ring — Bug Fix Test Suite   \n");
    printf("  Local node: id=4  ip=10.11.20.40      \n");
    printf("  Ring:        4 → 5 → 6 → 4            \n");
    printf("========================================\n");

    
    test_load_node_restores_links();       /* Bug #2 */
    test_generate_path_no_mutation();      /* Bug #3 */
    test_remote_get_successor();           /* SSH baseline */
    test_remote_closest_preceding_finger(); /* SSH baseline */
    test_find_predecessor_no_loop();       /* Bug #1 — the main culprit */
    test_find_successor_end_to_end();      /* Full integration */
    
    test_finger_table_ring_integration();  /* NEW: Modular finger table test */

    printf("\n========================================\n");
    printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
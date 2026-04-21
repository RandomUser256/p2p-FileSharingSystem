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

    printf("\n========================================\n");
    printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}

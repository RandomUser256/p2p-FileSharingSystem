#include <stdio.h>
#include <math.h>
#include <time.h>

#include "src/node.c"

/*
ERRORS
    - 

CONSIDERATIONS
    - Do not call join(lastNode, firstNode), it causes first node's successor to loop back to itself, breaking the ring 
*/

int main() {
    srand(time(NULL));
    
    Node* node1 = createNode(0, "");
    Node* node2 = createNode(1, "");
    Node* node3 = createNode(3, "");
    Node* node4 = createNode(6, "");
    Node* node5 = createNode(7, "");
    Node* node6 = createNode(11, "");
    Node* node7 = createNode(14, "");

    join(node1, NULL); // First node joins the network by itself

    join(node1, node2);

    stabilize(node1);
    fix_fingers(node1);

    join(node2, node3);

    join(node3, node4);

    stabilize(node2);
    fix_fingers(node2);

    join(node4, node5);

    
    stabilize(node1);
    fix_fingers(node1);

    join(node5, node6);

    
    stabilize(node3);
    fix_fingers(node3);


    join(node6, node7);

    //join(node7, node1); // Node 1 joins again, should not cause any issues

    stabilize(node1);
    
    stabilize(node2);

    stabilize(node3);
    
    stabilize(node4);
    
    stabilize(node5);

    stabilize(node6);

    stabilize(node7);

    fix_fingers(node1);
    fix_fingers(node2);
    fix_fingers(node3);
    fix_fingers(node4);
    fix_fingers(node5);
    fix_fingers(node6);
    fix_fingers(node7);

    //printNodeList(node1);

    check_ring(node1);

    Node* tempNode;

    tempNode = find_successor(node5, 4);
    printf("Node 5 finds successor of 2: %d\n", tempNode->id);

    tempNode = find_successor(node7, 0);
    printf("Node 6 finds successor of 0: %d\n", tempNode->id);

    tempNode = find_successor(node1, 7);
    printf("Node 2 finds successor of 7: %d\n", tempNode->id);

    tempNode = find_successor(node3, 13);
    printf("Node 3 finds successor of 13: %d\n", tempNode->id);

    //Free memory of dynamically allocated nodes
    Node* current = node1;
    while (current != NULL) {
        Node* next = current->successor;
        freeNode(current);
        current = next;
    }

    return 0;
}
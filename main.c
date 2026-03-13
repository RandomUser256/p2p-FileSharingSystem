#include <stdio.h>
#include <math.h>
#include <time.h>

#include "src/node.c"

/*
ERRORS
    - Program currently experiences a segmentation fault, which coudl be caused by trying to dereference a NULL pointer value. Or can also be by asigning a value to a unitialized pointer.
        - Add protection to null values
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

    join(node1, node2);
    join(node2, node3);

    stabilize(node2);
    fix_fingers(node1);

    join(node3, node4);

    stabilize(node3);
    fix_fingers(node2);

    join(node4, node5);

    stabilize(node4);
    fix_fingers(node3);

    join(node5, node6);

    stabilize(node5);
    fix_fingers(node4);

    join(node6, node7);

    stabilize(node6);
    fix_fingers(node5);

    Node* tempNode;

    tempNode = find_successor(node1, 2);
    printf("Node 1 finds successor of 2: %d\n", tempNode->id);

    tempNode = find_successor(node3, 4);
    printf("Node 1 finds successor of 4: %d\n", tempNode->id);

    tempNode = find_successor(node4, 5);
    printf("Node 4 finds successor of 5: %d\n", tempNode->id);

    //Free memory of dynamically allocated nodes
    Node* current = node1;
    while (current != NULL) {
        Node* next = current->successor;
        freeNode(current);
        current = next;
    }

    return 0;
}
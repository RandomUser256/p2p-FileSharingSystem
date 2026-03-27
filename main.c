#include <stdio.h>
#include <math.h>
#include <time.h>

//#include "src/node.c"
#include "src/DHASH.c"

/*
ERRORS
    - Program currently experiences a segmentation fault, which coudl be caused by trying to dereference a NULL pointer value. Or can also be by asigning a value to a unitialized pointer.
        - Add protection to null values
*/

int main() {
    srand(time(NULL));
    
    Node* node1 = createNode(0, "", "fileSharingTesting/dir1");
    Node* node2 = createNode(1, "", "fileSharingTesting/dir2");
    Node* node3 = createNode(3, "", "fileSharingTesting/dir3");
    Node* node4 = createNode(6, "", "fileSharingTesting/dir1");
    Node* node5 = createNode(7, "", "fileSharingTesting/dir2");
    Node* node6 = createNode(11, "", "fileSharingTesting/dir3");
    Node* node7 = createNode(14, "", "fileSharingTesting/dir1");

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

    tempNode = find_successor(node3, 12);
    printf("Node 6 finds successor of 6: %d\n", tempNode->id);

    tempNode = find_successor(node2, 7);
    printf("Node 2 finds successor of 7: %d\n", tempNode->id);

    tempNode = find_successor(node3, 13);
    printf("Node 3 finds successor of 13: %d\n", tempNode->id);


    insert(node1, "file1", "fileSharingTesting/dir1/file1", 3);

    //Free memory of dynamically allocated nodes (handle circular ring safely)
    if (node1 != NULL) {
        Node* current = node1;
        Node* next;

        do {
            next = current->successor;
            freeNode(current);
            current = next;
        } while (current != node1);
    }

    return 0;
}
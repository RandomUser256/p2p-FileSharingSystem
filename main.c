#include <stdio.h>

#include "src/node.c"

int main() {
    Node node1 = createNode(0, "");
    Node node2 = createNode(1, "");
    Node node3 = createNode(3, "");
    Node node4 = createNode(6, "");
    Node node5 = createNode(7, "");
    Node node6 = createNode(11, "");
    Node node7 = createNode(14, "");

    join(&node1, &node2);
    join(&node1, &node3);
    join(&node1, &node4);
    join(&node1, &node5);
    join(&node1, &node6);
    join(&node1, &node7);

    Node* tempNode;

    tempNode = find_successor(&node1, 2);
    printf("Node 1 finds successor of 2: %d\n", tempNode->id);

    tempNode = find_successor(&node1, 4);
    printf("Node 1 finds successor of 4: %d\n", tempNode->id);

    tempNode = find_successor(&node4, 5);
    printf("Node 4 finds successor of 5: %d\n", tempNode->id);

    return 0;
}
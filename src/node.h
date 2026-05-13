#ifndef NODE_H
#define NODE_H

#include <stdbool.h>

//#include "tcpServer.h"
typedef struct s_server t_server;

#define MAX_IP_LENGTH 16
#define MAX_FILE_PATH_LENGTH 256
#define NODE_ID_LENGTH 4 //in bits, equal to 16 total nodes
#define MAX_NUMBER_NODES (1U << NODE_ID_LENGTH) //Maximum number of nodes in the network

int in_open_interval(int id, int start, int end);
int half_left_open_interval(int id, int start, int end);
int half_right_open_interval(int id, int start, int end);

struct Node;
typedef struct Node Node;

typedef struct FingerTableEntry {
    int start;
    //Node* parent_node;
    int lowerIntervalLimit;
    int upperIntervalLimit;
    Node* successor;
    char Ip[MAX_IP_LENGTH];
} FingerTableEntry;

typedef struct Node {
    int id;
    char Ip[MAX_IP_LENGTH];
    Node* successor;
    Node* predecessor;

    char fileContentPath[MAX_FILE_PATH_LENGTH];  

    //Finger table
    struct FingerTableEntry fingerTable[NODE_ID_LENGTH];
} Node;

bool nullCheckNode(Node* node);
bool nullCheckFingerTable(FingerTableEntry* entry);
FingerTableEntry* createFingerTableEntry(int entryNumber, Node* parent_node, Node* successor);
void updateValuesFingerTable(Node* node);

Node* createNode(int id, const char* ip, const char* fileContentPath);
void saveFingerTableToFile(Node* node, const char* filepath);
void saveNodeToFile(Node* node, const char* filepath);
Node* loadNodeFromFile(const char* filepath);

Node* createNode(int id, const char* ip, const char* fileContentPath);
Node* copyNode(Node* node);
Node* closest_preceding_finger(Node* node, int targetId);

Node* find_predecessor(Node* startNode, int id);
Node* find_successor(Node* node, int id);

Node* init_finger_table(Node* existingNode, Node* newNode);
void update_finger_table(Node* existingNode, Node* newNode, int tableEntryNumber);
void update_others(Node* currentNode);

void join(Node* existingNode, Node* newNode);
void fix_fingers(Node* node);
void notify(Node* node, Node* potentialPredecessor);
void stabilize(Node* node);
void freeNode(Node* node);

void nodePrint(Node* node);
void fingerTablePrint(Node* node);
void printNodeList(Node* head);
void check_ring(Node* start);

void executeSSH(const char* ip, const char* command);

void saveFingerTableToFile(Node* node, const char* filepath);
void loadFingerTableFromFile(Node* node, const char* filepath);

/*
void printFingerTable(Node* node);
void remote_print_finger_table(const char* ip);
void remote_load_and_update_finger_table(Node* node, const char* remote_ip);
Node* find_successor_with_finger_table(Node* node, int id);
Node* find_predecessor_with_finger_table(Node* node, int id);
*/
int init_socket(const char* ip, int port);
Node* remote_get_node(t_server* s, int port, const char* ip);
Node* finger_table_fallback(t_server* s, int port);
Node* remote_find_successor(t_server* s, int port, int targetId);
Node* remote_find_predecessor(t_server* s, int port, int targetId);

//void remote_notify(const char* remote_ip, Node* potentialPredecessor);
void remote_fix_fingers(t_server* s);
void remote_join(const char* existingIp, int port, t_server* s);
void remote_notify(t_server* s, int port, const char* existingIp);
void remote_stabilize(t_server* s, int port);
void remote_check_ring(t_server* s);

#endif
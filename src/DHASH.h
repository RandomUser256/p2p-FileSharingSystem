#ifndef DHASH_H
#define DHASH_H

#include <stdio.h>
#include <stdbool.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "node.h"

//typedef struct Node Node;

Node* lookup(Node* node, int identifier);
char* generateDestinationFilePath(Node* destinationNode, char* identifier);
void insert(Node* hostNode, char* identifier, char* sourceFilePath, int destination);

Node* remote_find_successor(const char* ip, int targetId);
Node* remote_get_successor(const char* ip);
Node* remote_closest_preceding_finger(const char* ip, int targetId);

#endif
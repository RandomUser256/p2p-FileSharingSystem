#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <pthread.h>
#include <time.h>
#include "node.h"

typedef struct {
    Node* node;
    int stabilize_interval_ms;    // milliseconds between stabilize calls
    int fix_fingers_interval_ms;  // milliseconds between fix_fingers calls
    volatile int running;         // flag to stop maintenance thread
} MaintenanceThread;

void* maintanance_worker(void* arg);

// Start background maintenance thread
MaintenanceThread* start_maintenance_thread(Node* node, 
                                            int stabilize_interval_ms,
                                            int fix_fingers_interval_ms);

// Stop background maintenance thread
void stop_maintenance_thread(MaintenanceThread* mt);

// Manual maintenance operations
void perform_stabilization_cycle(Node* node);
void perform_finger_fix_cycle(Node* node);

#endif
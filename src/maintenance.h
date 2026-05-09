#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

typedef struct s_server t_server; 

/*
Class meant to execute stabilize and fix_fingers functionality from chord network.
Runs in a background thread, each action is execute every time in a given time interval
*/

typedef struct {
    //Node* node;
    t_server* localServer;
    int stabilize_interval_ms;    // milliseconds between stabilize calls
    int fix_fingers_interval_ms;  // milliseconds between fix_fingers calls
    _Atomic int running;
    pthread_t thread_id;
} MaintenanceThread;

void* maintanance_worker(void* arg);

// Start background maintenance thread
MaintenanceThread* start_maintenance_thread(t_server* s, 
                                            int stabilize_interval_ms,
                                            int fix_fingers_interval_ms);

// Stop background maintenance thread
void stop_maintenance_thread(MaintenanceThread* mt);

// Manual maintenance operations
/*
void perform_stabilization_cycle(Node* node);
void perform_finger_fix_cycle(Node* node);
*/
#endif
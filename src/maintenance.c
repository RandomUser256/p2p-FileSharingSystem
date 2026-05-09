#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger.h"
#include "maintenance.h"
#include "tcpServer.h"

#include "node.h"

double get_monotonic_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

//Function that loops in the background
//Checks for time intervals to routinely execute remote_stabilize() and fix_fingers()
void* maintanance_worker(void* arg) {
    MaintenanceThread* mt = (MaintenanceThread*)arg;
    double last_stabilized_time = get_monotonic_seconds();
    double last_fixed_time = get_monotonic_seconds();

    log_info("[MAINTENANCE] Background thread started for Node %d\n", mt->localServer->localNode->id);

    while (mt->running) {
        //time_t now = clock_gettime(CLOCK_MONOTONIC);
        double now = get_monotonic_seconds();

        //Checks time intervals to execute stabilization
        if ((now - last_stabilized_time) >= mt->stabilize_interval_ms / 1000.0) {
            log_info("[MAINTENANCE] Starting stabilization for Node %d\n", mt->localServer->localNode->id);

            remote_stabilize(mt->localServer, mt->localServer->port);

            last_stabilized_time = now;

            log_info("[MAINTENANCE] Stabilization completed for Node %d\n", mt->localServer->localNode->id);
        }

        //Checks time intervals to execute fix_fingers()
        if ((now - last_fixed_time) >= mt->fix_fingers_interval_ms / 1000.0) {
            log_info("[MAINTENANCE] Starting fix_fingers for Node %d\n", mt->localServer->localNode->id);

            /*
            pthread_mutex_lock(&mt->localServer->lock);
            fix_fingers(mt->localServer->localNode);
            pthread_mutex_unlock(&mt->localServer->lock);
            */

            remote_fix_fingers(mt->localServer);

            last_fixed_time = now;

            log_info("[MAINTENANCE] Fix fingers completed for Node %d\n", mt->localServer->localNode->id);
        }

        usleep(50000); // Sleep for stabilize interval
    }

    log_info("[MAINTENANCE] Background thread stopping for Node %d\n", mt->localServer->localNode->id);

    return NULL;
}

//Initializes a MaintenanceThread object, assigns time interval values, creates process thread and logs the start of the thread
MaintenanceThread* start_maintenance_thread(t_server* localServer, int stabilize_interval_ms, int fix_fingers_interval_ms) {
    MaintenanceThread* mt = malloc(sizeof(MaintenanceThread));
    mt->localServer = localServer;
    mt->stabilize_interval_ms = stabilize_interval_ms;
    mt->fix_fingers_interval_ms = fix_fingers_interval_ms;
    mt->running = 1;

    pthread_t thread_id;
    if(pthread_create(&mt->thread_id, NULL, maintanance_worker,(void*)mt) != 0) {
        log_warn("Failed to create maintenance thread\n");
        free(mt);
        return NULL;
    }

    log_info("[Maintenance] Started background thread for Node %d \n",
           localServer->localNode->id);

    return mt;
}

//Stops the background thread
//mt->running is a flag value that is checked by the loop in maintanance_worker() to stop the process
//Logs the stop of the process
void stop_maintenance_thread(MaintenanceThread* mt) {
    if (mt == NULL) {
        return;
    }

    mt->running = 0; // Signal the thread to stop
    sleep(1); // Give the thread time to exit

    log_info("[Maintenance] Stopped background thread for Node %d \n",
           mt->localServer->localNode->id);
    free(mt);
    sleep(1); // Ensure thread has time to clean up
}


//Local implementations of stabilize and fix_finger, does not work for a distributed network of computers

void perform_stabilization_cycle(Node* node) {
    log_info("[STAB] Manual stabilization triggered for Node %d\n", node->id);
    stabilize(node);
}

void perform_finger_fix_cycle(Node* node) {
    log_info("[FINGER] Manual finger fix triggered for Node %d\n", node->id);
    fix_fingers(node);
}
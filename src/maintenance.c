#include "maintenance.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "logger.h"

void* maintanance_worker(void* arg) {
    MaintenanceThread* mt = (MaintenanceThread*)arg;
    time_t last_stabilized_time = time(NULL);
    time_t last_fixed_time = time(NULL);

    log_info("[MAINTENANCE] Background thread started for Node %d\n", mt->node->id);

    while (mt->running) {
        time_t now = time(NULL);

        if ((now - last_stabilized_time) >= mt->stabilize_interval_ms / 1000) {
            remote_stabilize(mt->node);

            last_stabilized_time = now;

            log_info("[MAINTENANCE] Stabilization completed for Node %d\n", mt->node->id);
        }

        if ((now-last_fixed_time) >= mt->fix_fingers_interval_ms / 1000) {
            fix_fingers(mt->node);

            last_fixed_time = now;

            log_info("[MAINTENANCE] Fix fingers completed for Node %d\n", mt->node->id);
        }

        usleep(50000); // Sleep for stabilize interval
    }

    log_info("[MAINTENANCE] Background thread stopping for Node %d\n", mt->node->id);

    return NULL;
}

MaintenanceThread* start_maintenance_thread(Node* node, int stabilize_interval_ms, int fix_fingers_interval_ms) {
    MaintenanceThread* mt = malloc(sizeof(MaintenanceThread));
    mt->node = node;
    mt->stabilize_interval_ms = stabilize_interval_ms;
    mt->fix_fingers_interval_ms = fix_fingers_interval_ms;
    mt->running = 1;

    pthread_t thread_id;
    if(pthread_create(&thread_id, NULL, maintanance_worker,(void*)mt) != 0) {
        log_warn("Failed to create maintenance thread\n");
        free(mt);
        return NULL;
    }

    log_info("[Maintenance] Started background thread for Node %d \n",
           node->id);

    return mt;
}

void stop_maintenance_thread(MaintenanceThread* mt) {
    if (mt == NULL) {
        return;
    }

    mt->running = 0; // Signal the thread to stop
    sleep(1); // Give the thread time to exit

    log_info("[Maintenance] Stopped background thread for Node %d \n",
           mt->node->id);
    free(mt);
    sleep(1); // Ensure thread has time to clean up
}

void perform_stabilization_cycle(Node* node) {
    log_info("[STAB] Manual stabilization triggered for Node %d\n", node->id);
    stabilize(node);
}

void perform_finger_fix_cycle(Node* node) {
    log_info("[FINGER] Manual finger fix triggered for Node %d\n", node->id);
    fix_fingers(node);
}
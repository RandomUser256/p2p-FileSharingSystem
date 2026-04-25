# Periodic Maintenance Strategy for Distributed Chord Network

## Overview

To maintain a healthy Chord ring, two maintenance operations must run periodically on each node:

1. **`stabilize()`** - Verifies and repairs successor/predecessor links (run frequently)
2. **`fix_fingers()`** - Updates finger table entries (run less frequently)

**Recommended Intervals**:
- `stabilize()`: Every 100-500ms
- `fix_fingers()`: Every 5-10 seconds

---

## Approach 1: Multi-threaded (Recommended for Production)

### Best For
- Always-on daemon nodes
- Continuous network operation
- Consistent maintenance cadence

### Implementation

#### Step 1: Create maintenance header file
**File**: `src/maintenance.h`

```c
#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <pthread.h>
#include <time.h>
#include "node.c"

typedef struct {
    Node* node;
    int stabilize_interval_ms;    // milliseconds between stabilize calls
    int fix_fingers_interval_ms;  // milliseconds between fix_fingers calls
    volatile int running;         // flag to stop maintenance thread
} MaintenanceThread;

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
```

#### Step 2: Implement maintenance functions
**File**: `src/maintenance.c`

```c
#include "maintenance.h"
#include <unistd.h>
#include <stdio.h>

void* maintenance_worker(void* arg) {
    MaintenanceThread* mt = (MaintenanceThread*)arg;
    time_t last_stabilize = time(NULL);
    time_t last_fix_fingers = time(NULL);
    
    printf("[MAINTENANCE] Background thread started for Node %d\n", mt->node->id);
    
    while (mt->running) {
        time_t now = time(NULL);
        
        // Check if it's time to stabilize
        if ((now - last_stabilize) * 1000 >= mt->stabilize_interval_ms) {
            stabilize(mt->node);
            last_stabilize = now;
            printf("[STAB] Node %d stabilization completed at %ld\n", 
                   mt->node->id, now);
        }
        
        // Check if it's time to fix fingers
        if ((now - last_fix_fingers) * 1000 >= mt->fix_fingers_interval_ms) {
            fix_fingers(mt->node);
            last_fix_fingers = now;
            printf("[FINGER] Node %d finger table updated at %ld\n", 
                   mt->node->id, now);
        }
        
        // Sleep to avoid busy-waiting
        usleep(50000);  // 50ms sleep between checks
    }
    
    printf("[MAINTENANCE] Background thread stopping for Node %d\n", mt->node->id);
    return NULL;
}

MaintenanceThread* start_maintenance_thread(Node* node,
                                            int stabilize_interval_ms,
                                            int fix_fingers_interval_ms) {
    MaintenanceThread* mt = malloc(sizeof(MaintenanceThread));
    mt->node = node;
    mt->stabilize_interval_ms = stabilize_interval_ms;
    mt->fix_fingers_interval_ms = fix_fingers_interval_ms;
    mt->running = 1;
    
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, maintenance_worker, (void*)mt) != 0) {
        fprintf(stderr, "Error creating maintenance thread\n");
        free(mt);
        return NULL;
    }
    
    printf("[MAINTENANCE] Thread created (stabilize=%dms, fix_fingers=%dms)\n",
           stabilize_interval_ms, fix_fingers_interval_ms);
    
    return mt;
}

void stop_maintenance_thread(MaintenanceThread* mt) {
    if (mt == NULL) return;
    
    mt->running = 0;
    sleep(1);  // Give thread time to exit gracefully
    
    printf("[MAINTENANCE] Thread stop requested\n");
    free(mt);
}

void perform_stabilization_cycle(Node* node) {
    printf("[STAB] Manual stabilization triggered for Node %d\n", node->id);
    stabilize(node);
}

void perform_finger_fix_cycle(Node* node) {
    printf("[FINGER] Manual finger fix triggered for Node %d\n", node->id);
    fix_fingers(node);
}
```

#### Step 3: Update main.c to use threading

```c
#include <math.h>
#include <time.h>
#include "src/maintenance.h"

int main() {
    char input[100];

    Node* localNode = loadNodeFromFile("nodeInfo/Node");
    loadFingerTableFromFile(localNode, "nodeInfo/FingerTable");

    // Start background maintenance thread
    // stabilize every 200ms, fix_fingers every 8 seconds
    MaintenanceThread* mt = start_maintenance_thread(localNode, 200, 8000);

    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Chord Node %d Started with Background Maintenance       ║\n", localNode->id);
    printf("║  stabilize: 200ms | fix_fingers: 8000ms                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    while (true) {
        printf("\nEnter command ([t]est ring, [g]et info, [s]tabilize, [f]ix fingers, [e]xit): ");
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\r\n")] = '\0';

        if (strcmp(input, "t") == 0) {
            remote_check_ring(localNode, localNode->Ip);
        } else if (strcmp(input, "g") == 0) {
            nodePrint(localNode);
            printFingerTable(localNode);
        } else if (strcmp(input, "s") == 0) {
            perform_stabilization_cycle(localNode);
        } else if (strcmp(input, "f") == 0) {
            perform_finger_fix_cycle(localNode);
        } else if (strcmp(input, "e") == 0) {
            printf("Exiting...\n");
            stop_maintenance_thread(mt);
            freeNode(localNode);
            break;
        } else {
            printf("Invalid command.\n");
        }
    }

    return 0;
}
```

**Compile with threading**:
```bash
gcc -pthread main.c -o chord_node -lm
```

---

## Approach 2: Signal-based Periodic Timer (Linux/Unix)

### Best For
- Simpler implementation
- No additional threads
- Resource-constrained environments

### Implementation

**File**: `src/signal_maintenance.c`

```c
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "node.c"

volatile Node* g_maintenance_node = NULL;
volatile int stabilize_counter = 0;

void handle_alarm(int sig) {
    if (g_maintenance_node == NULL) return;
    
    stabilize_counter++;
    
    // Stabilize every alarm (every 100ms)
    stabilize((Node*)g_maintenance_node);
    
    // Fix fingers every 100 alarms (every 10 seconds)
    if (stabilize_counter % 100 == 0) {
        fix_fingers((Node*)g_maintenance_node);
        printf("[MAINTENANCE] Finger table updated\n");
    }
    
    // Reset alarm
    signal(SIGALRM, handle_alarm);
    alarm(1);  // 1 second intervals (adjust for finer granularity)
}

void setup_signal_maintenance(Node* node) {
    g_maintenance_node = node;
    signal(SIGALRM, handle_alarm);
    alarm(1);  // Start 1-second timer
    printf("[MAINTENANCE] Signal-based maintenance started\n");
}

void teardown_signal_maintenance() {
    alarm(0);  // Cancel alarm
    g_maintenance_node = NULL;
    printf("[MAINTENANCE] Signal-based maintenance stopped\n");
}
```

**Usage in main**:
```c
#include "src/signal_maintenance.c"

int main() {
    Node* localNode = loadNodeFromFile("nodeInfo/Node");
    loadFingerTableFromFile(localNode, "nodeInfo/FingerTable");
    
    setup_signal_maintenance(localNode);
    
    // ... rest of main loop
    
    teardown_signal_maintenance();
    return 0;
}
```

---

## Approach 3: Cron/External Scheduler (Distributed)

### Best For
- Cluster deployments
- Each node runs independently
- Managed by external scheduler (systemd, cron, container orchestration)

### Implementation

#### Step 1: Create RPC commands for maintenance
**Update**: `scripts/node_comms.c`

```c
// Add stabilize_all command
if (strcmp(argv[1], "stabilize_all") == 0) {
    if (!node->successor) {
        fprintf(stderr, "Error: Node has no successor\n");
        return 1;
    }
    
    stabilize(node);
    printf("[INFO] Node %d stabilized\n", node->id);
    return 0;
}

// Add fix_fingers command
if (strcmp(argv[1], "fix_fingers_all") == 0) {
    fix_fingers(node);
    printf("[INFO] Node %d finger table updated\n", node->id);
    return 0;
}
```

#### Step 2: Create systemd timer for Linux
**File**: `/etc/systemd/system/chord-stabilize.service`

```ini
[Unit]
Description=Chord Network Node Stabilization
After=network.target

[Service]
Type=oneshot
ExecStart=/path/to/project/scripts/node_comms stabilize_all
User=chord
WorkingDirectory=/path/to/project

[Install]
WantedBy=multi-user.target
```

**File**: `/etc/systemd/system/chord-stabilize.timer`

```ini
[Unit]
Description=Periodic Chord Stabilization
Requires=chord-stabilize.service

[Timer]
OnBootSec=1min
OnUnitActiveSec=200ms
Persistent=true

[Install]
WantedBy=timers.target
```

**Enable**:
```bash
systemctl daemon-reload
systemctl enable chord-stabilize.timer
systemctl start chord-stabilize.timer
```

#### Step 3: Create cron job for legacy systems
**Crontab entry**:
```bash
# Every 200ms (approximate with 1-minute intervals)
* * * * * /path/to/project/scripts/node_comms stabilize_all >> /var/log/chord/stabilize.log 2>&1

# Every 8 seconds with multiple cron entries
* * * * * /path/to/project/scripts/node_comms fix_fingers_all >> /var/log/chord/fingers.log 2>&1
* * * * * sleep 8; /path/to/project/scripts/node_comms fix_fingers_all >> /var/log/chord/fingers.log 2>&1
```

---

## Approach 4: Event Loop with Async I/O

### Best For
- High-concurrency scenarios
- Integration with async frameworks
- Scalable to many nodes

### Implementation

**File**: `src/async_maintenance.h`

```c
#ifndef ASYNC_MAINTENANCE_H
#define ASYNC_MAINTENANCE_H

#include <sys/time.h>
#include "node.c"

typedef struct {
    Node* node;
    struct timeval last_stabilize;
    struct timeval last_fix_fingers;
    int stabilize_interval_us;    // microseconds
    int fix_fingers_interval_us;  // microseconds
} AsyncMaintenance;

AsyncMaintenance* create_async_maintenance(Node* node,
                                          int stabilize_interval_ms,
                                          int fix_fingers_interval_ms);

void check_maintenance_timers(AsyncMaintenance* am);

void cleanup_async_maintenance(AsyncMaintenance* am);

#endif
```

**File**: `src/async_maintenance.c`

```c
#include "async_maintenance.h"
#include <stdlib.h>

AsyncMaintenance* create_async_maintenance(Node* node,
                                          int stabilize_interval_ms,
                                          int fix_fingers_interval_ms) {
    AsyncMaintenance* am = malloc(sizeof(AsyncMaintenance));
    am->node = node;
    am->stabilize_interval_us = stabilize_interval_ms * 1000;
    am->fix_fingers_interval_us = fix_fingers_interval_ms * 1000;
    
    gettimeofday(&am->last_stabilize, NULL);
    gettimeofday(&am->last_fix_fingers, NULL);
    
    return am;
}

long timeval_diff_us(struct timeval* start, struct timeval* end) {
    return (end->tv_sec - start->tv_sec) * 1000000 +
           (end->tv_usec - start->tv_usec);
}

void check_maintenance_timers(AsyncMaintenance* am) {
    if (am == NULL || am->node == NULL) return;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Check stabilization timer
    if (timeval_diff_us(&am->last_stabilize, &now) >= am->stabilize_interval_us) {
        stabilize(am->node);
        gettimeofday(&am->last_stabilize, NULL);
    }
    
    // Check finger fix timer
    if (timeval_diff_us(&am->last_fix_fingers, &now) >= am->fix_fingers_interval_us) {
        fix_fingers(am->node);
        gettimeofday(&am->last_fix_fingers, NULL);
    }
}

void cleanup_async_maintenance(AsyncMaintenance* am) {
    if (am != NULL) {
        free(am);
    }
}
```

**Usage in event loop**:
```c
AsyncMaintenance* am = create_async_maintenance(node, 200, 8000);

while (running) {
    // Process network events, commands, etc.
    handle_commands();
    handle_network_events();
    
    // Check if maintenance timers have expired
    check_maintenance_timers(am);
    
    // Sleep briefly to avoid busy-waiting
    usleep(10000);  // 10ms
}

cleanup_async_maintenance(am);
```

---

## Approach 5: Distributed Coordinator (For Large Networks)

### Best For
- Large-scale deployments (50+ nodes)
- Central monitoring
- Synchronized maintenance windows

### Implementation

**File**: `scripts/coordinator.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NODES 256

typedef struct {
    int id;
    char ip[16];
    time_t last_stabilize;
    time_t last_fix_fingers;
    int status;  // 0=offline, 1=online
} NodeStatus;

typedef struct {
    NodeStatus nodes[MAX_NODES];
    int node_count;
    int stabilize_interval;
    int fix_fingers_interval;
} Coordinator;

void coordinate_maintenance(Coordinator* coord) {
    time_t now = time(NULL);
    
    for (int i = 0; i < coord->node_count; i++) {
        NodeStatus* node = &coord->nodes[i];
        
        if (node->status == 0) continue;  // Skip offline nodes
        
        // Coordinate stabilization
        if (now - node->last_stabilize >= coord->stabilize_interval) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd),
                "ssh %s 'cd /path && ./scripts/node_comms stabilize_all'",
                node->ip);
            
            system(cmd);
            node->last_stabilize = now;
            printf("[COORD] Node %d stabilized\n", node->id);
        }
        
        // Coordinate finger fixing
        if (now - node->last_fix_fingers >= coord->fix_fingers_interval) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd),
                "ssh %s 'cd /path && ./scripts/node_comms fix_fingers_all'",
                node->ip);
            
            system(cmd);
            node->last_fix_fingers = now;
            printf("[COORD] Node %d fingers updated\n", node->id);
        }
    }
}

int main() {
    Coordinator coord = {0};
    coord.stabilize_interval = 1;   // 1 second
    coord.fix_fingers_interval = 8; // 8 seconds
    
    // Load node list
    FILE* nodes_file = fopen("nodes.txt", "r");
    while (fscanf(nodes_file, "%d %s", &coord.nodes[coord.node_count].id,
                  coord.nodes[coord.node_count].ip) == 2) {
        coord.nodes[coord.node_count].status = 1;
        coord.node_count++;
    }
    
    printf("[COORDINATOR] Started with %d nodes\n", coord.node_count);
    
    while (1) {
        coordinate_maintenance(&coord);
        sleep(1);
    }
    
    return 0;
}
```

---

## Comparison Matrix

| Approach | Complexity | Scalability | Overhead | Best Use Case |
|----------|-----------|------------|----------|---------------|
| **Multi-threaded** | Medium | Good (1-100 nodes) | Low | Production daemon |
| **Signal-based** | Low | Fair (1-50 nodes) | Very Low | Simple deployments |
| **Cron/Scheduler** | Low | Excellent (100+ nodes) | Medium | Enterprise/cluster |
| **Event Loop** | High | Excellent | Low | High-concurrency |
| **Coordinator** | High | Excellent | Medium | Large networks |

---

## Recommended Configuration

### For Small Networks (1-10 nodes)
```
Approach: Multi-threaded (Approach 1)
stabilize(): 200ms
fix_fingers(): 8 seconds
```

### For Medium Networks (10-100 nodes)
```
Approach: Signal-based + Cron (Approaches 2 + 3)
stabilize(): 500ms (via cron every minute)
fix_fingers(): 10 seconds (via cron)
```

### For Large Networks (100+ nodes)
```
Approach: Coordinator + Scheduler (Approaches 3 + 5)
Coordinator orchestrates maintenance
stabilize(): 2-5 seconds
fix_fingers(): 30-60 seconds
```

---

## Implementation Priority

1. **Start with Approach 1** (multi-threaded) - Simplest for development
2. **Add Approach 3** (RPC commands) - Enables external scheduling
3. **Graduate to Approach 5** (coordinator) - For production scale

---

## Example: Full Integration with Approach 1

**Compile Command**:
```bash
gcc -pthread -Wall -Wextra main.c src/maintenance.c -o chord_node -lm
```

**Usage**:
```bash
# Start node with background maintenance
./chord_node

# Node will run maintenance in background while accepting commands
```

**Configuration File** (optional): `config/maintenance.conf`
```
stabilize_interval_ms=200
fix_fingers_interval_ms=8000
log_level=info
```

---

## Monitoring Maintenance

Add logging to verify maintenance is working:

```c
typedef struct {
    long stabilize_count;
    long fix_fingers_count;
    time_t uptime;
} MaintenanceStats;

void print_maintenance_stats(MaintenanceStats* stats) {
    printf("═════════════════════════════════════════\n");
    printf("Maintenance Statistics:\n");
    printf("  Stabilizations: %ld\n", stats->stabilize_count);
    printf("  Finger Updates: %ld\n", stats->fix_fingers_count);
    printf("  Uptime: %ld seconds\n", stats->uptime);
    printf("═════════════════════════════════════════\n");
}
```

---

## Troubleshooting

### Problem: Maintenance not running
**Solutions**:
- Check thread is created successfully
- Verify `node->successor` is not NULL
- Check disk space for file saves
- Monitor CPU usage

### Problem: Ring becoming inconsistent
**Solutions**:
- Decrease maintenance intervals
- Check network connectivity
- Verify successor/predecessor links
- Enable detailed logging

### Problem: High CPU usage
**Solutions**:
- Increase maintenance intervals
- Use event loop approach instead of threading
- Profile with `perf` or similar tools

---

## Summary

**Choose your approach based on:**
1. **Scale**: How many nodes? (1-10, 10-100, 100+)
2. **Deployment**: Single machine or distributed?
3. **Complexity**: How much engineering effort available?
4. **Reliability**: How critical is consistency?

**Most recommended for your case**:
- **Phase 1**: Implement Approach 1 (multi-threaded) for development
- **Phase 2**: Add Approach 3 (RPC) for flexibility
- **Phase 3**: Scale to Approach 5 (coordinator) when needed

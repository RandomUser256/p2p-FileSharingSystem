# Finger Table Implementation Guide

## Overview
This document describes the implementation of the Chord algorithm's Finger Table persistence and optimized lookup mechanisms for the P2P File Sharing System.

## Architecture

### Components Implemented

#### 1. **Finger Table Persistence** (`node.c`)
Functions to save and load finger tables from disk storage:

- **`void saveFingerTableToFile(Node* node, const char* filepath)`**
  - Serializes the complete finger table to a text file
  - Format: `entry=<idx>,start=<start>,lower=<lower>,upper=<upper>,successor_id=<id>,successor_ip=<ip>`
  - Stores interval bounds and successor references
  - Called after topology changes to preserve state

- **`void loadFingerTableFromFile(Node* node, const char* filepath)`**
  - Deserializes finger table from disk
  - Reconstructs successor node references from stored IPs
  - Handles missing or corrupted files gracefully
  - Preserves heap allocation safety with `freeNode()`

- **`void printFingerTable(Node* node)`**
  - Formatted output of finger table for debugging
  - Shows entry index, interval bounds, and successor info
  - Used in test and validation scenarios

#### 2. **Remote Finger Table Operations** (`node.c`)

- **`void remote_print_finger_table(const char* ip)`**
  - SSH into remote node and print its finger table
  - Requires working SSH access to remote machines

- **`void remote_load_and_update_finger_table(Node* node, const char* remote_ip)`**
  - Fetch remote node's finger table via SSH
  - Integrate data into local node structure
  - Used for shadow copies or cache updates

#### 3. **Optimized Lookup Functions** (`node.c`)

- **`Node* find_successor_with_finger_table(Node* node, int id)`**
  - O(log n) lookup using finger table guidance
  - Steps:
    1. Local interval check: `(node_id, successor_id]`
    2. Find closest preceding finger via `closest_preceding_finger()`
    3. Remote lookup if needed: `remote_find_successor(cpf->Ip, id)`
  - More efficient than naive O(n) successor walk

- **`Node* find_predecessor_with_finger_table(Node* node, int id)`**
  - Optimized predecessor search using finger tables
  - Uses finger table to jump closer to target
  - Limits to O(log n) hops instead of full ring traversal

#### 4. **Node Communications Extension** (`scripts/node_comms.c`)
Added RPC commands for remote finger table operations:

- **`print_finger_table`** - Display finger table on remote node
- **`save_finger_table`** - Persist finger table to disk on remote node
- **`load_finger_table`** - Load finger table from disk on remote node
- **`get_finger_entry <index>`** - Retrieve specific finger table entry

## File Formats

### FingerTable Storage Format
File: `nodeInfo/FingerTable`

```
# Finger Table for Node 4
# Format: entry=<idx>,start=<start>,lower=<lower>,upper=<upper>,successor_id=<id>,successor_ip=<ip>

entry=0,start=5,lower=5,upper=6,successor_id=5,successor_ip=10.11.20.41
entry=1,start=6,lower=6,upper=8,successor_id=6,successor_ip=10.11.20.42
entry=2,start=8,lower=8,upper=12,successor_id=4,successor_ip=10.11.20.40
entry=3,start=12,lower=12,upper=4,successor_id=4,successor_ip=10.11.20.40
```

**Fields:**
- `entry`: Finger table index (0 to NODE_ID_LENGTH-1)
- `start`: Start of interval: `(node_id + 2^i) % 2^m`
- `lower`: Lower interval bound (same as start)
- `upper`: Upper interval bound (start of next interval)
- `successor_id`: ID of node in this interval
- `successor_ip`: IP address of successor node

### Node Storage Format
File: `nodeInfo/Node`

```
id=4
ip=10.11.20.40
fileContentPath=shared/files
successor=5 10.11.20.41
predecessor=6 10.11.20.42
```

## Usage Examples

### 1. Load and Display Finger Table
```c
Node* node = loadNodeFromFile("nodeInfo/Node");
loadFingerTableFromFile(node, "nodeInfo/FingerTable");
printFingerTable(node);
```

### 2. Optimized File Lookup
```c
// Find which node should store a file with ID 7
Node* responsibleNode = find_successor_with_finger_table(node4, 7);
printf("File storage node: ID=%d IP=%s\n", 
       responsibleNode->id, responsibleNode->Ip);
```

### 3. Traverse Ring with Finger Table Optimization
```c
// Find predecessor of ID 10
Node* pred = find_predecessor_with_finger_table(node4, 10);
printf("Predecessor of 10: ID=%d IP=%s\n", pred->id, pred->Ip);
freeNode(pred);
```

### 4. Save Updated Finger Table
```c
// After topology changes (node joins/leaves)
saveFingerTableToFile(node, "nodeInfo/FingerTable");
```

### 5. Remote Operations via SSH
```c
// Query remote node's finger table
remote_load_and_update_finger_table(node4, "10.11.20.41");
printFingerTable(node4);

// Print remote node's finger table
remote_print_finger_table("10.11.20.41");
```

## Chord Algorithm Integration

### Finger Table in Chord
In the Chord algorithm, each node maintains a finger table with entries pointing to nodes at exponentially increasing distances around the ring:

```
For node n with m-bit IDs:
  fingerTable[i].start = (n + 2^(i-1)) % 2^m    for i = 1 to m
  fingerTable[i].successor = first node >= fingerTable[i].start
```

### Lookup Efficiency
- **Without Finger Table**: O(n) - requires visiting every node
- **With Finger Table**: O(log n) - exponential skipping reduces hops
- **This Implementation**: Combines local finger table with remote SSH calls

## Synchronization Strategy

### When to Update Finger Tables
1. **Node Join**: `init_finger_table()` and `update_others()`
2. **Node Leave**: Trigger `update_finger_table()` on affected nodes
3. **Periodic Maintenance**: `fix_fingers()` runs periodically
4. **After Topology Stabilization**: `stabilize()` adjusts pointers

### File Persistence
- **On Startup**: `loadFingerTableFromFile()` restores state
- **On Topology Change**: `saveFingerTableToFile()` records new state
- **Remote Sync**: `remote_load_and_update_finger_table()` syncs across nodes

## Data Flow: File Insertion

```
┌─────────────────┐
│  Filename       │
│   "data.txt"    │
└────────┬────────┘
         │
         ├─→ hash_filename()          [TODO: Implement]
         │
         ├─→ hash_value = 7
         │
         ├─→ lookup(node4, 7)
         │
         ├─→ find_successor_with_finger_table()
         │
         ├─→ Uses finger table to navigate efficiently
         │
         ├─→ Returns Node 4 as responsible
         │
         └─→ Store file in node->fileContentPath
             └─→ shared/files/data.txt
```

## Testing

### Test Commands
```bash
# Compile the project
gcc main.c -o main -lm

# Run with finger table operations
./main

# Test remote operations (requires SSH access)
ssh 10.11.20.40 "cd /project && ./scripts/node_comms print_finger_table"
ssh 10.11.20.40 "cd /project && ./scripts/node_comms save_finger_table"
```

### Expected Output
```
=== Finger Table for Node 4 (IP: 10.11.20.40) ===
Idx │ Start │ Lower │ Upper │ Successor ID │ Successor IP  
0   │  5    │  5    │  6    │      5       │ 10.11.20.41
1   │  6    │  6    │  8    │      6       │ 10.11.20.42
2   │  8    │  8    │ 12    │      4       │ 10.11.20.40
3   │ 12    │ 12    │  4    │      4       │ 10.11.20.40
```

## Key Implementation Details

### Memory Management
- All remote nodes allocated on heap with `createNode()`
- Must call `freeNode()` to prevent memory leaks
- Finger table successors managed carefully in `loadFingerTableFromFile()`

### SSH Communication
- Commands: `remote_find_successor()`, `remote_get_successor()`, `remote_closest_preceding_finger()`
- Parsed from SSH output: `"<id> <ip>\n"`
- Error handling: Returns NULL on SSH failure

### Interval Checking
Three helper functions for interval logic:
- `in_open_interval(id, start, end)`: `(start, end)`
- `half_left_open_interval(id, start, end)`: `(start, end]`
- `half_right_open_interval(id, start, end)`: `[start, end)`

These handle wraparound in the circular ID space.

## TODO / Future Enhancements

1. **Hash Function**: Implement `hash_filename()` for deterministic file ID assignment
2. **Concurrent Updates**: Add locking for multi-threaded access
3. **Fault Tolerance**: Backup finger table entries (redundancy)
4. **Auto-Stabilization**: Periodic `stabilize()` and `fix_fingers()` background tasks
5. **Distributed Hash Table**: Complete DHT operations with get/put semantics
6. **Performance Metrics**: Add timing and hop count measurements
7. **Graceful Shutdown**: Persist state before node disconnect
8. **Replication**: Replicate files across k successor nodes

## Related Files

- `src/node.c` - Core Chord algorithm and finger table logic
- `src/DHASH.c` - Distributed hash and file operations
- `scripts/node_comms.c` - Remote node communication interface
- `nodeInfo/Node` - Persistent node configuration
- `nodeInfo/FingerTable` - Persistent finger table state
- `shared/files/` - Directory for storing distributed files

## References

1. Stoica, I., Morris, R., Liben-Nowell, D., et al. (2003). "Chord: A Scalable Peer-to-peer Lookup Service for Internet Applications"
2. Current Chord implementation: `src/node.c`
3. Test suite: `main_test.c` (validates correctness)

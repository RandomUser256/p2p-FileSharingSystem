# Quick Reference Guide - Finger Table Implementation

## API Reference

### Finger Table Persistence

| Function | Purpose | Example |
|----------|---------|---------|
| `void saveFingerTableToFile(Node* node, const char* filepath)` | Save finger table to disk | `saveFingerTableToFile(node4, "nodeInfo/FingerTable");` |
| `void loadFingerTableFromFile(Node* node, const char* filepath)` | Load finger table from disk | `loadFingerTableFromFile(node4, "nodeInfo/FingerTable");` |
| `void printFingerTable(Node* node)` | Display formatted finger table | `printFingerTable(node4);` |

### Remote Finger Table Operations

| Function | Purpose | Example |
|----------|---------|---------|
| `void remote_print_finger_table(const char* ip)` | Print remote node's finger table | `remote_print_finger_table("10.11.20.41");` |
| `void remote_load_and_update_finger_table(Node* node, const char* remote_ip)` | Load remote finger table | `remote_load_and_update_finger_table(node4, "10.11.20.41");` |

### Optimized Lookups

| Function | Purpose | Complexity | Example |
|----------|---------|-----------|---------|
| `Node* find_successor_with_finger_table(Node* node, int id)` | Find successor using finger table | O(log n) | `Node* succ = find_successor_with_finger_table(node4, 7);` |
| `Node* find_predecessor_with_finger_table(Node* node, int id)` | Find predecessor using finger table | O(log n) | `Node* pred = find_predecessor_with_finger_table(node4, 7);` |

### Node Communication (node_comms.c)

| Command | Purpose | Usage |
|---------|---------|-------|
| `print_finger_table` | Print finger table remotely | `./node_comms print_finger_table` |
| `save_finger_table` | Save finger table on remote | `./node_comms save_finger_table` |
| `load_finger_table` | Load finger table on remote | `./node_comms load_finger_table` |
| `get_finger_entry <idx>` | Get specific entry | `./node_comms get_finger_entry 0` |
| `find_successor <id>` | Find successor | `./node_comms find_successor 7` |
| `get_successor` | Get direct successor | `./node_comms get_successor` |
| `closest_preceding_finger <id>` | Find CPF | `./node_comms closest_preceding_finger 7` |

## Data Structures

### FingerTableEntry
```c
typedef struct FingerTableEntry {
    int start;                    // Start of interval: (node_id + 2^i) % 2^m
    int lowerIntervalLimit;       // Lower bound of interval
    int upperIntervalLimit;       // Upper bound of interval
    struct Node* successor;       // Pointer to responsible node
    char Ip[MAX_IP_LENGTH];       // IP address of successor
} FingerTableEntry;
```

### Node (with Finger Table)
```c
typedef struct Node {
    int id;                                   // Node ID (0-15 in 4-bit space)
    char Ip[MAX_IP_LENGTH];                  // IP address
    struct Node* successor;                  // Direct successor pointer
    struct Node* predecessor;                // Direct predecessor pointer
    char fileContentPath[MAX_FILE_PATH_PATH]; // Storage directory
    struct FingerTableEntry fingerTable[NODE_ID_LENGTH]; // Finger table
} Node;
```

## Common Workflows

### 1. Initialize System at Startup
```c
// Load local node
Node* local = loadNodeFromFile("nodeInfo/Node");

// Restore finger table from disk
loadFingerTableFromFile(local, "nodeInfo/FingerTable");

// Display for verification
printFingerTable(local);
```

### 2. Perform File Lookup
```c
// File hashes to ID 10
int fileId = 10;

// Find responsible node using optimized lookup
Node* responsible = find_successor_with_finger_table(local, fileId);

// Use for file operations
printf("Store at node: %d (%s)\n", responsible->id, responsible->Ip);

// Clean up
freeNode(responsible);
```

### 3. Handle Topology Change
```c
// After node joins or leaves
// ... chord operations happen ...

// Update and persist finger tables
saveFingerTableToFile(local, "nodeInfo/FingerTable");

// Optionally save other nodes via SSH
// (they would do this themselves)
```

### 4. Remote Node Query
```c
// Load remote node's finger table
remote_load_and_update_finger_table(local, "10.11.20.41");

// Now can display
printFingerTable(local);

// Or query individual entry
Node* cpf = remote_closest_preceding_finger("10.11.20.41", 10);
```

## Configuration Files

### nodeInfo/Node
```
id=4
ip=10.11.20.40
fileContentPath=shared/files
successor=5 10.11.20.41
predecessor=6 10.11.20.42
```

### nodeInfo/FingerTable
```
entry=0,start=5,lower=5,upper=6,successor_id=5,successor_ip=10.11.20.41
entry=1,start=6,lower=6,upper=8,successor_id=6,successor_ip=10.11.20.42
entry=2,start=8,lower=8,upper=12,successor_id=4,successor_ip=10.11.20.40
entry=3,start=12,lower=12,upper=4,successor_id=4,successor_ip=10.11.20.40
```

## Compilation

```bash
# Standard compilation
gcc main.c -o main -lm

# With debugging symbols
gcc -g main.c -o main -lm

# With strict warnings
gcc -Wall -Wextra main.c -o main -lm

# Run tests
gcc main_test.c -o test -lm
./test
```

## SSH Setup for Remote Operations

For remote finger table operations to work:

1. **SSH keys configured** between nodes
2. **node_comms executable** in `scripts/` directory on each node
3. **Working directory** contains `nodeInfo/Node` and `nodeInfo/FingerTable`

```bash
# Test SSH connectivity
ssh 10.11.20.41 "echo 'Connected'"

# Test node_comms
ssh 10.11.20.41 "cd /project && ./scripts/node_comms get_successor"
```

## Performance Characteristics

### Lookup Complexity
- **Full Ring Traversal**: O(n) - visits all n nodes
- **With Finger Tables**: O(log n) - exponential skipping
- **This Implementation**: O(log n) with SSH latency overhead

### Storage
- **Finger Table**: 4 entries × ~40 bytes = ~160 bytes per node
- **Node File**: ~200 bytes
- **Minimal overhead** for large networks

### Network Traffic
- **Per Lookup**: ~log n SSH connections
- **Per Save**: 1 SSH write
- **Per Load**: 1 SSH read

## Debugging Tips

### Print Finger Table
```c
printFingerTable(node);
```

### Verify Structure
```c
for (int i = 0; i < NODE_ID_LENGTH; i++) {
    printf("Entry %d: start=%d, successor=%d\n",
           i,
           node->fingerTable[i].start,
           node->fingerTable[i].successor->id);
}
```

### Check File Contents
```bash
cat nodeInfo/Node
cat nodeInfo/FingerTable
```

### Remote Debugging
```bash
ssh 10.11.20.41 "cat nodeInfo/FingerTable"
ssh 10.11.20.41 "cd /project && ./scripts/node_comms print_finger_table"
```

## Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| `loadFingerTableFromFile` finds empty file | File not updated | Call `saveFingerTableToFile` first |
| SSH commands fail | Network unreachable | Check SSH setup, firewall rules |
| Finger table not used in lookup | Using wrong function | Use `find_successor_with_finger_table()` |
| Memory leak after lookup | Forgot to `freeNode()` | Call `freeNode(result);` after use |
| Interval checks incorrect | Wraparound not handled | Verify use of interval helper functions |
| Remote load returns NULL | File not found on remote | Ensure `nodeInfo/FingerTable` exists remotely |

## Next Steps

1. **Implement hash function** for filenames → IDs
2. **Add replication** (k-redundancy)
3. **Implement get/put operations** for DHT
4. **Add background stabilization** tasks
5. **Test with real network** across multiple machines
6. **Monitor performance** metrics

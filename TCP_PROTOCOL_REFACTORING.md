# TCP Protocol Refactoring - Documentation

## Overview
This document describes the refactoring from SSH-based remote communication to TCP socket-based communication for the Chord DHT distributed system.

## Architecture Changes

### Previous Architecture (SSH-based)
```
Local Node (main.c) → CLI args → node_comms (subprocess via SSH) → Remote Node
                    ↑ Parse stdout (1st match only)
                    
Communication: SSH tunnel + stdin/stdout parsing
Issues: 
  - Slow (SSH overhead)
  - Shell escaping issues
  - Working directory assumptions
  - User authentication required
```

### New Architecture (TCP-based)
```
Local Node (main.c) 
    ↓
remote_* functions (node.c, DHASH.c)
    ↓
tcp_client.c (Socket API)
    ↓ TCP Port 9000
    ↓
Remote Node: node_comms (TCP Server)
    ↓
process_request() → Chord functions
    ↓ TCP Response
    ↓
tcp_client.c (Parse response)
    ↓
Return Node* structure

Communication: Direct TCP sockets
Advantages:
  - Fast (no SSH overhead)
  - No shell escaping needed
  - No authentication required
  - Type-safe communication via tcp_protocol.h
  - Asynchronous client handling (threaded server)
```

## Protocol Specification

### Request Format
```
<COMMAND>|<ARG1>|<ARG2>|...\n
```

### Response Format
```
<STATUS>|<DATA1>|<DATA2>|...\n
```

### Status Codes
- `OK` - Request succeeded, response contains data
- `ERROR` - Request failed, response contains error message
- `NOT_FOUND` - Resource not found

### Implemented Commands

#### 1. find_successor
**Request:** `find_successor|<target_id>`  
**Response:** `OK|<successor_id>|<successor_ip>` or `ERROR|<message>`  
**Purpose:** Find the successor of a given ID in the ring

#### 2. get_successor
**Request:** `get_successor`  
**Response:** `OK|<successor_id>|<successor_ip>` or `ERROR|<message>`  
**Purpose:** Get the immediate successor of the local node

#### 3. closest_preceding_finger
**Request:** `closest_preceding_finger|<target_id>`  
**Response:** `OK|<node_id>|<node_ip>` or `ERROR|<message>`  
**Purpose:** Find the closest preceding finger for a target ID

#### 4. notify
**Request:** `notify|<pred_id>|<pred_ip>`  
**Response:** `OK|<message>` or `ERROR|<message>`  
**Purpose:** Update the predecessor of the receiving node

#### 5. check_ring
**Request:** `check_ring`  
**Response:** `OK|<node_id>|<node_ip>` or `ERROR|<message>`  
**Purpose:** Verify local ring integrity

#### 6. print_finger_table
**Request:** `print_finger_table`  
**Response:** `OK|<message>`  
**Purpose:** Print finger table to console

#### 7. save_finger_table
**Request:** `save_finger_table`  
**Response:** `OK|<message>` or `ERROR|<message>`  
**Purpose:** Save finger table to disk

#### 8. load_finger_table
**Request:** `load_finger_table`  
**Response:** `OK|<message>` or `ERROR|<message>`  
**Purpose:** Load finger table from disk

#### 9. get_finger_entry
**Request:** `get_finger_entry|<index>`  
**Response:** `OK|<idx>|<start>|<lower>|<upper>|<succ_id>|<succ_ip>` or `ERROR|<message>`  
**Purpose:** Get a specific finger table entry

## Implementation Details

### New Files
1. **tcp_protocol.h** - Protocol constants and helper declarations
2. **tcp_protocol.c** - Protocol helper implementations (parse_response, build_request)
3. **tcp_client.h** - Client-side TCP functions
4. **tcp_client.c** - Socket connection management and request/response handling
5. **tcp_server.h** - Server-side TCP infrastructure
6. **tcp_server.c** - Multi-threaded server implementation

### Modified Files
1. **scripts/node_comms.c** - Converted from CLI tool to TCP server
2. **src/DHASH.c** - Replaced SSH-based remote functions with TCP calls
3. **src/node.c** - Added TCP includes, updated remote_notify and remote_join
4. **main.c** - Removed username parameter from remote_join calls

### Configuration Constants
```c
#define DEFAULT_TCP_PORT 9000        // Default listening port
#define TCP_BUFFER_SIZE 512          // Message buffer size
#define TCP_TIMEOUT_SEC 5            // Socket timeout in seconds
```

## Compilation

### Updated Compilation Command
```bash
# For node_comms (TCP server)
gcc -pthread node_comms.c ../src/DHASH.c ../src/logger.c ../src/node.c \
    ../src/tcp_server.c ../src/tcp_client.c ../src/tcp_protocol.c \
    -Wall -o node_comms -lm

# For main application
gcc -pthread main.c src/node.c src/DHASH.c src/maintenance.c src/logger.c \
    src/tcp_client.c src/tcp_protocol.c -o main -lm
```

### Dependencies
- POSIX sockets (sys/socket.h)
- POSIX threads (pthread.h)
- Standard C library (stdlib.h, string.h)
- Math library (-lm)

## Running the System

### 1. Start TCP Server on Each Node
```bash
# On node IP: 192.168.1.5
cd scripts
./node_comms 9000

# Output:
# ╔════════════════════════════════════════════════════════════╗
# ║  Chord Node 5 TCP Server Started                         ║
# ║  Listening on port 9000                                      ║
# ║  IP: 192.168.1.5                                        ║
# ╚════════════════════════════════════════════════════════════╝
```

### 2. Start Main Application
```bash
./main

# Follow prompts to join network or perform other operations
# Network operations now use TCP instead of SSH
```

### 3. Example: Joining a Node to the Ring
```
Enter command: n
Enter IP of existing node to join: 192.168.1.5
Joining node at 192.168.1.5...
[INFO] Node 7 has joined the ring via node at 192.168.1.5
```

## Testing

### Basic Connectivity Test
```bash
# Test if remote node is reachable
echo "get_successor" | nc -q 1 192.168.1.5 9000
# Expected: OK|20|192.168.1.10

# Test finger table lookup
echo "closest_preceding_finger|15" | nc -q 1 192.168.1.5 9000
# Expected: OK|18|192.168.1.7
```

### Debugging

Enable logging in your code:
```c
set_log_level(LOG_DEBUG);  // For detailed TCP messages
set_log_level(LOG_INFO);   // For operational messages
set_log_level(LOG_WARN);   // For warnings and errors only
```

Check TCP connections:
```bash
netstat -tuln | grep 9000   # Check if server is listening
netstat -tun | grep EST      # Check active connections
```

## Error Handling

### TCP Client Errors
- Connection failed: Log error, return NULL
- Timeout: 5-second socket timeout, logs error
- Invalid response format: Parse fails, logs error

### TCP Server Errors
- Bind failed: Server initialization fails
- Accept failed: Logs error, continues listening
- Send failed: Logs error, closes connection

### Protocol Errors
- Unknown command: Returns `ERROR|Unknown command`
- Missing arguments: Returns `ERROR|Missing <arg_name>`
- Invalid format: Returns `ERROR|Invalid response format`

## Performance Considerations

1. **Latency**: ~1-10ms per remote call (vs ~100-500ms with SSH)
2. **Throughput**: Limited by socket buffer size (512 bytes)
3. **Concurrency**: Multi-threaded server handles multiple simultaneous requests
4. **Memory**: One buffer per active connection (fixed size)

## Troubleshooting

### "Connection refused" error
- Ensure node_comms TCP server is running on remote node
- Check firewall allows port 9000
- Verify correct IP address

### "Timeout" errors
- Check network connectivity: `ping <remote_ip>`
- Verify node_comms is responsive: `echo "get_successor" | nc <remote_ip> 9000`
- Increase TCP_TIMEOUT_SEC if network is slow

### Incorrect responses
- Check node_comms is using updated TCP version
- Verify protocol format matches specification
- Enable LOG_DEBUG for detailed message traces

## Migration Notes

### From SSH to TCP
1. Ensure all nodes run updated node_comms
2. Remove SSH configuration requirements
3. Ensure TCP port 9000 is open on all nodes
4. Update any monitoring/automation scripts

### Backward Compatibility
- **Not backwards compatible** with old SSH-based system
- All nodes must be upgraded simultaneously
- Old node_comms CLI version is no longer supported

## Future Improvements

1. **Custom Port Configuration**: Environment variable or config file
2. **Authentication**: Add simple authentication token
3. **Encryption**: TLS/SSL support for secure communication
4. **Compression**: Gzip compression for large responses
5. **Message Framing**: Add length-prefixed messages for robustness
6. **Persistent Connections**: Connection pooling for better performance
7. **Timeout Tuning**: Make socket timeout configurable
8. **Binary Protocol**: Replace text-based with binary for better performance

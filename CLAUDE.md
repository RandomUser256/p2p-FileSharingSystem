# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

There is no Makefile. Compile all source files together with GCC (Linux/WSL):

```bash
gcc main.c src/node.c src/DHASH.c src/maintenance.c src/logger.c src/tcpServer.c -o main -pthread -lm
```

The binary must be run on Linux (uses POSIX sockets, `select`, `pthread`, `fcntl`). On Windows, use WSL.

## Running a node

The program loads its identity from `nodeInfo/Node` and `nodeInfo/FingerTable` on startup. If those files are absent it prompts for the node's own IP and creates a new identity. Node state is saved back to those files during operation.

The interactive CLI accepts single-letter commands:
- `n` — join an existing network (prompts for peer IP)
- `f` / `p` — find successor / predecessor
- `t` — walk the ring checking consistency
- `g` — print this node's info
- `l` / `w` — toggle INFO / WARN log output
- `e` — exit

## Architecture

This is a **Chord DHT** implementation. Each process represents one node in the ring. Nodes communicate exclusively over TCP on port 8080 (hardcoded).

### Module responsibilities

| File | Role |
|---|---|
| `main.c` | Entry point: loads node from disk, starts TCP server thread and maintenance thread, runs interactive CLI |
| `src/node.{c,h}` | Core Chord logic: `Node` and `FingerTableEntry` structs, local algorithms (`find_successor`, `find_predecessor`, `stabilize`, `notify`, `fix_fingers`, `closest_preceding_finger`), plus all remote TCP RPC wrappers (`remote_find_successor`, `remote_stabilize`, `remote_join`, `remote_check_ring`, etc.) and finger table persistence |
| `src/tcpServer.{c,h}` | Multi-client TCP server using `select()`. Parses newline-terminated text commands and dispatches to Chord logic. The `t_server` struct owns `localNode` and a `pthread_mutex_t lock` shared by all threads |
| `src/maintenance.{c,h}` | Background pthread that calls `remote_stabilize()` every 200 ms and `fix_fingers()` every 8000 ms |
| `src/DHASH.{c,h}` | File lookup (`lookup`) and insertion (`insert` via SCP). Hashing is not yet implemented — the destination node ID is currently passed manually |
| `src/logger.{c,h}` | Leveled logging (LOG_NONE → LOG_ERROR → LOG_WARN → LOG_INFO → LOG_DEBUG). Set via `set_log_level()` at runtime |

### Thread model

Three concurrent threads share `t_server`:
1. **Main thread** — interactive CLI
2. **server_loop thread** — `select()`-based TCP server accepting and processing client connections
3. **maintenance_worker thread** — periodic stabilize / fix-fingers

All access to `s->localNode` must be protected by `pthread_mutex_lock(&s->lock)`.

### TCP wire protocol

All messages are newline-terminated ASCII. Requests and responses:

```
GET_NODE                          → NODE <id> <ip> <succ_id> <succ_ip> <pred_id> <pred_ip>
FIND_SUCCESSOR <id>               → NODE <id> <ip>
CLOSEST_PRECEDING_FINGER <id>     → NODE <id> <ip>
STABILIZE <caller_ip>             → OK
JOIN <ip>                         → OK
CHECK_RING <origin_id>            → (propagates around the ring, no direct reply)
```

### Node identity and ring size

`NODE_ID_LENGTH = 4` bits → maximum 16 nodes (`MAX_NUMBER_NODES = 16`). Node IDs and finger table math use modular arithmetic over this space.

Node state is persisted to plain-text files:
- `nodeInfo/Node` — `id`, `ip`, `fileContentPath`, `successor`, `predecessor`
- `nodeInfo/FingerTable` — one entry per finger table slot

## Known issues (from TODO comments)

- `remote_stabilize()` calls `remote_get_node()` on the successor to obtain the successor's predecessor, but the predecessor field returned is sometimes wrong, causing ring-check failures.
- `remote_find_successor()` called from `remote_join()` may not converge correctly on a fresh join.
- The SSH username in legacy SSH-based calls is hardcoded to `/home/mmagallanes` and not parameterised.
- File hashing in `DHASH.c` is not implemented; `insert()` takes an explicit destination node ID instead of hashing the file identifier.

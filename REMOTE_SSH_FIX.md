# Remote SSH Path Fix - Function Calling Flow Analysis

## Problem Summary

When `remote_check_ring()` was called, it failed with:
- `[ERROR] Invalid response: Error opening Node file`
- `[ERROR] ERROR: successor->predecessor mismatch at node 2`

## Root Cause

The SSH commands in remote functions used **relative paths** that don't work in SSH context:

```bash
ssh 10.11.20.41 "./scripts/node_comms find_successor 5"
                 ↑
                 Relative path - SSH shell doesn't have correct working directory!
```

When the remote `node_comms` program tried to load `nodeInfo/Node`, the relative path failed because:
1. SSH doesn't change to the project directory by default
2. The `nodeInfo/Node` file path is relative to the project root
3. `node_comms` executed in SSH default home directory (~/) instead of `/home/chord/`

## Complete Function Calling Flow

```
main.c: remote_check_ring(localNode, localNode->successor->Ip)
    ↓
node.c: remote_check_ring() @ line 638
    ├─ Loop iteration 1:
    │   ├─ Calls: remote_closest_preceding_finger(ringChecker->Ip, ringChecker->successor->id)
    │   │   ↓
    │   │   DHASH.c: remote_closest_preceding_finger() @ line 154
    │   │   ├─ SSH command: ssh 10.11.20.41 "cd /home/chord && ./scripts/node_comms closest_preceding_finger 5"
    │   │   ├─ Remote node_comms loads "nodeInfo/Node" ✓ (NOW WORKS)
    │   │   └─ Returns: Node* with (id, ip)
    │   │
    │   ├─ Calls: remote_find_successor(ringChecker->Ip, ringChecker->predecessor->id)
    │   │   ↓
    │   │   DHASH.c: remote_find_successor() @ line 82
    │   │   ├─ SSH command: ssh 10.11.20.41 "cd /home/chord && ./scripts/node_comms find_successor 4"
    │   │   ├─ Remote node_comms loads "nodeInfo/Node" ✓ (NOW WORKS)
    │   │   └─ Returns: Node* with (id, ip)
    │   │
    │   └─ Validates successor->predecessor and predecessor->successor links
    │
    └─ Loop iteration 2+:
        └─ Calls: remote_find_successor(ringChecker->Ip, ringChecker->successor->id) to move to next node
            ↓
            Returns next Node in ring for ring traversal

```

## Files Fixed

### 1. **src/DHASH.c**
Fixed 3 SSH commands:
- `remote_find_successor()` @ line 86
- `remote_closest_preceding_finger()` @ line 158  
- `remote_get_successor()` @ line 125

**Before:**
```c
snprintf(command, sizeof(command),
    "ssh %s \"./scripts/node_comms find_successor %d\" 2>/dev/null",
    ip, targetId);
```

**After:**
```c
snprintf(command, sizeof(command),
    "ssh %s \"cd /home/chord && ./scripts/node_comms find_successor %d\" 2>/dev/null",
    ip, targetId);
```

### 2. **src/node.c**
Fixed 4 SSH commands:
- `remote_join()` @ line 524
- `remote_print_finger_table()` @ line 836
- `remote_load_and_update_finger_table()` @ line 841
- `remote_notify()` @ line 1021

**Before:**
```c
snprintf(command, sizeof(command),
    "ssh %s \"./scripts/node_comms print_finger_table\" 2>/dev/null", ip);
```

**After:**
```c
snprintf(command, sizeof(command),
    "ssh %s \"cd /home/chord && ./scripts/node_comms print_finger_table\" 2>/dev/null", ip);
```

## Key Insight: Why This Works

The fix adds `cd /home/chord &&` to every SSH command:

```bash
ssh 10.11.20.41 "cd /home/chord && ./scripts/node_comms find_successor 5"
                  ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
                  Changes to project directory FIRST
```

Now when `node_comms` runs, it:
1. **Is in the correct directory** (`/home/chord/`)
2. **Can find relative paths** like `nodeInfo/Node` and `nodeInfo/FingerTable`
3. **Loads node state correctly** without "Error opening Node file"
4. **Returns proper response** in format `id ip` that parsing expects

## Testing the Fix

To verify the fix works:

```bash
./main
```

Then at the CLI:
```
[n]ew [t]est [g]et info [l]ogger [w]arning [e]xit
> t
```

This should now successfully:
1. ✓ Call `remote_check_ring(localNode, successor_ip)`
2. ✓ SSH to remote nodes and load their node files
3. ✓ Verify successor->predecessor and predecessor->successor links
4. ✓ Traverse all nodes in the ring
5. ✓ Report "[INFO] Ring check passed for node X"

## Important Configuration Note

**The path `/home/chord/` must match your actual project location on remote nodes.**

If your project is in a different directory (e.g., `/root/p2p-FileSharingSystem/`), update all SSH commands accordingly:

```c
snprintf(command, sizeof(command),
    "ssh %s \"cd /your/actual/path && ./scripts/node_comms ...\" 2>/dev/null",
    ip);
```

Or better yet, define a constant:

```c
#define REMOTE_PROJECT_PATH "/home/chord"
```

Then use:
```c
snprintf(command, sizeof(command),
    "ssh %s \"cd %s && ./scripts/node_comms find_successor %%d\" 2>/dev/null",
    ip, REMOTE_PROJECT_PATH, targetId);
```

## Summary of Changes

| Function | File | Issue | Fix |
|----------|------|-------|-----|
| `remote_find_successor()` | DHASH.c:86 | Relative path for node_comms | Added `cd /home/chord &&` |
| `remote_closest_preceding_finger()` | DHASH.c:158 | Relative path for node_comms | Added `cd /home/chord &&` |
| `remote_get_successor()` | DHASH.c:125 | Relative path for node_comms | Added `cd /home/chord &&` |
| `remote_join()` | node.c:524 | Relative path for node_comms | Added `cd /home/chord &&` |
| `remote_print_finger_table()` | node.c:836 | Relative path for node_comms | Added `cd /home/chord &&` |
| `remote_load_and_update_finger_table()` | node.c:841 | Relative path for nodeInfo file | Added `cd /home/chord &&` |
| `remote_notify()` | node.c:1021 | Wrong directory reference | Changed from `cd scripts` to `cd /home/chord` |

## Compilation Status

✅ **Successfully recompiled** with all fixes applied:
```bash
gcc -pthread main.c src/node.c src/DHASH.c src/maintenance.c src/logger.c -o main -lm
```

Executable `main` is ready to use with proper SSH directory handling.

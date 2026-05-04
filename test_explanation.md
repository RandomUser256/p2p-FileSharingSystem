                                                                                        
  ---
  Test suite: tests/test_chord.c + tests/run_tests.sh                                                                                      
                                                                                                                                           
  The test binary talks to live nodes over TCP using the same wire protocol the nodes use — no project headers needed.                     
                                                                                                                                           
  Run                                                                                                                                    
                                                                                                                                           
  # From the project root, with nodes already running:                                                                                   
  ./tests/run_tests.sh 10.11.20.37 10.11.20.38 10.11.20.39                                                                                 
                                                                                                                                           
  # Include the UI responsiveness test:                                                                                                    
  ./tests/run_tests.sh 10.11.20.37 10.11.20.38 --ui                                                                                        
                                                                                                                                           
  ---
  What each test covers                                                                                                                    
                                                                                                                                         
  ┌─────────────────────┬────────────────────────────────────────────────────────────────────────────────────┐
  │        Test         │                                  What it verifies                                  │
  ├───────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────┤                   
  │ GET_NODE                      │ Node answers with correct 6-field NODE response                                    │

  Run

  # From the project root, with nodes already running:
  ./tests/run_tests.sh 10.11.20.37 10.11.20.38 10.11.20.39

  # Include the UI responsiveness test:
  ./tests/run_tests.sh 10.11.20.37 10.11.20.38 --ui

  ---
  What each test covers

  ┌────────────────────────────┬───────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │            Test            │                                           What it verifies                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ GET_NODE                   │ Node answers with correct 6-field NODE response                                                       │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ FIND_SUCCESSOR <id>        │ All 16 IDs return a valid NODE reply (tests handle_command + local find_successor)                    │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CLOSEST_PRECEDING_FINGER   │ Returns a valid node reference                                                                        │
  │ <id>                       │                                                                                                       │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ FIND_SUCCESSOR <id>        │ All 16 IDs return a valid NODE reply (tests handle_command + local find_successor)                    │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CLOSEST_PRECEDING_FINGER   │ Returns a valid node reference                                                                        │
  │ <id>                       │                                                                                                       │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ remote_find_predecessor    │ Simulates the hop-by-hop walk remote_find_predecessor() does internally; confirms the chain converges │
  │ walk                       │  and each CLOSEST_PRECEDING_FINGER response is coherent                                               │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CLOSEST_PRECEDING_FINGER   │ Returns a valid node reference                                                                        │
  │ <id>                       │                                                                                                       │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ remote_find_predecessor    │ Simulates the hop-by-hop walk remote_find_predecessor() does internally; confirms the chain converges │
  │ walk                       │  and each CLOSEST_PRECEDING_FINGER response is coherent                                               │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ remote_find_predecessor    │ Simulates the hop-by-hop walk remote_find_predecessor() does internally; confirms the chain converges │
  │ walk                       │  and each CLOSEST_PRECEDING_FINGER response is coherent                                               │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │                            │ thread blocked by maintenance                                                                         │
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │                            │ thread blocked by maintenance                                                                         │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │                            │ thread blocked by maintenance                                                                         │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  │                            │ if background threads freeze fgets()                                                                  │
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  │                            │ if background threads freeze fgets()                                                                  │
  └────────────────────────────┴───────────────────────────────────────────────────────────────────────────────────────────────────────┘
  │ <id>                       │                                                                                                       │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ remote_find_predecessor    │ Simulates the hop-by-hop walk remote_find_predecessor() does internally; confirms the chain converges │
  │ walk                       │  and each CLOSEST_PRECEDING_FINGER response is coherent                                               │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ remote_find_predecessor    │ Simulates the hop-by-hop walk remote_find_predecessor() does internally; confirms the chain converges │
  │ walk                       │  and each CLOSEST_PRECEDING_FINGER response is coherent                                               │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │                            │ thread blocked by maintenance                                                                         │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  │                            │ if background threads freeze fgets()                                                                  │
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │                            │ thread blocked by maintenance                                                                         │
  │                            │ thread blocked by maintenance                                                                         │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  ./tests/run_tests.sh 10.11.20.37 10.11.20.38 --ui

  ---
  What each test covers

  ┌────────────────────────────┬───────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │            Test            │                                           What it verifies                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ GET_NODE                   │ Node answers with correct 6-field NODE response                                                       │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ FIND_SUCCESSOR <id>        │ All 16 IDs return a valid NODE reply (tests handle_command + local find_successor)                    │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CLOSEST_PRECEDING_FINGER   │ Returns a valid node reference                                                                        │
  │ <id>                       │                                                                                                       │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ remote_find_predecessor    │ Simulates the hop-by-hop walk remote_find_predecessor() does internally; confirms the chain converges │
  │ walk                       │  and each CLOSEST_PRECEDING_FINGER response is coherent                                               │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ STABILIZE RPC              │ STABILIZE <ip> handler returns OK (exercises remote_notify path)                                      │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ CHECK_RING propagation     │ Fires ring check, waits 600 ms, confirms node is still alive                                          │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ full ring CHECK_RING       │ Replicates exactly what remote_check_ring() does; verifies every node in the list is still reachable  │
  │                            │ after the walk completes (multi-node only)                                                            │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: not blocking  │ Sends 10 GET_NODE requests at 100 ms intervals during active stabilize cycles; any timeout = server   │
  │                            │ thread blocked by maintenance                                                                         │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ maintenance: liveness      │ GET_NODE before and after 1 s sleep; failure = deadlock between maintenance mutex and server thread   │
  ├────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ UI responsiveness          │ Spawns ./main via fork/exec with piped stdin/stdout; sends g\ne\n; expects output within 5 s — fails  │
  │                            │ if background threads freeze fgets()                                                                  │
  └────────────────────────────┴───────────────────────────────────────────────────────────────────────────────────────────────────────┘

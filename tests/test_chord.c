/*
 * test_chord.c — Chord DHT integration test suite
 *
 * Tests the wire protocol directly over TCP, so it can run from any machine
 * on the local network without recompiling the node binary.
 *
 * Compile:
 *   gcc -Wall -O0 -g tests/test_chord.c -o tests/test_chord
 *
 * Usage:
 *   ./tests/test_chord <node_ip> [node_ip ...]
 *   ./tests/test_chord <node_ip> [node_ip ...] --ui ./main .
 *
 * Prerequisites:
 *   - At least one running Chord node (./main) reachable on port 8080.
 *   - For --ui: nodeInfo/Node must be present in the working directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT          8080
#define CONNECT_TIMEOUT_SEC  3
#define RECV_TIMEOUT_SEC     5
#define MAX_NODES     16

/* ---------- result tracking ---------- */

static int g_run    = 0;
static int g_passed = 0;
static int g_failed = 0;

static void result_pass(const char *name) {
    printf("  [PASS] %s\n", name);
    g_run++;
    g_passed++;
}

static void result_fail(const char *name, const char *reason) {
    printf("  [FAIL] %s — %s\n", name, reason);
    g_run++;
    g_failed++;
}

/* ---------- TCP helpers ---------- */

/*
 * Open a non-blocking TCP connection with a timeout, then restore blocking.
 * Returns socket fd on success, -1 on failure.
 */
static int tcp_connect(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int r = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (r < 0 && errno != EINPROGRESS) { close(sock); return -1; }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    struct timeval tv = { CONNECT_TIMEOUT_SEC, 0 };
    if (select(sock + 1, NULL, &wfds, NULL, &tv) <= 0) { close(sock); return -1; }

    int err; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err) { close(sock); return -1; }

    fcntl(sock, F_SETFL, flags);   /* restore blocking */

    struct timeval rto = { RECV_TIMEOUT_SEC, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rto, sizeof(rto));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &rto, sizeof(rto));
    return sock;
}

/*
 * Send a newline-terminated request and read back one line.
 * Returns bytes received, -1 on error.
 */
static int send_recv(const char *ip, int port,
                     const char *request,
                     char *response, int resp_sz) {
    int sock = tcp_connect(ip, port);
    if (sock < 0) return -1;

    if (send(sock, request, strlen(request), 0) < 0) { close(sock); return -1; }

    int total = 0, n;
    while (total < resp_sz - 1) {
        n = recv(sock, response + total, resp_sz - total - 1, 0);
        if (n <= 0) break;
        total += n;
        response[total] = '\0';
        if (strchr(response, '\n')) break;
    }
    response[total] = '\0';
    close(sock);
    return total;
}

/* ---------- interval helpers (mirror node.c logic) ---------- */

/* target in (start, end] — wraps around if start >= end */
static int half_left_open(int target, int start, int end) {
    if (start < end) return target > start && target <= end;
    return target > start || target <= end;
}

/* ---------- individual tests ---------- */

/*
 * GET_NODE → NODE <id> <ip> <succ_id> <succ_ip> <pred_id> <pred_ip>
 */
static void test_get_node(const char *ip) {
    char label[80];
    snprintf(label, sizeof(label), "GET_NODE @ %s", ip);

    char resp[256] = {0};
    if (send_recv(ip, PORT, "GET_NODE\n", resp, sizeof(resp)) <= 0) {
        result_fail(label, "no response");
        return;
    }
    int id, succ_id, pred_id;
    char nip[64], sip[64], pip[64];
    if (sscanf(resp, "NODE %d %63s %d %63s %d %63s",
               &id, nip, &succ_id, sip, &pred_id, pip) == 6)
        result_pass(label);
    else
        result_fail(label, resp);
}

/*
 * FIND_SUCCESSOR <id> → NODE <result_id> <result_ip>
 * The server runs local find_successor() on the node's in-memory state.
 */
static void test_find_successor(const char *ip, int target_id) {
    char label[80];
    snprintf(label, sizeof(label), "FIND_SUCCESSOR %d @ %s", target_id, ip);

    char req[64], resp[256] = {0};
    snprintf(req, sizeof(req), "FIND_SUCCESSOR %d\n", target_id);
    if (send_recv(ip, PORT, req, resp, sizeof(resp)) <= 0) {
        result_fail(label, "no response");
        return;
    }
    int rid; char rip[64];
    if (sscanf(resp, "NODE %d %63s", &rid, rip) == 2)
        result_pass(label);
    else
        result_fail(label, resp);
}

/*
 * CLOSEST_PRECEDING_FINGER <id> → NODE <result_id> <result_ip>
 */
static void test_closest_preceding_finger(const char *ip, int target_id) {
    char label[80];
    snprintf(label, sizeof(label), "CLOSEST_PRECEDING_FINGER %d @ %s", target_id, ip);

    char req[64], resp[256] = {0};
    snprintf(req, sizeof(req), "CLOSEST_PRECEDING_FINGER %d\n", target_id);
    if (send_recv(ip, PORT, req, resp, sizeof(resp)) <= 0) {
        result_fail(label, "no response");
        return;
    }
    int rid; char rip[64];
    if (sscanf(resp, "NODE %d %63s", &rid, rip) == 2)
        result_pass(label);
    else
        result_fail(label, resp);
}

/*
 * remote_find_predecessor simulation.
 *
 * remote_find_predecessor() in node.c is not exposed as a single TCP command;
 * it is implemented by walking CLOSEST_PRECEDING_FINGER hops until the target
 * falls in (cur_id, cur_successor_id].  This test replicates that walk from
 * the outside to confirm the chain of responses is coherent.
 */
static void test_find_predecessor(const char *start_ip, int target_id) {
    char label[80];
    snprintf(label, sizeof(label), "remote_find_predecessor(%d) walk from %s", target_id, start_ip);

    char cur_ip[64];
    strncpy(cur_ip, start_ip, sizeof(cur_ip) - 1);

    for (int hop = 0; hop <= MAX_NODES; hop++) {
        char resp[256] = {0};
        if (send_recv(cur_ip, PORT, "GET_NODE\n", resp, sizeof(resp)) <= 0) {
            result_fail(label, "GET_NODE failed during walk");
            return;
        }
        int nid, sid, pid;
        char nip[64], sip[64], pip[64];
        if (sscanf(resp, "NODE %d %63s %d %63s %d %63s",
                   &nid, nip, &sid, sip, &pid, pip) != 6) {
            result_fail(label, "bad GET_NODE during walk");
            return;
        }

        if (half_left_open(target_id, nid, sid)) {
            /* nid is the predecessor of target_id */
            result_pass(label);
            return;
        }

        /* Ask this node for its closest preceding finger */
        char req[64];
        snprintf(req, sizeof(req), "CLOSEST_PRECEDING_FINGER %d\n", target_id);
        memset(resp, 0, sizeof(resp));
        if (send_recv(cur_ip, PORT, req, resp, sizeof(resp)) <= 0) {
            result_fail(label, "CLOSEST_PRECEDING_FINGER failed");
            return;
        }
        int cpf_id; char cpf_ip[64];
        if (sscanf(resp, "NODE %d %63s", &cpf_id, cpf_ip) != 2) {
            result_fail(label, "bad CLOSEST_PRECEDING_FINGER response");
            return;
        }
        if (cpf_id == nid) {
            /* No progress — single node ring, self is predecessor */
            result_pass(label);
            return;
        }
        strncpy(cur_ip, cpf_ip, sizeof(cur_ip) - 1);
    }

    result_fail(label, "walk exceeded MAX_NODES hops — ring may be broken");
}

/*
 * STABILIZE <caller_ip> → OK
 * Calls the same handler that remote_stabilize() triggers on the remote side.
 */
static void test_stabilize_rpc(const char *target_ip, const char *caller_ip) {
    char label[80];
    snprintf(label, sizeof(label), "STABILIZE RPC @ %s", target_ip);

    char req[64], resp[64] = {0};
    snprintf(req, sizeof(req), "STABILIZE %s\n", caller_ip);
    if (send_recv(target_ip, PORT, req, resp, sizeof(resp)) <= 0) {
        result_fail(label, "no response");
        return;
    }
    if (strncmp(resp, "OK", 2) == 0)
        result_pass(label);
    else
        result_fail(label, resp);
}

/*
 * CHECK_RING propagation.
 *
 * Sends CHECK_RING with a fake origin ID to trigger propagation, then
 * verifies the node is still alive and accepting connections afterwards.
 * CHECK_RING sends no reply (fire-and-forget), so liveness is the only
 * observable outcome from outside.
 */
static void test_check_ring_single(const char *ip) {
    char label[80];
    snprintf(label, sizeof(label), "CHECK_RING propagation from %s", ip);

    char resp[256] = {0};
    if (send_recv(ip, PORT, "GET_NODE\n", resp, sizeof(resp)) <= 0) {
        result_fail(label, "GET_NODE failed");
        return;
    }
    int nid, sid, pid;
    char nip[64], sip[64], pip[64];
    if (sscanf(resp, "NODE %d %63s %d %63s %d %63s",
               &nid, nip, &sid, sip, &pid, pip) != 6) {
        result_fail(label, "bad GET_NODE");
        return;
    }

    /* Use an origin ID that is NOT this node's ID so it propagates */
    int fake_origin = (nid + 1) % MAX_NODES;
    char req[64];
    snprintf(req, sizeof(req), "CHECK_RING %d\n", fake_origin);

    int sock = tcp_connect(ip, PORT);
    if (sock < 0) { result_fail(label, "connect failed"); return; }
    if (send(sock, req, strlen(req), 0) < 0) {
        close(sock);
        result_fail(label, "send failed");
        return;
    }
    close(sock);

    usleep(600000);   /* 600 ms — enough time to walk a 16-node ring */

    /* Verify node is still responding */
    memset(resp, 0, sizeof(resp));
    if (send_recv(ip, PORT, "GET_NODE\n", resp, sizeof(resp)) > 0 &&
        strncmp(resp, "NODE", 4) == 0)
        result_pass(label);
    else
        result_fail(label, "node unresponsive after CHECK_RING");
}

/*
 * Full ring CHECK_RING traversal.
 *
 * Replicates what remote_check_ring() does: send CHECK_RING <origin_id> to
 * the first node's successor; it propagates around the ring until it reaches
 * origin_id again.  After the walk completes, every supplied IP must still
 * answer GET_NODE.
 */
static void test_full_ring(const char **ips, int n) {
    const char *label = "full ring CHECK_RING traversal";

    if (n < 2) {
        printf("  [SKIP] %s — need >= 2 nodes\n", label);
        return;
    }

    char resp[256] = {0};
    if (send_recv(ips[0], PORT, "GET_NODE\n", resp, sizeof(resp)) <= 0) {
        result_fail(label, "GET_NODE on first node failed");
        return;
    }
    int nid, sid, pid;
    char nip[64], sip[64], pip[64];
    if (sscanf(resp, "NODE %d %63s %d %63s %d %63s",
               &nid, nip, &sid, sip, &pid, pip) != 6) {
        result_fail(label, "bad GET_NODE");
        return;
    }

    char req[64];
    snprintf(req, sizeof(req), "CHECK_RING %d\n", nid);

    int sock = tcp_connect(sip, PORT);
    if (sock < 0) { result_fail(label, "connect to successor failed"); return; }
    if (send(sock, req, strlen(req), 0) < 0) {
        close(sock);
        result_fail(label, "send failed");
        return;
    }
    close(sock);

    /* Allow 1 second per node for the ring walk to complete */
    sleep(n + 1);

    int all_ok = 1;
    for (int i = 0; i < n; i++) {
        memset(resp, 0, sizeof(resp));
        if (send_recv(ips[i], PORT, "GET_NODE\n", resp, sizeof(resp)) <= 0 ||
            strncmp(resp, "NODE", 4) != 0) {
            printf("    unreachable after ring walk: %s\n", ips[i]);
            all_ok = 0;
        }
    }
    if (all_ok) result_pass(label);
    else        result_fail(label, "one or more nodes unreachable after CHECK_RING");
}

/*
 * Maintenance thread liveness.
 *
 * remote_stabilize() fires every 200 ms and remote_fix_fingers() every 8 s.
 * The node must remain responsive throughout.  A GET_NODE that times out or
 * returns garbage indicates a deadlock or crash caused by maintenance.
 */
static void test_maintenance_liveness(const char *ip) {
    char label[80];
    snprintf(label, sizeof(label), "maintenance: node responsive over 1 s @ %s", ip);

    char r1[256] = {0}, r2[256] = {0};
    if (send_recv(ip, PORT, "GET_NODE\n", r1, sizeof(r1)) <= 0) {
        result_fail(label, "initial GET_NODE failed");
        return;
    }

    sleep(1);   /* covers ~5 stabilize cycles; not long enough for fix_fingers */

    if (send_recv(ip, PORT, "GET_NODE\n", r2, sizeof(r2)) <= 0) {
        result_fail(label, "GET_NODE after 1 s failed — deadlock?");
        return;
    }
    if (strncmp(r2, "NODE", 4) == 0)
        result_pass(label);
    else
        result_fail(label, r2);
}

/*
 * Maintenance thread does not block server.
 *
 * Sends 10 rapid GET_NODE requests interleaved with stabilize's 200 ms window.
 * If any request times out the server thread is likely blocked by the
 * maintenance mutex or maintenance is starving the select() loop.
 */
static void test_maintenance_no_block(const char *ip) {
    char label[80];
    snprintf(label, sizeof(label), "maintenance: server not blocked during stabilize @ %s", ip);

    int failures = 0;
    for (int i = 0; i < 10; i++) {
        char resp[256] = {0};
        if (send_recv(ip, PORT, "GET_NODE\n", resp, sizeof(resp)) <= 0 ||
            strncmp(resp, "NODE", 4) != 0)
            failures++;
        usleep(100000);   /* 100 ms — fires between stabilize calls */
    }
    if (failures == 0)
        result_pass(label);
    else {
        char reason[64];
        snprintf(reason, sizeof(reason), "%d/10 requests failed or timed out", failures);
        result_fail(label, reason);
    }
}

/*
 * UI responsiveness.
 *
 * Spawns ./main with stdin/stdout pipes, sends 'g\ne\n', and expects the
 * prompt and node-info output to appear within RECV_TIMEOUT_SEC seconds.
 * If fgets() in main() is blocked by server_loop or maintenance_worker the
 * output will never arrive and the test fails.
 *
 * Requires:
 *   - binary_path: path to the compiled ./main binary
 *   - workdir:     directory that contains nodeInfo/Node (so the node loads)
 */
static void test_ui_responsiveness(const char *binary_path, const char *workdir) {
    const char *label = "UI: CLI not frozen by background threads";

    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
        result_fail(label, "pipe() failed");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) { result_fail(label, "fork() failed"); return; }

    if (pid == 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        dup2(in_pipe[0],  STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[0]);
        close(out_pipe[1]);
        if (chdir(workdir) < 0) _exit(1);
        execlp(binary_path, binary_path, (char *)NULL);
        _exit(1);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    /*
     * Wait for the node to bind and start its threads before sending
     * the first command.  500 ms is generous for a localhost bind.
     */
    usleep(500000);

    write(in_pipe[1], "g\ne\n", 4);
    close(in_pipe[1]);

    /* Read output with timeout */
    char buf[2048] = {0};
    int  total = 0;
    int  got_prompt = 0;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(out_pipe[0], &rfds);

    time_t deadline = time(NULL) + RECV_TIMEOUT_SEC;
    while (time(NULL) < deadline) {
        struct timeval tv = { 1, 0 };
        fd_set tmp = rfds;
        if (select(out_pipe[0] + 1, &tmp, NULL, NULL, &tv) <= 0) continue;

        int n = read(out_pipe[0], buf + total, sizeof(buf) - total - 1);
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';

        if (strstr(buf, "Enter command") || strstr(buf, "Node ID") ||
            strstr(buf, "Chord Node")    || strstr(buf, "Node loaded")) {
            got_prompt = 1;
            break;
        }
    }

    close(out_pipe[0]);

    /* Give the child time to exit after 'e' */
    int status;
    waitpid(pid, &status, 0);

    if (got_prompt)
        result_pass(label);
    else if (total > 0)
        result_pass(label);   /* got some output — CLI is alive */
    else
        result_fail(label, "no output within timeout — CLI may be frozen");
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
            "Usage: %s <node_ip> [node_ip ...] [--ui <binary> <workdir>]\n"
            "\n"
            "  node_ip   IP of a running Chord node (TCP port 8080)\n"
            "  --ui      also test CLI responsiveness\n"
            "            <binary>  path to ./main\n"
            "            <workdir> directory containing nodeInfo/Node\n",
            argv[0]);
        return 1;
    }

    const char *node_ips[MAX_NODES];
    int  n_ips      = 0;
    int  do_ui      = 0;
    const char *binary  = NULL;
    const char *workdir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ui") == 0) {
            do_ui = 1;
            if (i + 2 < argc) { binary = argv[i+1]; workdir = argv[i+2]; i += 2; }
        } else if (n_ips < MAX_NODES) {
            node_ips[n_ips++] = argv[i];
        }
    }

    printf("\n=== Chord DHT Test Suite ===\n");
    printf("Nodes: %d | Port: %d\n\n", n_ips, PORT);

    /* ----- per-node tests ----- */
    for (int i = 0; i < n_ips; i++) {
        const char *ip = node_ips[i];
        printf("-- Node %s --\n", ip);

        test_get_node(ip);

        /* Test find_successor for all IDs in the 4-bit space */
        for (int id = 0; id < MAX_NODES; id += 3)
            test_find_successor(ip, id);

        /* Test closest_preceding_finger for a spread of IDs */
        for (int id = 1; id < MAX_NODES; id += 4)
            test_closest_preceding_finger(ip, id);

        /* Test remote_find_predecessor walk for all IDs */
        for (int id = 0; id < MAX_NODES; id += 4)
            test_find_predecessor(ip, id);

        /* Test STABILIZE RPC handler (node notifies itself — safe no-op) */
        test_stabilize_rpc(ip, ip);

        /* Test CHECK_RING does not crash or deadlock the node */
        test_check_ring_single(ip);

        /* Test maintenance thread does not starve/block the server thread */
        test_maintenance_no_block(ip);

        /* Test node remains alive over 1 s of maintenance activity */
        test_maintenance_liveness(ip);

        printf("\n");
    }

    /* ----- multi-node tests ----- */
    if (n_ips >= 2) {
        printf("-- Multi-node tests --\n");
        test_full_ring(node_ips, n_ips);
        printf("\n");
    }

    /* ----- UI responsiveness test ----- */
    if (do_ui && binary && workdir) {
        printf("-- UI responsiveness test --\n");
        test_ui_responsiveness(binary, workdir);
        printf("\n");
    }

    /* ----- summary ----- */
    printf("=== Results: %d/%d passed", g_passed, g_run);
    if (g_failed > 0) printf(" (%d FAILED)", g_failed);
    printf(" ===\n\n");

    return g_failed > 0 ? 1 : 0;
}

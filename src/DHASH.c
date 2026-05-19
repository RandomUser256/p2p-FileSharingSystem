#include "DHASH.h"
#include "logger.h"


#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sha1.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
Notes:
    - Problems with split files, only saw first chunk be shared.
*/

char* toUpper_hexadecimal(const char* hexadecimal) {
    size_t len = strlen(hexadecimal);
    char* s_up = malloc(len + 1);
    if (!s_up) return NULL;

    for (size_t i = 0; i < len; i++) {
        s_up[i] = toupper((unsigned char)hexadecimal[i]);
    }
    s_up[len] = '\0'; 
    return s_up;
}

int hexadecimal_to_decimal(const char* hexadecimal) {
    char hexDigits[] = "0123456789ABCDEF";
    int decimalNumber = 0;
    int power = 0;

    char* upper_hex = toUpper_hexadecimal(hexadecimal);
    if (!upper_hex) return -1;

    for (int i = (int)strlen(upper_hex) - 1; i >= 0; i--) {
        for (int j = 0; j < 16; j++) {
            if (upper_hex[i] == hexDigits[j]) {
                decimalNumber += j * (int)pow(16, power);
                break;
            }
        }
        power++;
    }

    free(upper_hex); // Clean up the heap memory
    return decimalNumber;
}

// DISCLAIMER: In this function change arguments to incorporate appropriate hashing of files
int hash_node_identifier(const char* Ip, const char* port) {
    char nodeIdentifier[64]; 

    snprintf(nodeIdentifier, sizeof(nodeIdentifier), "%s:%s", Ip, port); 

    char result[64];
    char hexresult[41];
    size_t offset;

    SHA1(result, nodeIdentifier, strlen(nodeIdentifier));

    for( offset = 0; offset < 20; offset++) {
        sprintf( ( hexresult + (2*offset)), "%02x", result[offset]&0xff);
    }

    int resultId = hexadecimal_to_decimal(hexresult);

    return resultId;
}

int hash_file_node(const char* fileName) {
    char result[64];
    char hexresult[41];
    size_t offset;

    SHA1(result, fileName, strlen(fileName));

    for( offset = 0; offset < 20; offset++) {
        sprintf( ( hexresult + (2*offset)), "%02x", result[offset]&0xff);
    }

    int resultId = hexadecimal_to_decimal(hexresult);

    return resultId;
}

/*
Node* lookup(const char* fileName) {
    //Hash the identifier to get the corresponding node ID
    
    int targetNode = hash_file_node(fileName);

    
}
    */

//Looks at all possible locations for a fileChunk 
// COPIED FROM LOADFINGERTABLE, MODIFY FUNCTION
/*
void checkFileLocation(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        log_warn("[WARNING] cannot open file %s for reading, finger table will use defaults\n", filepath);
        return;
    }

    char line[512];
    int entry_idx = 0;

    //Loops through every line in the text file and updates the fingerTable
    while (fgets(line, sizeof(line), file) && entry_idx < NODE_ID_LENGTH) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        int idx, start, lower, upper, succ_id;
        char succ_ip[MAX_IP_LENGTH] = {0};

        int parsed = sscanf(line, 
                           "entry=%d,start=%d,lower=%d,upper=%d,successor_id=%d,successor_ip=%15s",
                           &idx, &start, &lower, &upper, &succ_id, succ_ip);

        if (parsed != 6) {
            log_warn("[WARNING] skipping malformed line: %s\n", line);
            continue;
        }

        // Validate entry index
        if (idx < 0 || idx >= NODE_ID_LENGTH) {
            log_warn("[WARNING] invalid entry index %d, skipping\n", idx);
            continue;
        }

        node->fingerTable[idx].start = start;
        node->fingerTable[idx].lowerIntervalLimit = lower;
        node->fingerTable[idx].upperIntervalLimit = upper;

        // Update successor reference
        //Checks if successor ID and IP are valid before updating the finger table entry
        if (succ_id != -1 && strcmp(succ_ip, "NONE") != 0) {
            // Reuse existing successor if IDs match, otherwise create new node
            if (node->fingerTable[idx].successor == NULL || 
                node->fingerTable[idx].successor->id != succ_id) {
                if (node->fingerTable[idx].successor != NULL && 
                    node->fingerTable[idx].successor != node) {
                    freeNode(node->fingerTable[idx].successor);
                }
                node->fingerTable[idx].successor = createNode(succ_id, succ_ip, "");
            }
            strncpy(node->fingerTable[idx].Ip, succ_ip, MAX_IP_LENGTH - 1);
            node->fingerTable[idx].Ip[MAX_IP_LENGTH - 1] = '\0';
        } else {
            // No valid successor, point to self
            node->fingerTable[idx].successor = node;
            node->fingerTable[idx].Ip[0] = '\0';
        }

        entry_idx++;
    }

    fclose(file);
    log_info("[INFO] Finger table for node %d loaded from %s\n", node->id, filepath);
}
    */


// Transfers a local file to a remote node's fileContentPath over TCP.
// Returns 0 on success, -1 on failure.
static int tcp_send_file(int port, const char *dest_ip,
                          const char *dest_filename, const char *local_path) {
    FILE *f = fopen(local_path, "rb");
    if (!f) {
        log_error("[STORE_FILE] cannot open local file %s", local_path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    int sock = init_socket(dest_ip, port);
    if (sock < 0) {
        log_error("[STORE_FILE] cannot connect to %s:%d", dest_ip, port);
        fclose(f);
        return -1;
    }

    // Send header with filename and filesize
    char header[512];
    snprintf(header, sizeof(header), "STORE_FILE %s %ld\n", dest_filename, filesize);
    if (send(sock, header, strlen(header), 0) < 0) {
        log_error("[STORE_FILE] send header failed");
        fclose(f); close(sock);
        return -1;
    }

    // Wait for READY before sending bytes
    char resp[64];
    int n = recv(sock, resp, sizeof(resp) - 1, 0);
    if (n <= 0 || strncmp(resp, "READY", 5) != 0) {
        resp[n > 0 ? n : 0] = '\0';
        log_error("[STORE_FILE] expected READY, got: %s", resp);
        fclose(f); close(sock);
        return -1;
    }

    // Send file bytes
    char buf[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (send(sock, buf, bytes_read, 0) < 0) {
            log_error("[STORE_FILE] send data failed");
            fclose(f); close(sock);
            return -1;
        }
    }
    fclose(f);

    // Wait for OK / ERROR
    n = recv(sock, resp, sizeof(resp) - 1, 0);
    close(sock);
    if (n <= 0) return -1;
    resp[n] = '\0';
    return (strncmp(resp, "OK", 2) == 0) ? 0 : -1;
}

char* generateDestinationFilePath(Node* destinationNode, const char* identifier) {
    static char destinationFilePath[MAX_FILE_PATH_LENGTH];
    int written = snprintf(destinationFilePath, MAX_FILE_PATH_LENGTH,
                           "%s/%s",
                           destinationNode->fileContentPath,
                           identifier);
    if (written < 0 || written >= MAX_FILE_PATH_LENGTH) {
        log_error("Error: Destination file path exceeds maximum length.\n");
        return NULL;
    }
    return destinationFilePath;
}

//MISSING ALL FILE HAN+DLING LOGIC
//IF TARGET ID IS NOT LOCAL NODE, MUST DELETE FILE AFTER
//Insert a file from the host node to the corresponding node for the given file identifier
void insert(t_server* s, const char* filename) {
    //Hash the identifier and find the node responsible for the given identifier
    //Node* destinationNode1 = lookup(hostNode, identifier);

    int destinationId = hash_file_node(filename);

    pthread_mutex_lock(&s->lock);
    int port = s->port;
    pthread_mutex_unlock(&s->lock);

    int redundancyCounter = 0;

    int loopDestinationId = destinationId;

    //redundancyCounter check to store information in set amount of nodes
    //Currently stores in 2 different nodes
    while (redundancyCounter < 2) {
        pthread_mutex_lock(&s->lock);
        if (s->localNode->successor == NULL) {
            break;
        }

        int localId = s->localNode->id;
        pthread_mutex_unlock(&s->lock);

        if (loopDestinationId == localId) {
            if (loopDestinationId >= 1023) {
                loopDestinationId = 0;
            } else {
                loopDestinationId++;
            }
            break;
        }

        Node* destinationNode = remote_find_successor(s, port, loopDestinationId);

        pthread_mutex_lock(&s->lock);
        if (destinationNode == s->localNode) {
            loopDestinationId = s->localNode->successor->id;
            pthread_mutex_unlock(&s->lock);

            redundancyCounter++;
            continue;
        }
        pthread_mutex_unlock(&s->lock);
        
        if (destinationNode == NULL) {
            log_warn("Invalid node for insert operation at ID:%d", loopDestinationId);

            //Provisional solution: Increments id until it finds a valid successor 
            //Review to see if there is better alternative in case of failure
            if (loopDestinationId >= 1023) {
                loopDestinationId = 0;
            } else {
                loopDestinationId++;
            }

            continue;
        }

        char* fileDirectory = generateDestinationFilePath(destinationNode, filename);

        // Transfer the file using the existing TCP connection instead of scp,
        // avoiding user-permission issues across machines.
        int result = tcp_send_file(port, destinationNode->Ip, filename, fileDirectory);

        if (result == 0) {
            //Success
            printf("File %s inserted successfully at node ID: %d IP: %s\n", filename ,destinationNode->id, destinationNode->Ip);

            //REMOVE - DOES NOT ENSURE ACTUAL REMOTE COPIES - JUST FOR DEBUG PURPOSES
            redundancyCounter++;
        } else {
            printf("File insert action not able to execute at node ID: %d IP: %s\n", destinationNode->id, destinationNode->Ip);
            continue;
        }

        if (destinationNode->successor != NULL) {
            //Inserts file in additional successors for redundancy
            Node* nextSuccessor = remote_get_node(s, port, destinationNode->successor->Ip);

            if (nextSuccessor != NULL) {
                loopDestinationId = nextSuccessor->id;
                //redundancyCounter++;
            }
        }
        else {
            break;
        }
        
    }
}

//Parses registry information into a ChunkEntry object
static int parse_chunk_entry(const char *s, ChunkEntry *c) {
    memset(c, 0, sizeof(*c));

    const char *eq = strchr(s, '=');
    if (!eq || eq == s) return 0;
    size_t nlen = (size_t)(eq - s);
    if (nlen >= sizeof(c->name)) return 0;
    memcpy(c->name, s, nlen);

    const char *p = eq + 1;
    if (*p++ != '[') return 0;

    /* parse IDs */
    c->replica_count = 0;
    while (*p && *p != ']') {
        char *end;
        long id = strtol(p, &end, 10);
        if (end == p) break;
        if (c->replica_count < MAX_REPLICAS)
            c->node_ids[c->replica_count++] = (int)id;
        p = (*end == ',') ? end + 1 : end;
    }
    
    if (*p++ != ']') return 0;
    if (*p++ != ':') return 0;
    if (*p++ != '[') return 0;

    /* parse IPs */
    int ip_idx = 0;
    while (*p && *p != ']') {
        const char *start = p;
        while (*p && *p != ',' && *p != ']') p++;
        size_t len = (size_t)(p - start);
        if (ip_idx < MAX_REPLICAS) {
            size_t cl = len < (MAX_IP_LENGTH - 1) ? len : (MAX_IP_LENGTH - 1);
            memcpy(c->node_ips[ip_idx], start, cl);
            ip_idx++;
        }
        if (*p == ',') p++;
    }
    /* make replica_count consistent with IP count */
    if (ip_idx < c->replica_count) c->replica_count = ip_idx;
    return 1;
}

//Copies registry information into provided FileEntry object
static int parse_registry_line(const char * line, FileEntry* entry) {
    memset(entry, 0, sizeof(*entry));

    const char *comma = strchr(line, ',');
    //Skips if line if it does not contain a comma, means its either empty or does not have a filename
    if (!comma) return 0;
    //Distancia entre inicio de linea y la coma
    size_t nlen = (size_t)(comma - line);
    //Invalid comma positions, either no filename or it chekcs if the entire line is only filename+comma
    if (nlen == 0 || nlen >= sizeof(entry->original_name)) return 0;

    //Copies filename to FileEntry
    memcpy(entry->original_name, line, nlen);

    const char *p = comma + 1;
    while (*p) {
        while (*p == ' ') p++;

        //Invalid file if first line is space
        if (!*p) break;

        /* a chunk token ends at the ']' closing the IP list;
         * find the "]:[" separator to locate that closing ']' */
        const char *id_close = strstr(p, "]:[");
        if (!id_close) break;
        const char *ip_close = strchr(id_close + 3, ']');
        if (!ip_close) break;

        size_t tlen = (size_t)(ip_close - p) + 1;
        char tok[512];
        if (tlen < sizeof(tok)) {
            memcpy(tok, p, tlen);
            tok[tlen] = '\0';
            if (entry->chunk_count < MAX_CHUNKS_PER_FILE)
                if (parse_chunk_entry(tok, &entry->chunks[entry->chunk_count]))
                    entry->chunk_count++;
        }
        p = ip_close + 1;
        if (*p == ',') p++;
    }
    printf("Line parsing completed for file %s\n", entry->original_name);

    return entry->chunk_count > 0 ? 1 : 0;
}

int load_registry(FileEntry* entries, int max_entries, int *out_count) {
    *out_count = 0;

    FILE *f = fopen(REGISTRY_PATH, "r");
    if (!f) {
        printf("[LOAD_REGISTRY] unable to open %s\n", REGISTRY_PATH);
        return -1;
    }
    
    char line[4096];
    while (fgets(line, sizeof(line), f) && *out_count < max_entries) {
        size_t l = strlen(line);

        //Adds null terminator to each line if it reaches the end of a text line (noted by \n or \r)
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';

        if (parse_registry_line(line, &entries[*out_count])) {
            (*out_count)++;
        }
    }
    fclose(f);

    printf("Loading registry complete\n");

    return 0;
}

int save_registry(const FileEntry * entries, int count) {
    FILE *f = fopen(REGISTRY_PATH, "w");
    if (!f) {
        printf("[SAVE_REGISTRY] unable to open %s", REGISTRY_PATH);
        return -1;
    }

    fprintf(f, "# File directory registry\n\n");
    for (int i=0; i < count; i++) {
        fprintf(f, "%s,", entries[i].original_name);
        for (int j=0; j<entries[i].chunk_count; j++) {
            const ChunkEntry *c = &entries[i].chunks[j];

            fprintf(f, "%s%s=[", j > 0 ? " " : "", c->name);

            for (int k = 0; k < c->replica_count; k++) {
                //Comma after the first nodeId
                if (k) fprintf(f, ",");
                fprintf(f, "%d", c->node_ids[k]);
            }
            fprintf(f, "]:[");
            for (int k = 0; k < c->replica_count; k++) {
                //Comma after the first nodeIp
                if (k) fprintf(f, ",");
                fprintf(f, "%s", c->node_ips[k]);
            }
            fprintf(f, "]");
        }
        fprintf(f, "\n");
    }

    fclose(f);
    return 0;
}

//Adds or updates an FileEntry object from another FileEntry object 
void upsert_file_entry(FileEntry *entries, int* count, int max_entries, const FileEntry* entry) {
    for (int i=0; i < *count; i++) {
        if (strcmp(entries[i].original_name, entry->original_name) == 0) {
            entries[i] = *entry;
            return;
        }
    }
    if (*count < max_entries) {
        entries[(*count)++] = *entry;
    }
}

//Generates names of all file chunks. Follows the same naming scheme than File_utilities/splitter.sh
static int derive_chunk_names(const char *filepath, int num_parts, char names[][256], int *chunk_count) {
    const char* slash = strrchr(filepath, '/');
    const char* basename = slash ? slash + 1 : filepath;
    const char* dot = strrchr(basename, '.');

    char base[256] = {0};
    char ext[64] = {0};
    if (dot) {
        size_t blen = (size_t)(dot - basename);
        if (blen >= sizeof(base)) return -1;
        memcpy(base, basename, blen);
        strncpy(ext, dot + 1, sizeof(ext) - 1);
    } else {
        strncpy(base, basename, sizeof(base) - 1);
    }

    *chunk_count = 0;
    for (int i=0; i< num_parts && i < MAX_CHUNKS_PER_FILE; i++) {
        char c1 = (char)('a' + i/26);
        char c2 = (char)('a' + i % 26);
        if (dot) {
            snprintf(names[i], 256, "%.248s%c%c.%.4s", base, c1, c2, ext);
        } else {
            snprintf(names[i], 256, "%.253s%c%c", base, c1, c2);
        }
        (*chunk_count)++;
    }

    return 0;
}

static int split_file(const char* filepath, int num_parts, char chunk_names[][256], int* chunk_count) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "bash File_utilities/splitter.sh \"%s\" %d", filepath, num_parts);
    if (system(cmd) != 0) {
        printf("[split_file] splitter.sh failed for %s", filepath);
        return -1;
    }
    return derive_chunk_names(filepath, num_parts, chunk_names, chunk_count);
}

/*
Protocol:
    client (local machine) -> FETCH_FILE sends file name of requested file
    server -> Returns 'FILE <size>' then returns raw file content
*/
int tcp_fetch_file(int port, const char* sourceIp, const char* source_filename, const char* storage_directory) {
    int sock = init_socket(sourceIp, port);
    if (sock < 0)  {
        printf("[FETCH_FILE] Could not establish connection with %s:%d", sourceIp, port);
        return -1;
    }

    char request[512];
    //Calls fetch process that returns the contents of the filename
    snprintf(request, sizeof(request), "FETCH_FILE %s\n", source_filename);
    if (send(sock, request, strlen(request), 0) < 0) {
        close(sock);
        return -1;
    }

    char response[128] = {0};
    int pos = 0;
    //Waits fore READY response
    while (pos < (int)sizeof(response) -1) {
        int n = recv(sock, response + pos, 1, 0);
        if (n <= 0) { 
            close(sock);
            return -1;
        }

        if (response[pos] == '\n') {
            response[pos] = '\0';
            break;
        }

        pos++;
    }

    if (strncmp(response, "ERROR", 5) == 0) {
        printf("%s\n", response);
        printf("[FETCH_FILE] file not found in remote node %s:%d\n", sourceIp, port);
        close(sock);
        return -1;
    }

    //Verify integrity of file size indicator Header
    long filesize = 0;
    if (sscanf(response, "FILE %ld", &filesize) != 1 || filesize < 0) {
        printf("[FETCH_FILE] Bad header received from %s:%d\n", sourceIp, port);
        close(sock);
        return -1;
    }

    FILE *f = fopen(storage_directory, "wb");

    long received = 0;
    char buf[4096];
    while (received < filesize) {
        long remaining = filesize - received;
        //Determine size of buffer to read next
        int to_read = (remaining < (long)sizeof(buf)) ? (int)remaining : (int)sizeof(buf);
        int n = recv(sock, buf, to_read, 0);

        if (n<=0) break;
        //Writes to file one bit at a time
        fwrite(buf, 1, n, f);
        received += n;
    }
    fclose(f);
    close(sock);

    if (received != filesize) {
        printf("[FETCH_FILE] incomplete file: got %ld of %ld bytes for %s", received, filesize, source_filename);
        return -1;
    }

    return 0;
}

static int tcp_send_registry_to(int port, const char *dest_ip, const char* content, long size) {
    int sock = init_socket(dest_ip, port);
    if (sock < 0) return -1;

    char header[64];
    snprintf(header, sizeof(header), "UPDATE_REGISTRY %ld\n", size);

    if (send(sock, header, strlen(header), 0) < 0) {
        close(sock);
        return -1;
    }

    char response[64];
    int pos = 0;

    //Waits fore READY response
    while (pos < (int)sizeof(response) -1) {
        int n = recv(sock, response + pos, 1, 0);
        if (n <= 0) { 
            close(sock);
            return -1;
        }

        if (response[pos] == '\n') {
            response[pos] = '\0';
            break;
        }

        pos++;
    }

    if (strncmp(response, "READY", 5) != 0) {
        close(sock);
        return -1;
    }

    //Sends content to connected socket, confirms success of operation
    if (send(sock, content, (size_t)size, 0) < 0) {
        close(sock);

        return -1;
    }

    pos = 0;
    memset(response, 0, sizeof(response));
    //Waits fore OK response
    while (pos < (int)sizeof(response) -1) {
        int n = recv(sock, response + pos, 1, 0);
        if (n <= 0) { 
            close(sock);
            return -1;
        }

        if (response[pos] == '\n') {
            response[pos] = '\0';
            break;
        }

        pos++;
    }
    close(sock);

    return (strncmp(response, "OK", 2) == 0) ? 0 : -1;
}

int broadcast_registry(t_server *s, int port) {
    FILE *f = fopen(REGISTRY_PATH, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc((size_t)size + 1);
    if (!content) { fclose(f); return -1; }
    fread(content, 1, (size_t)size, f);
    content[size] = '\0';
    fclose(f);
    
    pthread_mutex_lock(&s->lock);
    if (!s->localNode->successor) {
        pthread_mutex_unlock(&s->lock);
        free(content);
        return 0;
    }
    int  local_id = s->localNode->id;
    char next_ip[MAX_IP_LENGTH];
    strncpy(next_ip, s->localNode->successor->Ip, MAX_IP_LENGTH - 1);
    next_ip[MAX_IP_LENGTH - 1] = '\0';
    pthread_mutex_unlock(&s->lock);

    int visited = 0;
    //Loops every node to update registry
    while (visited < (int)MAX_NUMBER_NODES) {
        Node *node = remote_get_node(s, port, next_ip);
        if (!node) break;
        if (node->id == local_id) { freeNode(node); break; }
        
        tcp_send_registry_to(port, node->Ip, content, size);
        
        if (!node->successor) { freeNode(node); break; }
        strncpy(next_ip, node->successor->Ip, MAX_IP_LENGTH - 1);
        next_ip[MAX_IP_LENGTH - 1] = '\0';
        freeNode(node);
        visited++;
    }
    free(content);
    return 0;
}

int insert_chunked(t_server* s, const char* filepath) {
    char chunk_names[MAX_CHUNKS_PER_FILE][256];
    int chunk_count = 0;

    if (split_file(filepath, DHASH_NUM_CHUNKS, chunk_names, &chunk_count) != 0) {
        log_error("[INSERT_CHUNKED] Failed to split file %s", filepath);
    }

    printf("Chunk count: %d\n", chunk_count);

    pthread_mutex_lock(&s->lock);
    int port = s->port;
    pthread_mutex_unlock(&s->lock);

    const char* slash = strrchr(filepath, '/');
    const char* basename = slash ? slash + 1 : filepath;

    char directory_prefix[MAX_FILE_PATH_LENGTH] = {0};

    if (slash) {
        size_t dlen = (size_t)(slash - filepath) + 1;
        if (dlen < sizeof(directory_prefix)) {
            memcpy(directory_prefix, filepath, dlen);
        }
    }

    FileEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.original_name, basename, sizeof(entry.original_name) - 1);
    entry.chunk_count = chunk_count;

    for (int i=0; i< chunk_count; i++) {
        ChunkEntry *ce = &entry.chunks[i];
        strncpy(ce->name, chunk_names[i], sizeof(ce->name) - 1);
        ce->replica_count = 0;

        // Formula for obtaining Chord ring from hash: SHA1(name) mod (2^n)
        int base_id = hash_file_node(chunk_names[i]) % (int)MAX_NUMBER_NODES;

        printf("Base id %d for chunk %s\n", base_id, chunk_names[i]);

        char prev_ip[MAX_IP_LENGTH] = {0};

        for (int r=0; r<DHASH_REPLICA_COUNT; r++) {
            Node* destination;
            if (r==0) {
                destination = remote_find_successor(s, port, base_id);
            } else {
                destination = prev_ip[0] ? remote_get_node(s, port, prev_ip) : NULL;

                if (destination) {
                    Node *nxt = destination->successor ? remote_find_successor(s, port, destination->id + 1) : NULL;
                    destination = nxt;
                }
            }

            if (!destination) {
                log_warn("[INSERT_CHUNKED] No node for chunk %s replica %d", chunk_names[i], r);

                continue;
            }

            char chunk_path[512];
            snprintf(chunk_path, sizeof(chunk_path), "%s/%s", directory_prefix, chunk_names[i]);

            if (tcp_send_file(port, destination->Ip, chunk_names[i], chunk_path) == 0) {
                ce->node_ids[ce->replica_count] = destination->id;
                strncpy(ce->node_ips[ce->replica_count], destination->Ip, MAX_IP_LENGTH - 1);
                ce->replica_count++;
                
                strncpy(prev_ip, destination->Ip, MAX_IP_LENGTH - 1);

                //entry.chunks[ce->replica_count] = *ce;

                printf("File %s inserted successfully at node ID: %d IP: %s\n",  chunk_names[i], destination->id, destination->Ip);
            }

            freeNode(destination);
        }
    }

    //Update registry file
    FileEntry registry[MAX_REGISTRY_ENTRIES];
    int registry_count = 0;
    load_registry(registry, MAX_REGISTRY_ENTRIES, &registry_count);
    upsert_file_entry(registry, &registry_count, MAX_REGISTRY_ENTRIES, &entry);
    save_registry(registry, registry_count);
    broadcast_registry(s, port);

    return 0;
}

int retrieve_file(t_server *s, const char* original_filename, const char* output_dir) {
    pthread_mutex_lock(&s->lock);
    int port = s->port;
    pthread_mutex_unlock(&s->lock);

    FileEntry registry[MAX_REGISTRY_ENTRIES];
    int reg_count = 0;
    if (load_registry(registry, MAX_REGISTRY_ENTRIES, &reg_count) != 0) {
        printf("[retrieve_file] could not load registry");
        return -1;
    }

    //Find the target entry within the registry
    FileEntry* entry = NULL;
    for (int i = 0; i < reg_count; i++) {
        if (strcmp(registry[i].original_name, original_filename) == 0) {
            entry = &registry[i];
            break;
        }
    }

    if (!entry) {
        printf("[retrieve_file] %s not found in registry", original_filename);
        return -1;
    }

    char tmpdir[512];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/chord_retrieve_%d", getpid());
    {
        /*
        char mkdir_cmd[600];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", tmpdir);
        system(mkdir_cmd);
        */
       // Create the directory with read/write/search permissions for the owner (0700)
       if (mkdir(tmpdir, 0700) == -1) {
            if (errno != EEXIST) {
                printf("Failed to create directory for file retrieval\n");
                return -1;
            }
       } else {
            printf("Temp file directory succesfully created\n");
       }
    }

    for (int i = 0; i < entry->chunk_count; i++) {
        ChunkEntry* c = &entry->chunks[i];
        int fetched = 0;

        //Loops every replicated version of chunk in case of node failures
        for (int j = 0; j < c->replica_count && !fetched; j++) {
            char full_path[1024];
            char destination_path[512];

            int result = snprintf(full_path, sizeof(full_path), "%s/%s", tmpdir,c->name);

            printf("Full path %s with result value %d\n", full_path, result);

            if (result >= sizeof(full_path)) {
                fprintf(stderr, "Error: Path is too long for the buffer.\n");
            } else {
                strncpy(destination_path, full_path, sizeof(destination_path) - 1);
                destination_path[sizeof(destination_path) - 1] = '\0';
            }

            if (tcp_fetch_file(port, c->node_ips[j], c->name, full_path) == 0) {
                fetched = 1;
            } else {
                printf("[RETRIEVE_FILE] Unable to retrieve chunk %s replica %d %ls\n", c->name, j, c->node_ids);
            }
        }

        if (!fetched) {
            printf("[RETRIEVE_FILE] All replicated copies failed for chunk %s\n", c->name);
            return -1;
        } else {
            printf("Succesfully fetched chunk %s\n", c->name);
        }
    }

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, original_filename);

    printf("Output path is %s\n", out_path);

    char cat_cmd[2048];
    //Tracks position for text insertion in 'cat' command
    int pos = snprintf(cat_cmd, sizeof(cat_cmd), "cat");
    //Adds initial
    pos += snprintf(cat_cmd + pos, sizeof(cat_cmd) - (size_t)pos, " \"%s\"", out_path);
    for (int i = 0; i < entry->chunk_count && pos < (int)sizeof(cat_cmd) - 256; i++) {
        pos += snprintf(cat_cmd + pos, sizeof(cat_cmd) - (size_t)pos, " \"%s/%s\"", tmpdir, entry->chunks[i].name);
    }
    snprintf(cat_cmd + pos, sizeof(cat_cmd) - (size_t)pos, " > \"%s\"", out_path);

    printf("Cat command: %s\n", cat_cmd);

    if (system(cat_cmd) != 0) {
        printf("[RETRIEVE_FILE] Concatenation failed");
        return -1;
    }

    // Clean up temporary files for file reconstruction
    char rm_cmd[600];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", tmpdir);
    system(rm_cmd);

    printf("[retrieve_file] %s reconstructed at %s", original_filename, out_path);
    return 0;
}
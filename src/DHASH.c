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
*/

/*
=====================================================
Helper functions for manipulating hexadecimal strings
=====================================================
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


// Takes IP and port of a node and generates a numerical ID to assign as 
// Chord ring identifier
int hash_node_identifier(const char* Ip, const char* port) {
    char nodeIdentifier[64]; 

    //Formats IP and port into a single string
    snprintf(nodeIdentifier, sizeof(nodeIdentifier), "%s:%s", Ip, port); 

    char result[64];
    char hexresult[41];
    size_t offset;

    //SHA1 hashing of a string
    SHA1(result, nodeIdentifier, strlen(nodeIdentifier));

    //Generates final hexadecimal string
    for( offset = 0; offset < 20; offset++) {
        sprintf( ( hexresult + (2*offset)), "%02x", result[offset]&0xff);
    }

    int resultId = hexadecimal_to_decimal(hexresult);

    return resultId;
}

//Hashes file name into a Chord ring identifier
//To place file into corresponding node 
int hash_file_node(const char* fileName) {
    char result[64];
    char hexresult[41];
    size_t offset;

    //SHA1 hashing of a string
    SHA1(result, fileName, strlen(fileName));

    //Generates final hexadecimal string
    for( offset = 0; offset < 20; offset++) {
        sprintf( ( hexresult + (2*offset)), "%02x", result[offset]&0xff);
    }

    int resultId = hexadecimal_to_decimal(hexresult);

    return resultId;
}

// Transfers a local file to a remote node's fileContentPath over TCP.
// Returns 0 on success, -1 on failure.
static int tcp_send_file(int port, const char *dest_ip,
                          const char *dest_filename, const char *local_path) {
    FILE *f = fopen(local_path, "rb");
    if (!f) {
        log_error("[STORE_FILE] cannot open local file %s", local_path);
        return -1;
    }
    //Moves stream cursor to the end of the file
    fseek(f, 0, SEEK_END);
    //Determines file size from last cursor position
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

    // Send file bytes to receiving client
    char buf[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        //Checks if no bytes were sent
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

//Generetes ouput directory to store any given file in a node 
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


//Insert a file from the host node to the corresponding node for the given file identifier
void insert(t_server* s, const char* filename) {
    //Hash the identifier and find the node responsible for the given identifier
    int destinationId = hash_file_node(filename);

    pthread_mutex_lock(&s->lock);
    int port = s->port;
    pthread_mutex_unlock(&s->lock);

    //Counts amount of replicated copies of a given file
    int redundancyCounter = 0;

    //Indicates target node to insert file into
    int loopDestinationId = destinationId;

    //redundancyCounter check to store information in set amount of nodes
    //Currently stores in 2 different nodes
    while (redundancyCounter < 2) {
        //Copy relevant node information
        pthread_mutex_lock(&s->lock);
        if (s->localNode->successor == NULL) {
            break;
        }

        int localId = s->localNode->id;
        pthread_mutex_unlock(&s->lock);

        //In case of same node, increases Id to find next available successor
        if (loopDestinationId == localId) {
            //Loops back ID if exceeds permited range
            if (loopDestinationId >= (MAX_NUMBER_NODES - 1) ) {
                loopDestinationId = 0;
            } else {
                loopDestinationId++;
            }
            break;
        }

        //Gets destination node information
        Node* destinationNode = remote_find_successor(s, port, loopDestinationId);

         //In case of same node, grabs next available successor
        pthread_mutex_lock(&s->lock);
        if (destinationNode == s->localNode) {
            loopDestinationId = s->localNode->successor->id;
            pthread_mutex_unlock(&s->lock);

            //Increases replicated copy count
            redundancyCounter++;
            continue;
        }
        pthread_mutex_unlock(&s->lock);
        
        //If invalid node is found
        if (destinationNode == NULL) {
            log_warn("Invalid node for insert operation at ID:%d", loopDestinationId);

            //Provisional solution: Increments id until it finds a valid successor 
            //Review to see if there is better alternative in case of failure
            if (loopDestinationId >= (MAX_NUMBER_NODES - 1)) {
                loopDestinationId = 0;
            } else {
                loopDestinationId++;
            }

            continue;
        }

        char* fileDirectory = generateDestinationFilePath(destinationNode, filename);

        // Sends file to destinationNode
        int result = tcp_send_file(port, destinationNode->Ip, filename, fileDirectory);

        //Successfull operation
        if (result == 0) {
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
// Uses ChordRingFiles format
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

//Copies registry information into individual FileEntry structure
//Insets information one ChunkEntry at a time
//Copies information from a single file line
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

//Passes line by line information from a file into a FileEntry array structure
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

//Writes local FileEntry array information into persisted file 'ChordRingFiles'
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

//Calls splitter.sh to partition a given file into separate files
//Uses bash call to initiate script
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

    //If error occurs
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

    //Writes information from socket communication into 'buf' buffer
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

    //Checks if written information is same length as reported file size
    if (received != filesize) {
        printf("[FETCH_FILE] incomplete file: got %ld of %ld bytes for %s", received, filesize, source_filename);
        return -1;
    }

    return 0;
}

//Sends local registry file to another node
//Used for replicating global registry information - enables file lookup
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

    //Checks for error response message
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

//Replicates registry file to every node in the Chord ring
int broadcast_registry(t_server *s, int port) {
    FILE *f = fopen(REGISTRY_PATH, "rb");
    //Error opening file
    if (!f) return -1;

    //Obtain file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    //Assigns 'content' buffer and copies information from local registry file
    char *content = malloc((size_t)size + 1);
    if (!content) { fclose(f); return -1; }
    fread(content, 1, (size_t)size, f);
    content[size] = '\0';
    fclose(f);
    
    //Copies relevant information from local and successor node 
    pthread_mutex_lock(&s->lock);
    //Checks if valid node succesor
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
        //If consulted node is the same as local
        if (node->id == local_id) { freeNode(node); break; }
        
        //Remote send of file contents
        tcp_send_registry_to(port, node->Ip, content, size);
        
        //Breaks replication if successor is not operational
        if (!node->successor) { freeNode(node); break; }
        strncpy(next_ip, node->successor->Ip, MAX_IP_LENGTH - 1);
        next_ip[MAX_IP_LENGTH - 1] = '\0';
        freeNode(node);

        //Updates amount of visited nodes
        visited++;
    }
    free(content);
    return 0;
}

//Remotely stores all partitions of a given file
//Executes whole process of partitioning->searching destination node -> sending file
int insert_chunked(t_server* s, const char* filepath) {
    //String array for storing filename of each partition
    char chunk_names[MAX_CHUNKS_PER_FILE][256];
    int chunk_count = 0;

    //Partitions files and stores respective names into filename array
    if (split_file(filepath, DHASH_NUM_CHUNKS, chunk_names, &chunk_count) != 0) {
        log_error("[INSERT_CHUNKED] Failed to split file %s", filepath);
    }

    printf("Chunk count: %d\n", chunk_count);

    //Copy local node information
    pthread_mutex_lock(&s->lock);
    int port = s->port;
    pthread_mutex_unlock(&s->lock);

    //Stores pointer to memory location of last slash of directory path
    const char* slash = strrchr(filepath, '/');
    //Stores pointer to memory location of the start of the base filename
    const char* basename = slash ? slash + 1 : filepath;

    char directory_prefix[MAX_FILE_PATH_LENGTH] = {0};

    if (slash) {
        //Obtains length of directory path string - not including filename
        size_t dlen = (size_t)(slash - filepath) + 1;
        if (dlen < sizeof(directory_prefix)) {
            memcpy(directory_prefix, filepath, dlen);
        }
    }

    //Initializes FileEntry object
    FileEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.original_name, basename, sizeof(entry.original_name) - 1);
    entry.chunk_count = chunk_count;

    //Loops for every existing file partition
    for (int i=0; i< chunk_count; i++) {
        //Local pointer to file chunk
        ChunkEntry *ce = &entry.chunks[i];
        strncpy(ce->name, chunk_names[i], sizeof(ce->name) - 1);

        //Counter for tracking amount of partition copies sent out
        ce->replica_count = 0;

        // Formula for obtaining Chord ring from hash: SHA1(name) mod (2^n)
        int base_id = hash_file_node(chunk_names[i]) % (int)MAX_NUMBER_NODES;

        printf("Base id %d for chunk %s\n", base_id, chunk_names[i]);

        char prev_ip[MAX_IP_LENGTH] = {0};

        //Loops for every copy needed for each file partition
        for (int r=0; r<DHASH_REPLICA_COUNT; r++) {
            //Node to send file partition to
            Node* destination;
            if (r==0) {
                //If first loop, obtain immediate successor
                destination = remote_find_successor(s, port, base_id);
            } else {
                //If no valid node is found, registers null to evetually skip loop
                destination = prev_ip[0] ? remote_get_node(s, port, prev_ip) : NULL;

                if (destination) {
                    Node *nxt = destination->successor ? remote_find_successor(s, port, destination->id + 1) : NULL;
                    destination = nxt;
                }
            }

            //No valid destination node found
            if (!destination) {
                log_warn("[INSERT_CHUNKED] No node for chunk %s replica %d", chunk_names[i], r);

                continue;
            }

            //Builds complete ouput directory path for file partition storage 
            char chunk_path[512];
            snprintf(chunk_path, sizeof(chunk_path), "%s/%s", directory_prefix, chunk_names[i]);

            //Sends partition to node and verifies success
            if (tcp_send_file(port, destination->Ip, chunk_names[i], chunk_path) == 0) {
                //Updates registry information with destination node info
                ce->node_ids[ce->replica_count] = destination->id;
                strncpy(ce->node_ips[ce->replica_count], destination->Ip, MAX_IP_LENGTH - 1);
                ce->replica_count++;
                
                //Copies destination ip to prev_ip
                strncpy(prev_ip, destination->Ip, MAX_IP_LENGTH - 1);

                //entry.chunks[ce->replica_count] = *ce;

                printf("File %s inserted successfully at node ID: %d IP: %s\n",  chunk_names[i], destination->id, destination->Ip);
            }

            freeNode(destination);
        }
    }

    //Update persisted registry file
    FileEntry registry[MAX_REGISTRY_ENTRIES];
    int registry_count = 0;
    load_registry(registry, MAX_REGISTRY_ENTRIES, &registry_count);
    upsert_file_entry(registry, &registry_count, MAX_REGISTRY_ENTRIES, &entry);
    save_registry(registry, registry_count);
    broadcast_registry(s, port);

    return 0;
}

//Reconstruct a file from its partitions stored in other nodes
int retrieve_file(t_server *s, const char* original_filename, const char* output_dir) {
    pthread_mutex_lock(&s->lock);
    int port = s->port;
    pthread_mutex_unlock(&s->lock);

    //Loads registry information into local variable
    FileEntry registry[MAX_REGISTRY_ENTRIES];
    int reg_count = 0;
    if (load_registry(registry, MAX_REGISTRY_ENTRIES, &reg_count) != 0) {
        printf("[retrieve_file] could not load registry");
        return -1;
    }

    //Find the target entry within the registry, saves to 'entry' local variable
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

    //Constructs temporary directory to store all retrieved file partitions
    // to later reconstruct final file
    char tmpdir[512];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/chord_retrieve_%d", getpid());
    {
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

    //Loop for retrieving every file partition
    for (int i = 0; i < entry->chunk_count; i++) {
        ChunkEntry* c = &entry->chunks[i];
        int fetched = 0;

        //Loops every replicated version of chunk in case of node failures
        for (int j = 0; j < c->replica_count && !fetched; j++) {
            char full_path[1024];
            char destination_path[512];

            //Constructs output directory for file partition on local node
            // Built from the temporary directory path and the corresponding partition name
            int result = snprintf(full_path, sizeof(full_path), "%s/%s", tmpdir,c->name);

            printf("Full path %s with result value %d\n", full_path, result);

            if (result >= sizeof(full_path)) {
                fprintf(stderr, "Error: Path is too long for the buffer.\n");
            } else {
                strncpy(destination_path, full_path, sizeof(destination_path) - 1);
                destination_path[sizeof(destination_path) - 1] = '\0';
            }

            //Executes remote file fetching
            if (c->node_ips[j] == s->localNode->Ip) {
                char mv_command[1024];
                snprintf(mv_command, sizeof(mv_command), "cp shared/files/%s %s", c->name, tmpdir);

                if (system(mv_command) != 0) {
                    log_error("Unable to load locally stored file %s", c->name);
                    continue;
                }
            }
            else if (tcp_fetch_file(port, c->node_ips[j], c->name, full_path) == 0) {
                fetched = 1;
            } else {
                printf("[RETRIEVE_FILE] Unable to retrieve chunk %s replica %d %ls\n", c->name, j, c->node_ids);
            }
        }

        //Checks if any copies of the target partition were retrieved
        if (!fetched) {
            printf("[RETRIEVE_FILE] All replicated copies failed for chunk %s\n", c->name);
            return -1;
        } else {
            printf("Succesfully fetched chunk %s\n", c->name);
        }
    }

    //Output path for final file reconstruction
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, original_filename);

    printf("Output path is %s\n", out_path);

    //Builds 'cat' command to concatenate all file partitions into original file
    char cat_cmd[2048];
    //Variable tracks position for text insertion in 'cat' command
    int pos = snprintf(cat_cmd, sizeof(cat_cmd), "cat");
    
    //Adds output path for cat
    pos += snprintf(cat_cmd + pos, sizeof(cat_cmd) - (size_t)pos, " \"%s\"", out_path);
    
    //Adds every file partition path for concatenation
    for (int i = 0; i < entry->chunk_count && pos < (int)sizeof(cat_cmd) - 256; i++) {
        pos += snprintf(cat_cmd + pos, sizeof(cat_cmd) - (size_t)pos, " \"%s/%s\"", tmpdir, entry->chunks[i].name);
    }
    snprintf(cat_cmd + pos, sizeof(cat_cmd) - (size_t)pos, " > \"%s\"", out_path);

    printf("Cat command: %s\n", cat_cmd);

    //Executes 'cat' command
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
#ifndef DHASH_H
#define DHASH_H

#include "node.h"
#include "tcpServer.h"

/*
This library is meant to perform hashing functions on files to assign them ID's compatible with the Chord network
*/

/* ── tuneable constants ──────────────────────────────────────────────────── */
#define DHASH_NUM_CHUNKS     4   /* parts each file is split into            */
#define DHASH_REPLICA_COUNT  2   /* copies of each chunk stored in the ring  */
#define MAX_CHUNKS_PER_FILE 64
#define MAX_REPLICAS         8
#define MAX_REGISTRY_ENTRIES 256
#define REGISTRY_PATH "shared/ChordRingFiles"

/* ── registry data structures ────────────────────────────────────────────── */

typedef struct {
    char name[256];
    int  node_ids[MAX_REPLICAS];
    char node_ips[MAX_REPLICAS][MAX_IP_LENGTH];
    int  replica_count;
} ChunkEntry;

typedef struct {
    char       original_name[256];
    ChunkEntry chunks[MAX_CHUNKS_PER_FILE];
    int        chunk_count;
} FileEntry;

/* ── hashing helpers ─────────────────────────────────────────────────────── */
char* toUpper_hexadecimal(const char* hexadecimal);
int   hexadecimal_to_decimal(const char* hexadecimal);
int   hash_node_identifier(const char* Ip, const char* port);
int   hash_file_node(const char* fileName);

/* ── registry I/O ────────────────────────────────────────────────────────── */
int  load_registry(FileEntry *entries, int max_entries, int *out_count);
int  save_registry(const FileEntry *entries, int count);
void upsert_file_entry(FileEntry *entries, int *count, int max_entries,
                       const FileEntry *entry);

/* ── file operations ─────────────────────────────────────────────────────── */
char* generateDestinationFilePath(Node* destinationNode, const char* identifier);

/* original single-chunk insert (kept for compatibility) */
void insert(t_server* s, const char* filename);

/* split a file, distribute chunks across the ring, update and broadcast registry */
int insert_chunked(t_server *s, const char *filepath);

/* fetch all chunks from the ring, concatenate into output_dir/<original_name> */
int retrieve_file(t_server *s, const char *original_filename,
                  const char *output_dir);

/* ── TCP helpers (used by insert / retrieve) ─────────────────────────────── */
int tcp_fetch_file(int port, const char *src_ip, const char *remote_filename,
                   const char *local_dest_path);
int broadcast_registry(t_server *s, int port);

#endif

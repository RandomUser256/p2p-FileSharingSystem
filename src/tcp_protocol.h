#ifndef TCP_PROTOCOL_H
#define TCP_PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TCP Communication Configuration */
#define DEFAULT_TCP_PORT 9000
#define TCP_BUFFER_SIZE 512
#define TCP_TIMEOUT_SEC 5

/* Protocol Message Format
 * Text-based protocol with pipe-delimited fields
 * Request: COMMAND|ARG1|ARG2|...\n
 * Response: STATUS|DATA1|DATA2|...\n
 */

/* Command Types */
#define CMD_FIND_SUCCESSOR "find_successor"
#define CMD_GET_SUCCESSOR "get_successor"
#define CMD_CLOSEST_PRECEDING_FINGER "closest_preceding_finger"
#define CMD_NOTIFY "notify"
#define CMD_PRINT_FINGER_TABLE "print_finger_table"
#define CMD_SAVE_FINGER_TABLE "save_finger_table"
#define CMD_LOAD_FINGER_TABLE "load_finger_table"
#define CMD_CHECK_RING "check_ring"
#define CMD_GET_FINGER_ENTRY "get_finger_entry"
#define CMD_STABILIZE "stabilize"
#define CMD_JOIN "join"

/* Response Status Codes */
#define RESP_OK "OK"
#define RESP_ERROR "ERROR"
#define RESP_NOT_FOUND "NOT_FOUND"

/* Helper Functions */

/* Parse a response line into fields
 * Returns number of fields parsed, stores them in fields array
 * Example: "OK|20|192.168.1.5" -> fields[0]="OK", fields[1]="20", fields[2]="192.168.1.5"
 */
int parse_response(const char* line, char** fields, int max_fields);

/* Build a request string
 * Returns dynamically allocated string (caller must free)
 */
char* build_request(const char* command, const char** args, int arg_count);

#endif /* TCP_PROTOCOL_H */

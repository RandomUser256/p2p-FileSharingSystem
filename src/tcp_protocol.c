#include "tcp_protocol.h"

int parse_response(const char* line, char** fields, int max_fields) {
    if (!line || !fields || max_fields <= 0) {
        return 0;
    }

    char* buffer = malloc(strlen(line) + 1);
    if (!buffer) return 0;

    strcpy(buffer, line);

    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    int field_count = 0;
    char* token = strtok(buffer, "|");

    while (token && field_count < max_fields) {
        fields[field_count] = malloc(strlen(token) + 1);
        if (!fields[field_count]) {
            break;
        }
        strcpy(fields[field_count], token);
        field_count++;
        token = strtok(NULL, "|");
    }

    free(buffer);
    return field_count;
}

char* build_request(const char* command, const char** args, int arg_count) {
    if (!command) {
        return NULL;
    }

    /* Calculate required buffer size */
    size_t size = strlen(command) + 2; /* command + newline + null */
    for (int i = 0; i < arg_count; i++) {
        if (args[i]) {
            size += strlen(args[i]) + 1; /* arg + pipe separator */
        }
    }

    char* request = malloc(size);
    if (!request) {
        return NULL;
    }

    strcpy(request, command);

    for (int i = 0; i < arg_count; i++) {
        if (args[i]) {
            strcat(request, "|");
            strcat(request, args[i]);
        }
    }

    strcat(request, "\n");

    return request;
}

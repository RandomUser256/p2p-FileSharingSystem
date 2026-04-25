#include <stdio.h>
#include <stdarg.h>
#include "logger.h"

LogLevel CURRENT_LOG_LEVEL = LOG_INFO;

void set_log_level(LogLevel level) {
    CURRENT_LOG_LEVEL = level;
}

LogLevel get_log_level() {
    return CURRENT_LOG_LEVEL;
}

static void log_internal(LogLevel level, const char* prefix, const char* fmt, va_list args) {
    if (level > CURRENT_LOG_LEVEL) return;

    fprintf(stderr, "%s", prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(LOG_ERROR, "[ERROR] ", fmt, args);
    va_end(args);
}

void log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(LOG_WARN, "[WARN] ", fmt, args);
    va_end(args);
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(LOG_INFO, "[INFO] ", fmt, args);
    va_end(args);
}

void log_debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(LOG_DEBUG, "[DEBUG] ", fmt, args);
    va_end(args);
}
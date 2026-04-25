#ifndef LOGGER_H
#define LOGGER_H

// Log levels
typedef enum {
    LOG_NONE = 0,
    LOG_ERROR = 1,
    LOG_WARN = 2,
    LOG_INFO = 3,
    LOG_DEBUG = 4
} LogLevel;

// Global log level
extern LogLevel CURRENT_LOG_LEVEL;

// Control function
void set_log_level(LogLevel level);
LogLevel get_log_level();

// Logging functions
void log_error(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_info(const char* fmt, ...);
void log_debug(const char* fmt, ...);

#endif
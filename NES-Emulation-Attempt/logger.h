#pragma once

typedef enum {
    LOG_INFO =  0,
    LOG_DEBUG = 1,
    LOG_WARN = 2,
    LOG_CRITICAL = 3,
} Log_level;

int log_initialise();
void log_deinialise();
void log__(Log_level level, const char* fmt, ...);

#define log_info(fmt, ...) log__(LOG_INFO, fmt, ##__VA_ARGS__)
#ifdef _DEBUG
    #define log_debug(fmt, ...) log__(LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define log_debug(fmt, ...)
#endif // DEBUG



#define log_warn(fmt, ...) log__(LOG_WARN, fmt, ##__VA_ARGS__)
#define log_critical(fmt, ...) log__(LOG_CRITICAL, fmt, ##__VA_ARGS__)
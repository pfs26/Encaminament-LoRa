#include "config.h"

// Define log levels
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

// ANSI color codes
#define COLOR_RESET  "\033[0m"
#define COLOR_INFO   "\033[32m"  // Green
#define COLOR_WARN   "\033[33m"  // Yellow
#define COLOR_ERROR  "\033[31m"  // Red

// Amplades per aliniar
#define LOG_LEVEL_WIDTH  1   // Espai per [I], [W], [E]
#define TIMESTAMP_WIDTH  8   // Per millis()
#define FILENAME_WIDTH   16  // Nom fitxer + extensió
#define LINE_WIDTH       4   // Espai línia de codi

#if LOG_LEVEL <= LOG_LEVEL_INFO
    #define _PI(fmt, ...) Serial.printf(COLOR_INFO "[%-*s] %*lu %-*s:%*d | " fmt COLOR_RESET "\n", \
        LOG_LEVEL_WIDTH, "I", TIMESTAMP_WIDTH, millis(), FILENAME_WIDTH, __FILENAME__, LINE_WIDTH, __LINE__, ##__VA_ARGS__)
#else
    #define _PI(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
    #define _PW(fmt, ...) Serial.printf(COLOR_WARN "[%-*s] %*lu %-*s:%*d | " fmt COLOR_RESET "\n", \
        LOG_LEVEL_WIDTH, "W", TIMESTAMP_WIDTH, millis(), FILENAME_WIDTH, __FILENAME__, LINE_WIDTH, __LINE__, ##__VA_ARGS__)
#else
    #define _PW(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
    #define _PE(fmt, ...) Serial.printf(COLOR_ERROR "[%-*s] %*lu %-*s:%*d | " fmt COLOR_RESET "\n", \
        LOG_LEVEL_WIDTH, "E", TIMESTAMP_WIDTH, millis(), FILENAME_WIDTH, __FILENAME__, LINE_WIDTH, __LINE__, ##__VA_ARGS__)
#else
    #define _PE(fmt, ...)
#endif

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
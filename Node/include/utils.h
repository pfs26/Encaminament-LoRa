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
#define COLOR_GRAY   "\033[37m"

// Amplades per aliniar
#define LOG_LEVEL_WIDTH  1   // Espai per [I], [W], [E]
#define TIMESTAMP_WIDTH  8   // Per millis()
#define FILENAME_WIDTH   20  // Nom fitxer + extensió
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

#define DUMP_ARRAY(arr, len) { \
    uint8_t *data = (uint8_t *)arr; \
    for (size_t i = 0; i < len; i++) { \
        if (i % 30 == 0 && i != 0) { \
            Serial.print("\t"); \
            for (size_t j = i - 30; j < i; j++) { \
                if (data[j] >= 32 && data[j] <= 126) { \
                    Serial.print((char)data[j]); \
                } else { \
                    Serial.print('.'); \
                } \
            } \
            Serial.println(); \
        } \
        Serial.printf("%02X ", data[i]); \
    } \
    size_t remaining = len % 30; \
    if (remaining > 0) { \
        for (size_t j = len - remaining; j < len; j++) { \
            if (data[j] >= 32 && data[j] <= 126) { \
                Serial.print((char)data[j]); \
            } else { \
                Serial.print('.'); \
            } \
        } \
        Serial.println(); \
    } \
}

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#include "config.h"

// Define log levels
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

#if LOG_LEVEL <= LOG_LEVEL_INFO
    #define _PI(fmt, ...) Serial.printf("[I] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define _PI(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
    #define _PW(fmt, ...) Serial.printf("[W] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define _PW(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
    #define _PE(fmt, ...) Serial.printf("[E] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define _PE(fmt, ...)
#endif

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
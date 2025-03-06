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

#if LOG_LEVEL <= LOG_LEVEL_INFO
    #define _PI(fmt, ...) Serial.printf(COLOR_INFO "[I]\t%lu\t%s:%d:\t" fmt COLOR_RESET "\n", millis(), __FILENAME__, __LINE__, ##__VA_ARGS__)
#else
    #define _PI(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
    #define _PW(fmt, ...) Serial.printf(COLOR_WARN "[W]\t%lu\t%s:%d:\t" fmt COLOR_RESET "\n", millis(), __FILENAME__, __LINE__, ##__VA_ARGS__)
#else
    #define _PW(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
    #define _PE(fmt, ...) Serial.printf(COLOR_ERROR "[E]\t%lu\t%s:%d:\t" fmt COLOR_RESET "\n", millis(), __FILENAME__, __LINE__, ##__VA_ARGS__)
#else
    #define _PE(fmt, ...)
#endif


#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
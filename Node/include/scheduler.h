// IMPORTANT INCLOURE AQUEST FITXER ABANS QUE TASKSCHEDULER.H

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

// #define _TASK_TIMECRITICAL       // Enable monitoring scheduling overruns
#define _TASK_SLEEP_ON_IDLE_RUN     // Enable 1 ms SLEEP_IDLE powerdowns between runs if no callback methods were invoked during the pass
#define _TASK_STATUS_REQUEST        // Compile with support for StatusRequest functionality - triggering tasks on status change events in addition to time only
// #define _TASK_WDT_IDS            // Compile with support for wdt control points and task ids
// #define _TASK_LTS_POINTER        // Compile with support for local task storage pointer
// #define _TASK_PRIORITY           // Support for layered scheduling priority
// #define _TASK_MICRO_RES          // Support for microsecond resolution
// #define _TASK_STD_FUNCTION       // Support for std::function (ESP8266 ONLY)
// #define _TASK_DEBUG              // Make all methods and variables public for debug purposes
// #define _TASK_INLINE             // Make all methods "inline" - needed to support some multi-tab, multi-file implementations
// #define _TASK_TIMEOUT            // Support for overall task timeout
// #define _TASK_OO_CALLBACKS       // Support for callbacks via inheritance
// #define _TASK_EXPOSE_CHAIN       // Methods to access tasks in the task chain
// #define _TASK_SCHEDULING_OPTIONS // Support for multiple scheduling options
// #define _TASK_DEFINE_MILLIS      // Force forward declaration of millis() and micros() "C" style
// #define _TASK_EXTERNAL_TIME      // Custom millis() and micros() methods
// #define _TASK_THREAD_SAFE        // Enable additional checking for thread safety
#define _TASK_SELF_DESTRUCT         // Enable tasks to "self-destruct" after disable

#define _TASK_CLEANUP_INTERVAL 10000

/* 
 Incloem només les DECLARACIONS.
 TaskScheduler.h inclou també DEFINICIONS
 fent que si s'inclou a múltiples fitxer hi hagi
 diverses definicions del mateix.
 El TaskScheduler.h NOMÉS s'inclou al fitxer scheduler.cpp,
 a un únic lloc en tot el codi. 
*/
#include <TaskSchedulerDeclarations.h>

#include <vector>

Task* scheduler_once(TaskCallback callback, unsigned long startDelay = 0);
Task* scheduler_infinite(unsigned long interval, TaskCallback cb, unsigned long startDelay = 0);
Task* scheduler_repeat(unsigned long interval, unsigned int repetition, TaskCallback cb, unsigned long startDelay = 0);
void scheduler_stop(Task* task);
void scheduler_run();


#endif




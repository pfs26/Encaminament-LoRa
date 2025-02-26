#include "scheduler.h"  // scheduler.h abans que taskscheduler, ja que conté #defines necessaris

#include <Arduino.h>
#include <TaskScheduler.h>

#include "utils.h"

void _delete_completed_tasks();
void _start_cleanup_if_needed();
void _stop_cleanup_if_needed();

Scheduler ts;  
bool is_cleanup_running = false;
std::vector<Task*> scheduled_tasks;

Task* scheduler_once(TaskCallback cb, unsigned long startDelay) {
    Task* task = new Task(TASK_IMMEDIATE, 1, cb, &ts, true, NULL, NULL, false);
    startDelay ? task->enableDelayed(startDelay) : task->enable();
    scheduled_tasks.push_back(task);

    // Inicia cleanup de tasques si fa falta
    _start_cleanup_if_needed();
    return task;
}

Task* scheduler_infinite(unsigned long interval, TaskCallback cb, unsigned long startDelay) {
    Task* task = new Task(interval, TASK_FOREVER, cb, &ts, false);
    startDelay ? task->enableDelayed(startDelay) : task->enable();
    scheduled_tasks.push_back(task);

    // Inicia cleanup de tasques si fa falta
    _start_cleanup_if_needed();
    return task;
}

Task* scheduler_repeat(unsigned long interval, unsigned int repetition, TaskCallback cb, unsigned long startDelay) {
    Task* task = new Task(interval, repetition, cb, &ts, false);
    startDelay ? task->enableDelayed(startDelay) : task->enable();
    scheduled_tasks.push_back(task);

    // Inicia cleanup de tasques si fa falta
    _start_cleanup_if_needed();
    return task;
}

void scheduler_stop(Task* task) {
    task->disable();
    _start_cleanup_if_needed();
}

void _delete_completed_tasks() {
    _PP("Before cleanup, task count: ");
    _PL(scheduled_tasks.size());

    for (auto tsk = scheduled_tasks.begin(); tsk != scheduled_tasks.end();) {
        if (!(*tsk)->isEnabled()) {  // Comprva si tasca habilitada
            ts.deleteTask(**tsk);   
            delete *tsk;  // Borrar de memòria i vector
            tsk = scheduled_tasks.erase(tsk); 
            _PM("Task deleted");
        } else {
            ++tsk;
        }
    }

    _PP("After cleanup, task count: ");
    _PL(scheduled_tasks.size());

    // Aturar cleanup si fa falta
    _stop_cleanup_if_needed();
}

void _start_cleanup_if_needed() {
    // La tasca que hi ha al vector és la que s'acaba d'afegir
    if (!is_cleanup_running && scheduled_tasks.size() == 1) {
        // Iniciar cleanup
        scheduler_infinite(_TASK_CLEANUP_INTERVAL, &_delete_completed_tasks, _TASK_CLEANUP_INTERVAL);
        is_cleanup_running = true;
        _PM("Cleanup started");
    }
}

void _stop_cleanup_if_needed() {
    // Si només queda la propia tasca de cleanup
    if (is_cleanup_running && scheduled_tasks.size() == 1) {  
        Task* cleanup_task = *scheduled_tasks.begin();  
        ts.deleteTask(*cleanup_task); 
        delete cleanup_task;  
        scheduled_tasks.clear();  // Elimina la propia tasca de cleanup
        is_cleanup_running = false;
        _PM("Cleanup task stopped and deleted");
    }
}


void scheduler_run() { ts.execute(); }

/*
TEST, que inclou una mica tot:
    1. Crea tasca única que encén LED
    2. Crea tasca repetitiva que fa 10 pampallugues de LED
    3. Crea tasca infinita que fa pampallugues de LED
    4. Cancel·la la última tasca (aturant infinitud)
    5. Es fa cleanup de totes les tasques al cap de 10 segons (si així es configura)
    6. S'atura cleanup automàtic
    
Task* inf;

void ledOn() {
	_PM("LedOn");
    digitalWrite(LED_BUILTIN, HIGH);
}

void ledOff() {
	_PM("LedOff");
    digitalWrite(LED_BUILTIN, LOW);
}

void ledToggle() {
	_PM("ToggleLed");
	digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void stop() {
	// scheduler_once(&ledToggle, 100);
	scheduler_stop(inf);
}


void setup() {
    #ifdef DEBUG
        Serial.begin(115200);
    #endif
    pinMode(LED_BUILTIN, OUTPUT);

    scheduler_once(&ledOn, 100);
	scheduler_repeat(100, 10, &ledToggle, 500);
	inf = scheduler_infinite(250, ledToggle, 2000);
	scheduler_once(&stop, 4000);
}

void loop() {
    scheduler_run();
}
 */
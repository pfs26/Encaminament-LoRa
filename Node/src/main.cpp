#include <Arduino.h>
#include "config.h"
#include "scheduler.h"

#define LED_BUILTIN 2
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
	scheduler_once(&stop, 1000);
}

void loop() {
    scheduler_run();
}
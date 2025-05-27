/*
    Programa per determinar l'error del rellotge en mode de baix consum.
    Per a diferents temps de sleep, s'envia un missatge abans i després del sleep,
    i a través de `ts` és possible determinar l'error.
*/

#include <Arduino.h>

static RTC_DATA_ATTR int boot = 0;

// en minuts, durada de sleep
static int sleepTimes[] = {2, 4, 8, 16, 30}; 

void setup() {
    Serial.begin(115200);

    Serial.printf("Sleeping for %d minute(s) (%d)\n", sleepTime, boot);
    
    esp_deep_sleep(MIN_TO_US(sleepTimes[MAX(boot++, 4)]));
}

void loop() {
}
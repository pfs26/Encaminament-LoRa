/*
    Proves provinents de la cinquena prova d'`provesOrigenErrorClock.cpp`.

    S'ha dediuit que l'error s'introdueix depenent de l'estona que està actiu.
    Depenent del cicle funcionament (si per 5 minuts estem 1 minut, cicle del 20%),
    la temperatura interna del micro pot augmentar.
    L'error canvia en +1000/+1500ppm per ºC, per tant podria ser possible que fos la causa de l'error.

    Es fan proves amb diferents temps de cicle i comparant amb l'error.

    Per cicles de 2.5 minuts:

    Nova:
    Temps actiu     |Sleep  | Error   |Error % | Error (ppm)  |
    ----------------|-------|---------|--------|--------------|
    1               |148.990|1.643201 |1.1029  |11029         |
    2               |147.980|1.749711 |1.1824  |11824         |
    10              |139.980|2.095421 |1.4969  |14969         |
    25              |124.980|2.066104 |1.6531  |16531         |
    50              |99.980 |1.68     |1.6803  |16803         |
    75              |74.980 |1.303489 |1.7384  |17384         |


    Vella:
    Temps actiu     |Sleep  | Error   | Error (ppm)  |
    ----------------|-------|---------|--------------|
    1               |148.962|1.618333 |10864         |
    2               |147.962|1.851677 |12515         |
    10              |139.961|2.051842 |14660         |
    25              |124.960|2.032617 |16266         |
    50              |99.961 |1.683075 |16837         |
    75              |74.961 |1.233312 |16453         |
*/


#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"

static RTC_DATA_ATTR int boot = 0;

// en segons. Indiquen temps actiu abans de dormir
static float activeTimes[] = {1, 2, 10, 25, 50, 75}; 

void onSend() {
    int sleep_ms = MIN_TO_MS(2.5);

    // Dormim el temps restant
    uint64_t sleepTime = sleep_ms - millis();

    Serial.printf("Sleeping for %d ms\n", sleepTime);
    
    esp_deep_sleep(MS_TO_US(sleepTime));
}

void onSendMac(uint16_t id) {
    onSend();
}

void delaySend() {
    Transport_send(0x09, 0x01, nullptr, 0, false);
}

void setup() {
    Serial.begin(115200);

    scheduler_once(onSend, S_TO_MS(activeTimes[boot++ % 6]));
}

void loop() {
    scheduler_run();
}
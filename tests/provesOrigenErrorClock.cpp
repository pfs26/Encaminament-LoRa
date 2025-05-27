/*
    Primera prova és sense delay: enviar un missatge i dormir.
    Segona prova és despertar i dormir automàticament sense enviar.
    Tercera prova és fer enviament a través de MAC per descartar que el problema sigui de capes superiors a MAC
    Quarta prova és despertar, esperar 10 segons, i dormir.
    Cinquena prova és en un dispositiu despertar, enviar i dormir, i en l'altre dispositiu queda despert un temps
        similar al que es triga a enviar, i dormir

        De la 5ena prova s'extreu que els temps d'error són MOLT similars, i que per tant
        no depenen de si es fa transmissió o no.
        Es dedueix que l'error s'introdueix en tenir el dispositiu actiu durant un temps determinat.

*/

#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"

static RTC_DATA_ATTR int boot = 0;

// en minuts, per prova 4
static float sleepTimes[] = {1.25, 2.5, 5, 10}; 

void onSend() {
    int sleep_ms = MIN_TO_MS(5);


    // int sleep_ms = MIN_TO_MS(sleepTimes[boot++ % 4]);

    LoRaRAW_sleep();
    Transport_deinit(SLEEP_PORT);
    
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

    // Inicialitzem per tenir mateix estat que quan estigui funcionant
    if(!Transport_init(0x05, IS_GATEWAY)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    RoutingTable_addRoute(0x09, 0x09);

    // Prova 1
    Transport_onEvent(0x01, nullptr, onSend, onSend);
    // Transport_send(0x09, 0x01, nullptr, 0, false);
  
    // Prova 2. Necessari per forçar sleep
    // onSend();
    
    // Prova 3
    // MAC_onSend(onSendMac);
    // MAC_onTxFailed(onSendMac);
    // MAC_send(0x09, nullptr, 0);

    // Prova 4.
    // Esperem 10 segons (ja que listener estaria despert també un temps abans esperant recepció)
    // i llavors enviem
    // scheduler_once(delaySend, 10000);

    // Prova 5. Pel dispositiu que no envia. Temps fet a mà.
    // scheduler_once(onSend, 970);
}

void loop() {
    scheduler_run();
}
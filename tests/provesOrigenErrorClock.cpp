/*
    Bateria de proves per buscar l'origen de l'error del rellotge.
    Amb tests `calculErrorPPM` s'obtenen valors semblants per ambdós dispositius, en
    implementar protocol, es converteixen en +8000 i +16000 PPM.

    Diferents combinacions de:
    - Enviar un missatge i dormir
    - Despertar i dormir automàticament sense enviar
    - Enviar a través de MAC per descartar que el problema sigui de capes superiors a MAC
    - Despertar, esperar, i enviar. Procediment que fa el listener d'APP.
    - Despertar, esperar un temps similar al que triga a enviar el de referència, i dormir, sense enviar.
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

    // Prova 1. Enviar per transport i dormir.
    Transport_onEvent(0x01, nullptr, onSend, onSend);
    // Transport_send(0x09, 0x01, nullptr, 0, false);
  
    // Prova 2. Despertar i dormir automàticament sense enviar.
    // onSend();
    
    // Prova 3. Enviar a través de MAC per descartar que el problema sigui de capes superiors a MAC.
    // MAC_onSend(onSendMac);
    // MAC_onTxFailed(onSendMac);
    // MAC_send(0x09, nullptr, 0);

    // Prova 4. Despertar, esperar 10 segons, i enviar. Procediemtn semblant al que fa "listener" d'APP.
    // Esperem 10 segons (ja que listener estaria despert també un temps abans esperant recepció)
    // i llavors enviem
    // scheduler_once(delaySend, 10000);

    // Prova 5. Despertar, esperar temps semblant al que triga a enviar el de referència, i dormir.
    // scheduler_once(onSend, 970);
}

void loop() {
    scheduler_run();
}
/*
    Per verificar cua de recepció de MAC.
    Capa MAC notifica en cada recepció a la capa superior pel callback; si la capa superior
    no obté les dades rebudes, es guarden en buffer de recepció.
    En executar `MAC_receive()` s'obté la primera dada rebuda. És responsabilitat de la capa
    superior d'obtenir les dades (i en cas de no fer-ho, gestionar quantes en queden per rebre).

    El buffer de recepció existeix, únicament, com a mesura de seguretat per no perdre dades.

    Situació:
        1. Un node de recepció que no envia dades, només ACK. Guarda les dades al buffer de recepció
        2. Quan s'avisa per callback de recepció, guarda quantes dades s'han rebut
        3. En rebre'n 5, obté les 5 dades rebudes de cop

        Requereix un node transmissor que enviï les dades.
*/

#include <Arduino.h>
#include "mac.h"
#include "scheduler.h"
#include "utils.h"


void onRcv() {
    static int received = 0;
    Serial.println("MAC frame received");
    received++;
    if(received == 5) {
        for(int i = 0; i < 5; i++) {
            mac_data_t data;
            size_t length;
            mac_addr_t tx = MAC_receive(&data, &length);
            data[length] = '\0';
            Serial.printf("\tTX: %d\n\tData: %s\tLength: %d\n", tx, data, length);
        }
        received = 0;
    }
}



void setup() {
    Serial.begin(115200);

    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println(esp_reset_reason());



    mac_addr_t addr = 0x01;

    if(!MAC_init(addr, false)) {
        _PE("ERR");
        while(1);
    }

    MAC_onReceive(onRcv);
}

void loop() {
    scheduler_run();
}
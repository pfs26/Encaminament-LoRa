/*
    Exemple amb dos nodes: un és transmissor i l'altre receptor.
    El transmissor envia frames cada 5 segons. El retard del tansmissor és bloquejant (fet amb delay())
    El receptor imprimeix les dades rebudes per pantalla, i envia ACK si li toca
*/

#include <Arduino.h>
#include "mac.h"
#include "scheduler.h"
#include "utils.h"

#define SENDER

int count = 0;

void onSend(mac_id_t id) {
    Serial.printf("MAC frame sent (%d)\n", id);
    delay(5000);
    mac_data_t data = "Hola!";
    mac_id_t id2;
    MAC_send(0x02, data, 5, &id2);
    Serial.printf("Sent %d\n", id2);
}

void onErr(mac_id_t id) {
    Serial.printf("Error sending mac frame %d\n", id);
    delay(5000);
    mac_data_t data = "Hola!";
    MAC_send(0x02, data, 5);
}

void onRcv() {
    Serial.println("MAC frame received");
    mac_data_t data;
    size_t length;
    node_address_t tx = MAC_receive(&data, &length);
    data[length] = '\0'; // Per poder imprimir amb serial
    Serial.printf("\tTX: %d\n\tData: %s\tLength: %d\n", tx, data, length);
}

void setup() {
    Serial.begin(921600);

    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println(esp_reset_reason());


    #ifdef SENDER
    node_address_t addr = 0x01;
    #else
    node_address_t addr = 0x02;
    #endif
    if(!MAC_init(addr)) {
        _PE("ERR");
        while(1);
    }

    MAC_onSend(onSend);
    MAC_onTxFailed(onErr);
    MAC_onReceive(onRcv);

    #ifdef SENDER
        mac_data_t data = "Hola!";
        mac_id_t id;
        MAC_send(0x02, data, 5, &id);
        Serial.printf("Sent %d\n", id);
    #endif
}

void loop() {
    scheduler_run();
}
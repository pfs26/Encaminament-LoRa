/*
    Exemple amb dos nodes: un és transmissor i l'altre receptor.
    El transmissor envia frames cada 5 segons. El retard del tansmissor és bloquejant (fet amb delay())
    El receptor imprimeix les dades rebudes per pantalla, i envia ACK si li toca
*/

#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"

#define SENDER

int count = 0;

void onSend() {
    Serial.println("Segment sent");
    transport_data_t data = "UDP!!";
    transport_err_t state = Transport_send(0x02, 0x01, data, 5, false);
    if(state != TRANSPORT_SUCCESS) {
        Serial.printf("Error sending: %d\n", state);
        return;
    }
    Serial.println("Segment scheduled to be sent");
}

void onRcv() {
    Serial.println("Segment received");
    transport_data_t data;
    size_t length;
    uint8_t port;
    node_address_t tx = Transport_receive(&port, &data, &length);
    data[length] = '\0'; // Per poder imprimir amb serial
    Serial.printf("\tTX: %d\tData: %s\tLength: %d\tPort: %d\n", tx, data, length, port);
}

void setup() {
    Serial.begin(115200);

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
    if(!Transport_init(addr, false)) {
        _PE("ERR");
        while(1);
    }


    Transport_onReceive(onRcv);
    Transport_onSend(onSend);

    RoutingTable_clear();
    #ifdef SENDER
        RoutingTable_addRoute(0x02, 0x02);
        transport_data_t data = "Hola!";
        transport_err_t state = Transport_send(0x02, 0x01, data, 5, true);
        if(state != TRANSPORT_SUCCESS) {
            Serial.printf("Error sending: %d\n", state);
            return;
        }
        Serial.println("Segment scheduled to be sent");
        state = Transport_send(0x02, 0x01, data, 5, true);
        if(state != TRANSPORT_SUCCESS) {
            Serial.printf("Error sending: %d\n", state);
            return;
        }
        Serial.println("Segment scheduled to be sent");
    #else
        RoutingTable_addRoute(0x01, 0x01);
    #endif
}

void loop() {
    scheduler_run();
}

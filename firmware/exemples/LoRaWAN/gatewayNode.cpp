/*
    Exemple de comunicació entre dos nodes amb LoRaWAN i routing per gateway.
    Situació:
        - Gateway **real** de LoRaWAN, que utilitza l'adreça 'virtual' NODE_ADDRESS_GATEWAY (0x01)
        - Node amb rol de gateway de la xarxa privada, amb adreça 0x02
        - Node client de la xarxa privada, amb adreça 0x03
    El node client (0x03) vol fer una transmissió a gateway LoRaWAN (0x01), utilitzant node gateway (0x02) com a intermediari.
    El node gateway (0x02) ha de reenviar la trama a LoRaWAN, i el gateway LoRaWAN ha de rebre la trama.

    Es fa una única transmissió des de capa de transport.

    La taula de ruta de client és:
        0x01 -> 0x02
        0x02 -> 0x02
    La taula de ruta de gateway és:
        0x01 -> 0x01
        0x03 -> 0x03
*/

#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"

#define GATEWAY

int count = 0;

void onSend() {
    Serial.println("Segment sent to LoRaWAN!");
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
    delay(2000);
    
    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println(esp_reset_reason());
    Serial.println("=================================");
    Serial.println("LoRaWAN with Gateway routing test");
    Serial.println("=================================");


    #ifdef GATEWAY
        node_address_t addr = 0x02;
        bool is_gateway = true;
    #else
        node_address_t addr = 0x03;
        bool is_gateway = false;
    #endif

    if(!Transport_init(addr, is_gateway)) {
        _PE("ERR");
        while(1);
    }


    Transport_onReceive(onRcv);
    Transport_onSend(onSend);

    RoutingTable_clear();
    #ifdef GATEWAY
        RoutingTable_addRoute(0x01, 0x01);
        RoutingTable_addRoute(0x03, 0x03);
    #else
        RoutingTable_addRoute(0x01, 0x02);
        RoutingTable_addRoute(0x02, 0x02);
    #endif

    #ifndef GATEWAY
        transport_data_t data = "Hola!";
        transport_err_t state = Transport_send(NODE_ADDRESS_GATEWAY, 0x01, data, 5, true);
        if(state != TRANSPORT_SUCCESS) {
            Serial.printf("Error sending: %d\n", state);
            return;
        }
        Serial.println("Segment scheduled to be sent");
    #endif
}

void loop() {
    scheduler_run();
}

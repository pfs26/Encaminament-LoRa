/*
    TRANSPORT

    Situació:
        0x03            0x02            Gateway
        ============    ============    ============
        0x02 -> 0x02    0x03 -> 0x03
        GW -> 0x02      GW -> GW

    Node 0x03 vol enviar a GW, TCP
*/


#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"
#include <stdlib.h> // For rand()

#if IS_GATEWAY
    #define NODE_ADDRESS 0x02
#else
    #define NODE_ADDRESS 0x03
#endif

void onReceive() {
    transport_port_t port;
    transport_data_t data;
    size_t datalen;
    Transport_receive(&port, &data, &datalen);
    data[datalen] = '\0'; // Null-terminate the string
    Serial.printf("Received data on port: %d\tLenght: %d\tData: %s\n", port, datalen, (char*)data);
}

void onSend() {
    // Executat quan s'ha enviat un missatge i s'ha rebut ACK
    // NO s'executa si s'envia sense esperar ACK
    Serial.printf("[%d] Data sent with ACK reception\n", millis());
        char* data = "Hello 0x02";
        Transport_send(0x02, 63, (uint8_t*)data, 10, false);

}

void setup() {
    Serial.begin(921600);
    
    Serial.println("===========================");
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.print("Flash: "); Serial.println(ESP.getFlashChipSize());
    Serial.print("Flash speed: "); Serial.println(ESP.getFlashChipSpeed());
    Serial.print("Flash mode: "); Serial.println(ESP.getFlashChipMode());
    Serial.print("Reset reason: "); Serial.println(esp_reset_reason());
    Serial.println("===========================");
    Serial.printf("Node address: 0x%02X\tGateway: %d\n", NODE_ADDRESS, IS_GATEWAY);
    Serial.println("===========================");
    
    if(!Transport_init(NODE_ADDRESS, IS_GATEWAY)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    // configurem callbacks capa transport per aplicació personalitzada (aleatòria)
    Transport_onEvent(63, onReceive, onSend);

    // Configurem taula de rutes de cada node
    RoutingTable_clear();
    #if IS_GATEWAY
        RoutingTable_addRoute(NODE_ADDRESS_GATEWAY, NODE_ADDRESS_GATEWAY); 
        RoutingTable_addRoute(0x03, 0x03); 
    #else
        RoutingTable_addRoute(NODE_ADDRESS_GATEWAY, 0x02); 
        RoutingTable_addRoute(0x02, 0x02); 
    #endif

    #if !IS_GATEWAY
        char* data = "Hello";
        Transport_send(NODE_ADDRESS_GATEWAY, 63, (uint8_t*)data, 5, true);
    #endif
}

void loop() {
    scheduler_run();
}


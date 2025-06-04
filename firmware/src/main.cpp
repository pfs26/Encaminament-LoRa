#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"

#if IS_GATEWAY
    #define NODE_ADDRESS 0x02
#else
    #define NODE_ADDRESS 0x03
#endif

// Únicament per mostrar informació del test
uint16_t RTC_DATA_ATTR bootCount = 0;
uint16_t RTC_DATA_ATTR syncCount = 0;

void getData(uint8_t* data, size_t size) {
    Serial.printf("[%d] Ready to send data: ", millis());
    syncCount++;
    for(int i = 0; i < size; i++) {
        data[i] = esp_random() % 256;
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    Serial.printf("Boot: %d\tSync: %d\n", bootCount, syncCount);
    
    if(!Transport_init(NODE_ADDRESS, IS_GATEWAY)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    Sleep_onSync(getData);

    // Configurem nodes a qui notificar missatges de sincronització
    #if IS_GATEWAY
        node_address_t node = 0x01;
    #else
        node_address_t node = 0x02;
        #endif
        
    RoutingTable_addRoute(node, node);
    
    if(!Sleep_setForwardNode(node)) {
        Serial.println("Sleep set forward node failed");
        while(1) delay(1);
    }

    // Després de `sleep_init()` no hi hauria d'haver transmissions, a banda de les que 
    // sleep genera.
    if(!Sleep_init()) {
        Serial.println("Sleep init failed");
        while(1) delay(1);
    }
}

void loop() {
    scheduler_run();
}
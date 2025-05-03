#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"
#include <stdlib.h> // For rand()

#if true
// #if IS_GATEWAY
    #define NODE_ADDRESS 0x02
#else
    #define NODE_ADDRESS 0x03
#endif

// Únicament per mostrar informació del test
uint16_t RTC_DATA_ATTR bootCount = 0;
uint16_t RTC_DATA_ATTR syncCount = 0;

void getData(uint8_t* data) {
    Serial.printf("[%d] Ready to send data: ", millis());
    syncCount++;
    for(int i = 0; i < SLEEP_DATASIZE_PER_NODE; i++) {
        data[i] = esp_random() % 256;
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

void setup() {
    Serial.begin(921600);
    
    if(bootCount++ == 0) {
        Serial.println("===========================");
        Serial.print("Model: "); Serial.println(ESP.getChipModel());
        Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
        Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
        Serial.print("Flash: "); Serial.println(ESP.getFlashChipSize());
        Serial.print("Flash speed: "); Serial.println(ESP.getFlashChipSpeed());
        Serial.print("Flash mode: "); Serial.println(ESP.getFlashChipMode());
        Serial.print("Reset reason: "); Serial.println(esp_reset_reason());
        Serial.println("===========================");
        Serial.println("RANDOM OPERATION SLEEP TEST");
        Serial.println("===========================");
        Serial.printf("Node address: 0x%02X\tGateway: %d\n", NODE_ADDRESS, IS_GATEWAY);
    }
    
    Serial.println("===========================");
    Serial.printf("Boot: %d\tSync: %d\n", bootCount, syncCount);
    
    if(!Transport_init(NODE_ADDRESS, IS_GATEWAY)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    Sleep_onSync(getData);

    // Configurem nodes a qui notificar missatges de sincronització
    #if IS_GATEWAY
        node_address_t node = 0x00;
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
    // Només s'haurien d'iniciar transmissions després de rebre SYNC `Sleep_onSync()`
    if(!Sleep_init()) {
        Serial.println("Sleep init failed");
        while(1) delay(1);
    }
}

void loop() {
    scheduler_run();
}
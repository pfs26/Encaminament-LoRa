#include "utils/LinkedFIFO.hpp"
#include <Arduino.h>
#include "mac.h"


LinkedFIFO<mac_pdu_t> fifo;

mac_pdu_t data;
void createNode(int high) {
    fifo.push(data);  
    // Serial.printf("Node pushed! (Heap: %d)\n", ESP.getFreeHeap());
}

void getNode() {
    mac_pdu_t data;
    if (fifo.pop(data)) {
        // Serial.printf("Popped %d (Heap: %d)\n", data.id, ESP.getFreeHeap());
    } else {
        // Serial.println("FIFO is empty.");
    }
}


void setup() {
    Serial.begin(115200); 
    delay(2000); // Give some time for the serial to initialize

    Serial.println("Starting LinkedFIFO Test...");

    #define NODES 20

    long start = micros();
    for (size_t i = 0; i < NODES; i++)
    {
        createNode(i);
    }
    long end = micros();
    Serial.printf("Pushed %d nodes in %lu us. (avg: %lu)\n", NODES, end - start, (end - start) / NODES);

    start = micros();
    for (size_t i = 0; i < NODES; i++)
    {
        getNode();
    }
    end = micros();
    Serial.printf("Popped %d nodes in %lu us. (avg: %lu)\n", NODES, end - start, (end - start) / NODES);
    
    Serial.println("End of FIFO Test.");
}

void loop() {
}

/*
    RESULTATS TEMPS AMB mac_pdu_T
    - 20 nodes: 8us/push, 5us/pop
    - 100 nodes: 10us/push, 4us/pop
    - 500 nodes: 19us/push, 4us/pop
    - 750 nodes: 28us/push, 5us/pop
*/
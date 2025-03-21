/*
    Verificacions LinkedList
*/

#include <Arduino.h>
#include "utils/LinkedFIFO.hpp"

LinkedFIFO<int> fifo;

void setup() {
    Serial.begin(115200);

    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

    Serial.println("Starting LinkedFIFO Test...");
    Serial.println("Pushing elements 1, 2, 3 into FIFO...");
    fifo.push(1);
    fifo.push(2);
    fifo.push(3);

    Serial.print("FIFO size after push: ");
    Serial.println(fifo.getSize());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

    int value;
    while (!fifo.isEmpty()) {
        if (fifo.pop(value)) {
            Serial.print("Popped value: ");
            Serial.println(value);
            Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
        }
    }

    Serial.print("FIFO size after popping all elements: ");
    Serial.println(fifo.getSize());

    Serial.println("Pushing elements 10, 20, 30...");
    fifo.push(10);
    fifo.push(20);
    fifo.push(30);
    Serial.print("FIFO size after pushing: ");
    Serial.println(fifo.getSize());

    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println("Clearing FIFO...");
    fifo.clear();
    Serial.print("FIFO size after clear: ");
    Serial.println(fifo.getSize());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
}

void loop() {
// Main loop does nothing, as this is a one-time test
}
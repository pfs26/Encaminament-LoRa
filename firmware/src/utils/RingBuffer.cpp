/*
    Implementació d'un buffer circular de valors de 16 bits.
        1. Si s'afegeix un valor i el buffer està ple, es substitueix pel valor més antic
        2. Permet afegir i eliminar valors del buffer
        3. Permet buscar elements dins del buffer
*/

// TODO: Potser hauria de ser una interfície de std::vector, reservant mida demanada, i limitant? O de deque?

#include "RingBuffer.h"

RingBuffer::RingBuffer(int size) {
    capacity = size;
    buffer = new uint16_t[capacity];
    head = 0;
    tail = 0;
    count = 0;
}

RingBuffer::~RingBuffer() {
    delete[] buffer;
}

void RingBuffer::enqueue(uint16_t value) {
    buffer[head] = value;
    head = (head + 1) % capacity;
    if (count == capacity) {
        tail = (tail + 1) % capacity;  // Sobreescriure més antic (cua)
    } else {
        count++;
    }
}

uint16_t RingBuffer::dequeue() {
    if (count == 0) return 255; // Valor arbitrari si és buida
    uint16_t value = buffer[tail];
    tail = (tail + 1) % capacity;
    count--;
    return value;
}

bool RingBuffer::contains(uint16_t value) {
    for (int i = 0, idx = tail; i < count; i++, idx = (idx + 1) % capacity) {
        if (buffer[idx] == value) return true;
    }
    return false;
}

bool RingBuffer::remove(uint16_t value) {
    if (count == 0) return false;

    int shiftCount = 0;
    for (int i = 0, idx = tail; i < count; i++, idx = (idx + 1) % capacity) {
        if (buffer[idx] == value) {
            shiftCount++;  // S'ha trbat l'element a eliminar
        } else if (shiftCount > 0) { // Un cop trobat, desplaçar elements següents
            // Shift elements forward
            int prevIdx = (idx - shiftCount + capacity) % capacity;
            buffer[prevIdx] = buffer[idx];
        }
    }

    count -= shiftCount;
    head = (tail + count) % capacity;
    return shiftCount > 0;
}

void RingBuffer::printBuffer() {
    Serial.print("Buffer: ");
    for (int i = 0, idx = tail; i < count; i++, idx = (idx + 1) % capacity) {
        Serial.print(buffer[idx]);
        Serial.print(" ");
    }
    Serial.println();
}
#include "RingBuffer.h"

RingBuffer::RingBuffer(int size) {
    capacity = size;
    buffer = new uint8_t[capacity];
    head = 0;
    tail = 0;
    count = 0;
}

RingBuffer::~RingBuffer() {
    delete[] buffer;
}

void RingBuffer::enqueue(uint8_t value) {
    buffer[head] = value;
    head = (head + 1) % capacity;
    if (count == capacity) {
        tail = (tail + 1) % capacity;  // Overwrite oldest
    } else {
        count++;
    }
}

uint8_t RingBuffer::dequeue() {
    if (count == 0) return 255; // Return invalid if empty
    uint8_t value = buffer[tail];
    tail = (tail + 1) % capacity;
    count--;
    return value;
}

bool RingBuffer::contains(uint8_t value) {
    for (int i = 0, idx = tail; i < count; i++, idx = (idx + 1) % capacity) {
        if (buffer[idx] == value) return true;
    }
    return false;
}

bool RingBuffer::remove(uint8_t value) {
    if (count == 0) return false;

    int shiftCount = 0;
    for (int i = 0, idx = tail; i < count; i++, idx = (idx + 1) % capacity) {
        if (buffer[idx] == value) {
            shiftCount++;  // Mark this value to be removed
        } else if (shiftCount > 0) {
            // Shift elements forward
            int prevIdx = (idx - shiftCount + capacity) % capacity;
            buffer[prevIdx] = buffer[idx];
        }
    }

    count -= shiftCount;
    head = (tail + count) % capacity; // Adjust head position
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
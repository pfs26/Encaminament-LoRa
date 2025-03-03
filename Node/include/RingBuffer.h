#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <Arduino.h>

class RingBuffer {
private:
    uint8_t* buffer;
    int capacity;
    int head;
    int tail;
    int count;

public:
    // Constructor & Destructor
    RingBuffer(int size);
    ~RingBuffer();

    // Buffer operations
    void enqueue(uint8_t value);
    uint8_t dequeue();
    bool contains(uint8_t value);
    bool remove(uint8_t value);
    void printBuffer();
};

#endif // RINGBUFFER_H

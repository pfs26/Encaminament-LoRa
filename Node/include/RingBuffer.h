#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <Arduino.h>

class RingBuffer {
private:
    uint16_t* buffer;
    int capacity;
    int head;
    int tail;
    int count;

public:
    // Constructor & Destructor
    RingBuffer(int size);
    ~RingBuffer();

    // Buffer operations
    void enqueue(uint16_t value);
    uint16_t dequeue();
    bool contains(uint16_t value);
    bool remove(uint16_t value);
    void printBuffer();
};

#endif // RINGBUFFER_H

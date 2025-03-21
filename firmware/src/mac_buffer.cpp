#include "mac_buffer.h"

static mac_buffer_t txQueue;
static mac_buffer_t rxQueue;

bool MACbuff_isTxEmpty() {
    return txQueue.high.isEmpty() && txQueue.low.isEmpty();
}

bool MACbuff_isRxEmpty() {
    return rxQueue.high.isEmpty() && rxQueue.low.isEmpty();
}

mac_buffer_priority_t MACbuff_popTx(mac_pdu_t& pdu) {
    if(!txQueue.high.isEmpty()) {
        txQueue.high.pop(pdu);
        return MACBUFF_PRIORITY_HIGH;
    }
    else if (!txQueue.low.isEmpty()) {
        txQueue.low.pop(pdu);
        return MACBUFF_PRIORITY_LOW;
    }
    return MACBUFF_PRIORITY_NONE;
}

bool MACbuff_pushTx(mac_pdu_t& pdu, mac_buffer_priority_t priority) {
    switch (priority) {
        case MACBUFF_PRIORITY_HIGH:
            txQueue.high.push(pdu);
            break;
        case MACBUFF_PRIORITY_LOW:
            txQueue.low.push(pdu);
            break;
        default:
            return false;
    }
    return true;
}

mac_buffer_priority_t MACbuff_popRx(mac_pdu_t& pdu) {
    if(!rxQueue.high.isEmpty()) {
        rxQueue.high.pop(pdu);
        return MACBUFF_PRIORITY_HIGH;
    }
    else if (!rxQueue.low.isEmpty()) {
        rxQueue.low.pop(pdu);
        return MACBUFF_PRIORITY_LOW;
    }
    return MACBUFF_PRIORITY_NONE;
}

bool MACbuff_pushRx(mac_pdu_t& pdu, mac_buffer_priority_t priority) {
    switch (priority) {
        case MACBUFF_PRIORITY_HIGH:
            rxQueue.high.push(pdu);
            break;
        case MACBUFF_PRIORITY_LOW:
            rxQueue.low.push(pdu);
            break;
        default:
            return false;
    }
    return true;
}

size_t MACbuff_getTxSize() {
    return txQueue.high.getSize() + txQueue.low.getSize();
}

size_t MACbuff_getRxSize() {
    return rxQueue.high.getSize() + rxQueue.low.getSize();
}
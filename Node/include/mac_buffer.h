#ifndef _MAC_BUFFER_H
#define _MAC_BUFFER_H

#include <stdint.h>
#include "utils/LinkedFIFO.hpp"
#include "mac.h"

typedef struct {
    LinkedFIFO<mac_pdu_t> high;
    LinkedFIFO<mac_pdu_t> low;
} mac_buffer_t;

enum mac_buffer_priority_t {MACBUFF_PRIORITY_NONE = -1, MACBUFF_PRIORITY_LOW, MACBUFF_PRIORITY_HIGH};


bool MACbuff_isTxEmpty();
bool MACbuff_isRxEmpty();
mac_buffer_priority_t MACbuff_popTx(mac_pdu_t& pdu);
bool MACbuff_pushTx(mac_pdu_t& pdu, mac_buffer_priority_t priority);
mac_buffer_priority_t MACbuff_popRx(mac_pdu_t& pdu);
bool MACbuff_pushRx(mac_pdu_t& pdu, mac_buffer_priority_t priority);
size_t MACbuff_getTxSize();
size_t MACbuff_getRxSize();

#endif
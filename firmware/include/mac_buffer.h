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

/// @brief Retorna si cua TX està buid
/// @return `true` si buida
bool MACbuff_isTxEmpty();

/// @brief Retorna si cua RX està buid
/// @return `true` si buida
bool MACbuff_isRxEmpty();

/// @brief Obté un element de la cua de TX
/// @param pdu PDU obtinguda
/// @return Prioritat del PDU obtingut, o `MACBUFF_PRIORITY_NONE` si no hi ha cap element
mac_buffer_priority_t MACbuff_popTx(mac_pdu_t& pdu);

/// @brief Afegeix un element a la cua de TX
/// @param pdu PDU a afegir a la cua
/// @param priority Prioritat del PDU a afegir
bool MACbuff_pushTx(mac_pdu_t& pdu, mac_buffer_priority_t priority);

/// @brief Obté un element de la cua de RX
/// @param pdu PDU obtinguda
/// @return Prioritat del PDU obtingut, o `MACBUFF_PRIORITY_NONE` si no hi ha cap element
mac_buffer_priority_t MACbuff_popRx(mac_pdu_t& pdu);

/// @brief Afegeix un element a la cua de RX
/// @param pdu PDU a afegir a la cua
/// @param priority Prioritat del PDU a afegir
bool MACbuff_pushRx(mac_pdu_t& pdu, mac_buffer_priority_t priority);

/// @brief Retorna la mida de la cua de TX
/// @return Mida de la cua de TX
size_t MACbuff_getTxSize();

/// @brief Retorna la mida de la cua de RX
/// @return Mida de la cua de RX
size_t MACbuff_getRxSize();

#endif
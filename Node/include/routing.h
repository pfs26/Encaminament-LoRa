#ifndef _ROUTING_H
#define _ROUTING_H

#include <stdint.h>
#include "mac.h"
#include "node_address.h"
#include "routing_table.h"

#define ROUTING_MAX_TTL 5

#define ROUTING_HEADERS_SIZE (2+1+1) // 2 adreces + 1 ttl + 1 datalength
#define ROUTING_MAX_DATA_SIZE MAC_MAX_DATA_SIZE - ROUTING_HEADERS_SIZE

typedef uint8_t routing_data_t[ROUTING_MAX_DATA_SIZE];

typedef struct {
    node_address_t src;
    node_address_t dst;
    uint8_t ttl;
    uint8_t dataLength;
    routing_data_t data;
} routing_pdu_t;

typedef void (*routing_rx_callback_t)(void);
typedef void (*routing_tx_callback_t)(uint16_t); // no definir a mac_id_t, per si mai es canvia i no es vol utilitzar MAC
                                                 // l'únic requisit és que sigui uint16_t

enum routing_err_t {
    ROUTING_SUCCESS,
    ROUTING_ERR,
    ROUTING_ERR_NO_ROUTE,
    ROUTING_ERR_MAX_LENGTH,
};

bool Routing_init(node_address_t selfAddr, bool is_gateway);
void Routing_deinit();
routing_err_t Routing_send(node_address_t rx, const routing_data_t data, size_t length, uint16_t* id = nullptr);
node_address_t Routing_receive(routing_data_t* data, size_t* length);
void Routing_onReceive(routing_rx_callback_t cb);
void Routing_onSend(routing_tx_callback_t cb);
void Routing_onTxError(routing_tx_callback_t cb);

#endif
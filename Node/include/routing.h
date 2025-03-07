#ifndef _ROUTING_H
#define _ROUTING_H

#include <stdint.h>
#include "mac.h"
#include "routing_table.h"

#define ROUTING_MAX_TTL 5

#define ROUTING_HEADERS_SIZE 4
#define ROUTING_MAX_DATA_SIZE MAC_MAX_DATA_SIZE - ROUTING_HEADERS_SIZE

typedef uint8_t routing_data_t[ROUTING_MAX_DATA_SIZE];

typedef struct {
    routing_addr_t src;
    routing_addr_t dst;
    uint8_t ttl;
    uint8_t dataLength;
    routing_data_t data;
} routing_pdu_t;

typedef void (*routing_callback_t)(void);

enum routing_err_t {
    ROUTING_SUCCESS,
    ROUTING_ERR,
    ROUTING_ERR_NO_ROUTE,
    ROUTING_ERR_MAX_LENGTH,
};

bool Routing_init(routing_addr_t selfAddr, bool is_gateway);
void Routing_deinit();
routing_err_t Routing_send(routing_addr_t rx, const routing_data_t data, size_t length);
routing_addr_t Routing_receive(routing_data_t* data, size_t* length);

#endif
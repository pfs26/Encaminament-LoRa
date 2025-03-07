#ifndef _ROUTING_TABLE_H
#define _ROUTING_TABLE_H

#include <stdint.h>

typedef uint8_t routing_addr_t;
typedef struct {
    routing_addr_t dst;
    routing_addr_t nextHop;
} routing_entry_t;

bool RoutingTable_init();
void RoutingTable_deinit();



#endif
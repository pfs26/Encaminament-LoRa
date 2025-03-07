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

routing_addr_t RoutingTable_getRoute(routing_addr_t dst);
bool RoutingTable_addRoute(routing_addr_t dst, routing_addr_t nextHop);
bool RoutingTable_updateRoute(routing_addr_t dst, routing_addr_t nextHop);
bool RoutingTable_removeRoute(routing_addr_t dst);
bool RoutingTable_clear();
void RoutingTable_print();

#endif
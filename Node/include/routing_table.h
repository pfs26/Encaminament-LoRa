#ifndef _ROUTING_TABLE_H
#define _ROUTING_TABLE_H

#include <stdint.h>
#include "node_address.h"

typedef struct {
    node_address_t dst;
    node_address_t nextHop;
} routing_entry_t;

bool RoutingTable_init();
void RoutingTable_deinit();

node_address_t RoutingTable_getRoute(node_address_t dst);
bool RoutingTable_addRoute(node_address_t dst, node_address_t nextHop);
bool RoutingTable_updateRoute(node_address_t dst, node_address_t nextHop);
bool RoutingTable_removeRoute(node_address_t dst);
bool RoutingTable_clear();
void RoutingTable_print();

#endif
#ifndef _NODE_ADDRESS_H_
#define _NODE_ADDRESS_H

#include <stdint.h>

#define NODE_ADDRESS_NULL 0x00
// No utilitzat; per possibles millores futures; tot i això, filtrat implementat
#define NODE_ADDRESS_BROADCAST 0xFF 

typedef uint8_t node_address_t;

#define IS_ADDRESS_VALID(addr) ((addr) == NODE_ADDRESS_NULL || (addr) == NODE_ADDRESS_BROADCAST)

#endif
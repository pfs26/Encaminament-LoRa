#ifndef _NODE_ADDRESS_H_
#define _NODE_ADDRESS_H

#include <stdint.h>

#define NODE_ADDRESS_NULL 0x00
// No utilitzat; per possibles millores futures; tot i això, filtrat implementat
#define NODE_ADDRESS_BROADCAST 0xFF 

// Adreça simulada que utiltiza el gateway de lora
// Nodes amb rol de gateway filtraran RX = 0x01 per enviar per LoRaWAN
#define NODE_ADDRESS_GATEWAY 0x01

typedef uint8_t node_address_t;

#define IS_ADDRESS_VALID(addr) ((addr) != NODE_ADDRESS_NULL && (addr) != NODE_ADDRESS_BROADCAST && (addr) != NODE_ADDRESS_GATEWAY)

#endif
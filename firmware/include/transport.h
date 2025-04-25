#ifndef _TRANSPORT_H
#define _TRANSPORT_H

#include "routing.h"


// Ports màxims (2^6-1 = 63)
#define TRANSPORT_MAX_PORT 63

#define TRANSPORT_HEADER_SIZE (2+1+1) // 2 d'ID + 1 flags + 1 dataLength
#define TRANSPORT_MAX_DATA_SIZE ROUTING_MAX_DATA_SIZE - TRANSPORT_HEADER_SIZE

typedef uint8_t transport_data_t[TRANSPORT_MAX_DATA_SIZE]; 
typedef uint16_t transport_id_t;
typedef uint8_t transport_port_t;

typedef struct {    
    uint8_t ACKRequest: 1;
    uint8_t ACKResponse: 1;
    uint8_t port: 6; // 64 possibles aplicacions (0x00 pot ser control, per exemple)
} transport_pdu_flags_t;

typedef struct {
    transport_id_t ID;
    transport_pdu_flags_t flags;
    uint8_t dataLength; 
    transport_data_t data;
} transport_pdu_t;

typedef void (*transport_callback_t)(void);

enum transport_err_t {
    TRANSPORT_SUCCESS,
    TRANSPORT_ERR,
    TRANSPORT_ERR_NO_ACK,
    TRANSPORT_ERR_MAX_LENGTH,
};

bool Transport_init(node_address_t selfAddr, bool is_gateway);
void Transport_deinit(transport_port_t port);
transport_err_t Transport_send(node_address_t rx, transport_port_t port, const transport_data_t data, size_t length, bool ackRequested);
node_address_t Transport_receive(transport_port_t* port, transport_data_t* data, size_t* length);
bool Transport_onEvent(transport_port_t port, 
                       transport_callback_t onReceive = nullptr, 
                       transport_callback_t onSend = nullptr, 
                       transport_callback_t onSendError = nullptr);

#endif
#ifndef _TRANSPORT_H
#define _TRANSPORT_H

#include "routing.h"

// Nombre màxim de reintents si no es rep ACK (i sol·licitat)
#define TRANSPORT_MAX_RETRIES 3 
/* Time before an ACK timeout; it shuold consider payload length (including overhead of headers),
datarate, maximum number of hops, delay between hops (TX+ACK+possible retries), and any other overheads
After each retry, the delay is doubled, applying an exponential backoff; thus, if the maximum retries
is set to 5 (6 total attempts), and the initial timeout is 1second, on the last attempt the timeout
will be set to 1*2^(6-1) sec. The real timeout follows `tout(attempt) = tout_base * 2^(attempt-1)` 
Value is specified in ms! */
#define TRANSPORT_RETRY_DELAY 5000 

// Mida de cua d'últims segments rebuts, per filtrar repeticions
#define TRANSPORT_QUEUE_SIZE 10

#define TRANSPORT_HEADER_SIZE (2+1+1) // 2 d'ID + 1 flags + 1 dataLength
#define TRANSPORT_MAX_DATA_SIZE ROUTING_MAX_DATA_SIZE - TRANSPORT_HEADER_SIZE

typedef uint8_t transport_data_t[TRANSPORT_MAX_DATA_SIZE]; 
typedef uint16_t transport_id_t;

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
void Transport_deinit();
transport_err_t Transport_send(node_address_t rx, uint8_t port, const transport_data_t data, size_t length, bool ackRequested);
node_address_t Transport_receive(uint8_t* port, transport_data_t* data, size_t* length);
void Transport_onReceive(transport_callback_t cb);
void Transport_onSend(transport_callback_t cb);

#endif
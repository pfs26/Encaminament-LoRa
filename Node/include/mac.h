#ifndef _MAC_H
#define _MAC_H

#include <stdint.h>
#include "lora.h"

// Número màxim de reintents, sense comptar primera transmissió (així, serien 4 intents)
#define MAC_MAX_RETRIES 3

// NO CANVIA DINÀMICAMENT STRUCT DE MAC_DATA_T
// UTILITZAT NOMÉS PER SABER MIDA MÀXIMA DE DADES
#define MAC_ID_SIZE 2       // bytes utilitzats per ID
    #define MAC_MAX_ID ((1<<16)-1)
#define MAC_ADDRESS_SIZE 1  // bytes per cada adreça
#define MAC_CRC_SIZE 1      // bytes per FEC

#define MAC_MAX_DATA_SIZE LORA_MAX_SIZE - 2*MAC_ADDRESS_SIZE - MAC_ID_SIZE -MAC_CRC_SIZE  // @tx + @rx + crc + 2*id

typedef uint8_t mac_addr_t;
typedef uint8_t mac_crc_t;
typedef uint16_t mac_id_t;
// Estructura dades capa MAC
typedef struct {
    mac_addr_t tx;
    mac_addr_t rx;
    mac_id_t id;
    uint8_t data[MAC_MAX_DATA_SIZE];
    mac_crc_t crc;

    uint8_t length; // Mida del camp de dades (entre 0 i MAC_MAC_DATA_SIZE)
} mac_pdu_t;

typedef uint8_t mac_data_t[MAC_MAX_DATA_SIZE];

typedef uint8_t mac_err_t;
#define MAC_ERR_SUCCESS         0
#define MAC_ERR                 -1
#define MAC_ERR_INVALID_ADDR    1
#define MAC_ERR_MAX_RETRIES     2

typedef void (*mac_callback_t)();

bool MAC_init(mac_addr_t selfAddr, bool is_gateway);
bool MAC_deinit();

mac_err_t MAC_send(mac_addr_t rx, const mac_data_t data, uint8_t lenght);
mac_addr_t MAC_receive(mac_data_t data, uint8_t* length);

void MAC_onReceive(mac_callback_t cb);
void MAC_onSend(mac_callback_t cb);

#endif
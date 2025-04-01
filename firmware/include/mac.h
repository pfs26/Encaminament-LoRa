#ifndef _MAC_H
#define _MAC_H

#include <stdint.h>
#include "lora.h"
#include "node_address.h"
#include "config.h"


// Increment de potència per cada retransmissió. Operació en float, després ja es converteix a int
#define MAC_TX_POW_STEP ((float)(LORA_MAX_TX_POW - LORA_TX_POW) / MAC_MAX_RETRIES)

// NO CANVIA DINÀMICAMENT STRUCT DE MAC_DATA_T
// UTILITZAT NOMÉS PER SABER MIDA MÀXIMA DE DADES
#define MAC_ID_SIZE 2       // bytes utilitzats per ID
#define MAC_ADDRESS_SIZE 1  // bytes per cada adreça
#define MAC_CRC_SIZE 1      // bytes per FEC
#define MAC_FLAGS_SIZE 1    // bytes per flags
#define MAC_LENGTH_FIELD_SIZE 1
#define MAC_PDU_HEADER_SIZE (2*MAC_ADDRESS_SIZE + MAC_ID_SIZE + MAC_CRC_SIZE + MAC_FLAGS_SIZE + MAC_LENGTH_FIELD_SIZE)
#define MAC_MAX_DATA_SIZE LORA_MAX_SIZE - MAC_PDU_HEADER_SIZE // @tx + @rx + crc + id + flags + lengthField

typedef uint8_t mac_crc_t;
typedef uint16_t mac_id_t;
typedef uint8_t mac_data_t[MAC_MAX_DATA_SIZE];


// Estructura dades capa MAC
typedef struct {
    uint8_t isACK : 1;    // 0 = Data, 1 = ACK
    uint8_t retry : 2;    // Valor reintents (0-3)
    // uint8_t priority : 1; // Prioritat
    // uint8_t frag : 1;     // 1 = Fragmentat
    uint8_t reserved : 5; // Reservat ús futur i ocupar tot el byte
} mac_pdu_flags_t;

typedef struct {
    node_address_t tx;
    node_address_t rx;
    mac_id_t id;
    mac_pdu_flags_t flags;
    uint8_t dataLength; // Mida de dades. No podem utilitzar el '\0' com a separador, ja que potser capes superiors l'utilitzen al header o en mig de dades
    mac_data_t data; // potser uint8_t data[MAC_MAX_DATA_SIZE+1];, per deixar de marge el caràcter final '\0' -> Ja no si tenim datalength
    mac_crc_t crc;
} mac_pdu_t;


enum mac_err_t{
    MAC_SUCCESS,
    MAC_ERR,
    MAC_ERR_INVALID_ADDR,
    MAC_ERR_MAX_RETRIES,
    MAC_ERR_MAX_LENGTH,
    MAC_ERR_TX_PENDING
};


typedef void (*mac_rx_callback_t)();
// propagar un identificador de 16 bits; no s'utilitza `mac_id_t` per compatibilitat amb capes més altes
// ja que així no cal incloure mac; es queda fixat a 16 bits, i si mai es modifica mida de mac_id_t
// no hauria de suposar un problema si es veu aquest identificador com un de diferent
typedef void (*mac_tx_callback_t)(uint16_t); 

bool MAC_init(node_address_t selfAddr);
void MAC_deinit();

// ID és un identificador pel frame enviat per ser utilitzat per capes superiors per fer-ne seguiment
// **NO** té perquè ser igual al mac_id_t!
mac_err_t MAC_send(node_address_t rx, const mac_data_t data, size_t length, uint16_t* ID = nullptr);
node_address_t MAC_receive(mac_data_t* data, size_t* length);
size_t MAC_toReceive();
bool MAC_isAvailable();
void MAC_onReceive(mac_rx_callback_t cb);
void MAC_onSend(mac_tx_callback_t cb);
void MAC_onTxFailed(mac_tx_callback_t cb);


#endif
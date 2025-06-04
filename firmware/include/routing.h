#ifndef _ROUTING_H
#define _ROUTING_H

#include <stdint.h>
#include "mac.h"
#include "node_address.h"
#include "routing_table.h"


#define ROUTING_HEADERS_SIZE (2+1+1) // 2 adreces + 1 ttl + 1 datalength
#define ROUTING_MAX_DATA_SIZE MAC_MAX_DATA_SIZE - ROUTING_HEADERS_SIZE

typedef uint8_t routing_data_t[ROUTING_MAX_DATA_SIZE];

typedef struct {
    node_address_t src;
    node_address_t dst;
    uint8_t ttl;
    uint8_t dataLength;
    routing_data_t data;
} routing_pdu_t;

typedef void (*routing_rx_callback_t)(void);
typedef void (*routing_tx_callback_t)(uint16_t); // no definir a mac_id_t, per si mai es canvia i no es vol utilitzar MAC
                                                 // l'únic requisit és que sigui uint16_t

enum routing_err_t {
    ROUTING_SUCCESS,
    ROUTING_ERR,
    ROUTING_ERR_NO_ROUTE,
    ROUTING_ERR_MAX_LENGTH,
};

/// @brief Inicialitza capa d'encaminament. Inicialitza capa MAC i taula de rutes.
/// @param selfAddr Adreça del node actual
/// @param is_gateway Si té capacitats de gateway (true) o no (false)
/// @return `true` si s'ha inicialitzat correctament, `false` en cas contrari
bool Routing_init(node_address_t selfAddr, bool is_gateway);

/// @brief Desinicialitza capa d'encaminament. Desinicialitza capa MAC i taula de rutes.
void Routing_deinit();

/// @brief Envia un paquet a través de la capa d'encaminament.
/// @param rx Adreça del node receptor
/// @param data Dades a enviar
/// @param length Longitud de les dades a enviar
/// @param id ID del paquet enviat (opcional, pot ser `nullptr`)
routing_err_t Routing_send(node_address_t rx, const routing_data_t data, size_t length, uint16_t* id = nullptr);

/// @brief Obté el paquet rebut a través de la capa d'encaminament. S'ha d'executar després de ser notificat pel callback
/// @param data Dades del paquet rebut (s'ha d'inicialitzar abans de la crida)
/// @param length Longitud de les dades del paquet rebut (s'ha d'inicialitzar abans de la crida)
/// @return Adreça del node emissor del paquet rebut
node_address_t Routing_receive(routing_data_t* data, size_t* length);

/// @brief Configura el callback que s'executarà quan es rebi un paquet a través de la capa d'encaminament
/// @param cb Callback a executar quan es rebi un paquet
void Routing_onReceive(routing_rx_callback_t cb);

/// @brief Configura el callback que s'executarà quan s'enviï un paquet a través de la capa d'encaminament
/// @param cb Callback a executar quan s'enviï un paquet
void Routing_onSend(routing_tx_callback_t cb);

/// @brief Configura el callback que s'executarà quan hi hagi un error en l'enviament d'un paquet a través de la capa d'encaminament
/// @param cb Callback a executar quan hi hagi un error en l'enviament d'un paquet
void Routing_onTxError(routing_tx_callback_t cb);

#endif
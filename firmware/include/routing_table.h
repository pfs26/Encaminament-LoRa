#ifndef _ROUTING_TABLE_H
#define _ROUTING_TABLE_H

#include <stdint.h>
#include "node_address.h"

typedef struct {
    node_address_t dst;
    node_address_t nextHop;
} routing_entry_t;

/// @brief Inicialitza la taula de rutes, obtenint-la de NVS
/// @return `true`si s'ha pogut inicialitzar
bool RoutingTable_init();

/// @brief Desinicialitza la taula de rutes, alliberant la memòria. No esborra de NVS
void RoutingTable_deinit();

/// @brief Obtén la ruta per a un node de destí
/// @param dst L'adreça del node de destí
/// @return L'adreça del node següent en la ruta, o `NODE_ADDRESS_NULL` si no hi ha ruta
node_address_t RoutingTable_getRoute(node_address_t dst);

/// @brief Afegeix una ruta a la taula de rutes. Guarda a NVS
/// @param dst L'adreça del node de destí
/// @param nextHop L'adreça del node següent en la ruta
/// @return `true` si s'ha pogut afegir la ruta, `false` si ja existeix
bool RoutingTable_addRoute(node_address_t dst, node_address_t nextHop);

/// @brief Actualitza una ruta existent a la taula de rutes. La crea si no existeix. Guarda a NVS
/// @param dst L'adreça del node de destí
/// @param nextHop L'adreça del node següent en la ruta
/// @return `true` si s'ha pogut actualitzar la ruta, `false` si no existeix o no cal actualitzar
bool RoutingTable_updateRoute(node_address_t dst, node_address_t nextHop);

/// @brief Esborra una ruta de la taula de rutes. Guarda a NVS
/// @param dst L'adreça del node de destí
/// @return `true` si s'ha pogut esborrar la ruta, `false` si no existeix
bool RoutingTable_removeRoute(node_address_t dst);

/// @brief Esborra totes les rutes de la taula de rutes. Allibera memòria i esborra de NVS
/// @return `true` si s'ha pogut esborrar la taula de rutes, `false` si hi ha algun error
bool RoutingTable_clear();

/// @brief Imprimeix la taula de rutes a la consola. Per DEBUG.
void RoutingTable_print();

#endif
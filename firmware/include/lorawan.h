#ifndef _LORAWAN_H
#define _LORAWAN_H

#include <Arduino.h>
#include "lora.h"

/// @brief Inicialitza i configura LoRaWAN (OOTA i guardar info a NVS)
bool LW_init();

/// @brief Desinicialitzar LoRaWAN (desconnectar, i eliminar credencials LoRaWAN)
void LW_deinit();

/// @brief Enviar dades a través de LoRaWAN
/// @param data Dades a enviar
/// @param length Longitud de les dades a enviar
/// @param port Port a utilitzar per enviar les dades (per defecte, LW_DEFAULT_UPLINK_PORT)
/// @param confirmed Si s'ha d'esperar confirmació de l'uplink (per defecte, LW_CONFIRMED_UPLINKS)
bool LW_send(const lora_data_t data, size_t length, uint8_t port = LW_DEFAULT_UPLINK_PORT, bool confirmed = LW_CONFIRMED_UPLINKS);

/// @brief Retorna les dades guardades de l'últim downlink
/// @param data Dades rebudes (s'ha d'inicialitzar abans de la crida)
/// @param length Longitud de les dades rebudes (s'ha d'inicialitzar abans de la crida)
/// @param port Port de les dades rebudes (s'ha d'inicialitzar abans de la crida)
bool LW_receive(lora_data_t data, size_t *length, uint8_t *port);

/// @brief Comprovar si hi ha connexió establerta amb xarxa LoRaWAN
bool LW_isConnected();
    
//// @brief Configurar callback per recepció de dades a través de LW
void LW_onReceive(lora_callback_t cb);

#endif
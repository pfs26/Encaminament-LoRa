#ifndef _LORAWAN_H
#define _LORAWAN_H

#include <Arduino.h>
#include "lora_common.h"
#include "lora.h"

#define RADIOLIB_LORAWAN_JOIN_EUI  0x0000000000000000

// the Device EUI & two keys can be generated on the TTN console
#define RADIOLIB_LORAWAN_DEV_EUI   0x70B3D57ED006EF81
#define RADIOLIB_LORAWAN_APP_KEY   0x30, 0x45, 0xF3, 0xB9, 0xB1, 0xBC, 0xE5, 0xB5, 0xF9, 0x6A, 0xB4, 0xA7, 0x79, 0x39, 0x74, 0xE6
#define RADIOLIB_LORAWAN_NWK_KEY   0xC6, 0xC7, 0x8D, 0x65, 0xF8, 0xAD, 0x37, 0xBF, 0x6E, 0x9C, 0x52, 0x8D, 0x52, 0x63, 0x1F, 0x56

// Defineix si els uplinks esperaran confirmació o no
#define LW_CONFIRMED_UPLINKS 1
// Port utilitzat per defecte per enviar uplinks a lorawan
#define LW_DEFAULT_UPLINK_PORT 1



// Inicialitza i configura LoRaWAN (OOTA i guardar info a NVS)
bool LW_init();

// Desinicialitzar LoRaWAN (desconnectar, i eliminar credencials LoRaWAN)
void LW_deinit();

// Enviar dades a través de LoRaWAN
bool LW_send(const lora_data_t data, size_t length, uint8_t port = LW_DEFAULT_UPLINK_PORT, bool confirmed = LW_CONFIRMED_UPLINKS);

bool LW_receive(lora_data_t data, size_t *length, uint8_t *port);

// Comprovar si hi ha connexió establerta amb xarxa LoRaWAN
bool LW_isConnected();
    
// Configurar callback per recepció de dades a través de LW
void LW_onReceive(lora_callback_t cb);


#endif
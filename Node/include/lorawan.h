#ifndef _LORAWAN_H
#define _LORAWAN_H

#include <Arduino.h>
#include "lora_common.h"

// Inicialitza i configura LoRaWAN (OOTA i guardar info a NVS)
bool LW_init();

// Desinicialitzar LoRaWAN (desconnectar, i eliminar credencials LoRaWAN)
void LW_deinit();

// Enviar dades a través de LoRaWAN
bool LW_send(const lora_data_t data, size_t length);

// Comprovar si hi ha connexió establerta amb xarxa LoRaWAN
bool LW_isConnected();
    
// Configurar callback per recepció de dades a través de LW
void LW_onReceive(lora_callback_t cb);


#endif
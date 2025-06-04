/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

#ifndef _LORA_H
#define _LORA_H

#include <stdint.h>
#include "lora_common.h"
#include "loraraw.h"
#include "lorawan.h"

/// @brief Inicialitza ràdio LoRa, tant per WAN com RAW
bool LoRa_init(); 

/// @brief Desinicialitza ràdio LoRa, tant per WAN com RAW
void LoRa_deinit();

/// @brief Configura ràdio LoRa per mode RAW (rebre i enviar dades sense cap protocol)
void LoRa_setModeRAW();       

/// @brief Configura ràdio LoRa per mode WAN (rebre i enviar dades a través de LoRaWAN)
void LoRa_setModeWAN();  

#endif
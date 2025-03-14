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

bool LoRa_init(); 
void LoRa_deinit();
void LoRa_setModeRAW();       
void LoRa_setModeWAN();  

#endif
#ifndef _LORA_COMMON_H
#define _LORA_COMMON_H

#include <RadioLib.h>

// Potser moure a "config.h"
#define LORA_SS SS      
#define LORA_DIO1 2
#define LORA_NRESET 22
#define LORA_BUSY 4

#define LORA_FREQ 868.0
#define LORA_BW 125.0   // En kHz
#define LORA_DATARATE 5 // entre 0 i 7
#define LORA_SF 9
#define LORA_CODERATE 5 // Denominador de coderate. Valor entre 5 i 8, resultant en CR = [4/5, 4/6, 4/7, 4/8]
#define LORA_TX_POW -9  // en dBm, entre -9 i 22

// Radio de RadioLib utilitzada; definida LoRa
extern SX1262 radio;

// Mida màxima LoRa. No pot ser major a RADIOLIB_SX126X_MAX_PACKET_LENGTH
// Oju que potser dona problemes si mida màxima > mida màxima lorawan (en funció de SF)
// https://avbentem.github.io/airtime-calculator/ttn/eu868/223,12
#define LORA_MAX_SIZE RADIOLIB_SX126X_MAX_PACKET_LENGTH
typedef uint8_t lora_data_t[LORA_MAX_SIZE];
typedef void (*lora_callback_t)();

#endif
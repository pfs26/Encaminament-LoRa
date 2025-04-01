#ifndef _LORA_COMMON_H
#define _LORA_COMMON_H

#include <RadioLib.h>

// Errors en TX
enum lora_tx_error_t {
    LORA_ERROR_TX_MAX_LENGTH = -0xFF,   
    LORA_ERROR_TX_BUSY,         
    LORA_ERROR_TX_TIMEOUT,      
    LORA_ERROR_TX_PENDING,      
    LORA_ERROR = -1,                  
    LORA_SUCCESS,               
};

// Radio de RadioLib utilitzada;
extern SX1262 radio;
extern bool isLoraInitialized;

// Mida màxima LoRa. No pot ser major a RADIOLIB_SX126X_MAX_PACKET_LENGTH
// Oju que potser dona problemes si mida màxima > mida màxima lorawan (en funció de SF)
// https://avbentem.github.io/airtime-calculator/ttn/eu868/223,12
#define LORA_MAX_SIZE RADIOLIB_SX126X_MAX_PACKET_LENGTH
typedef uint8_t lora_data_t[LORA_MAX_SIZE];
typedef void (*lora_callback_t)();

#endif
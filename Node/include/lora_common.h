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

// Potser moure a "config.h"
#define LORA_SS SS      
#define LORA_DIO1 2
#define LORA_NRESET 22
#define LORA_BUSY 4

#define LORA_FREQ 868.0 // En MHz
#define LORA_BW 125.0   // En kHz
#define LORA_DATARATE 5 // entre 0 i 7
#define LORA_SF 7   // Entre 7 i 12 (a menor SF, major velocitat, però menor distància)
#define LORA_CODERATE 5 // Denominador de coderate. Valor entre 5 i 8, resultant en CR = [4/5, 4/6, 4/7, 4/8]
#define LORA_MAX_TX_POW 22 // en dBm, entre -9 i 22
#define LORA_TX_POW -9  // en dBm, entre -9 i 22



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
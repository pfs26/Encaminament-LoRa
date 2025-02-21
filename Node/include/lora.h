/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

#ifndef _LORA_H
#define _LORA_H

#include <stdint.h>
#include <RH_SX126x.h>

// ===== CONFIG =====
// Potser moure a "config.h"
#define LORA_SS SS
#define LORA_DIO1 7
#define LORA_BUSY 8
#define LORA_NRESET 9

#define LORA_FREQ 868
#define LORA_DATARATE 5 // entre 0 i 7
#define LORA_CODERATE 5 // Denominador de coderate. Valor entre 5 i 8, resultant en CR = [4/5, 4/6, 4/7, 4/8]
#define LORA_TX_POW 13
// ===== FI CONFIG =====

// Interval comprovació recepcions 
#define LORA_RCV_INTERVAL 1
// Mida màxima LoRa. No pot ser major a RH_SX126x_MAX_MESSAGE_LEN
#define LORA_MAX_SIZE RH_SX126x_MAX_MESSAGE_LEN

// Errors en TX
typedef int lora_tx_error_t;
#define LORA_SUCCESS                0
#define LORA_ERROR                  -1
#define LORA_ERROR_TX_MAX_LENGTH    1
#define LORA_ERROR_TX_BUSY          2
#define LORA_ERROR_TX_TIMEOUT       3

// Estructura dades lora
typedef struct {
    uint8_t data[LORA_MAX_SIZE];
    uint8_t length; // Mida de les dades
} lora_data_t;

// Callbacks
typedef void (*lora_callback_t)();

bool LoRa_init(); 
void LoRa_deinit();

lora_tx_error_t LoRa_send(const lora_data_t* data); 
bool LoRa_receive(lora_data_t* data, uint8_t& length);
bool LoRa_isAvailable();
bool LoRa_isBusy();
int16_t LoRa_getLastRSSI();
int16_t LoRa_getLastSNR();
bool LoRa_sleep();
bool LoRa_setFrequency(float frequency);
void LoRa_printDebug();


void LoRa_onReceive(lora_callback_t cb);
void LoRa_onSend(lora_callback_t cb);

#endif
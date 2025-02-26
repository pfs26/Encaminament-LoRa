/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

#ifndef _LORA_H
#define _LORA_H

#include <stdint.h>
#include <RadioLib.h>

// ===== CONFIG =====
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
#define LORA_TX_POW -2  // en dBm, entre -9 i 22
// ===== FI CONFIG =====

// Interval comprovació recepcions 
#define LORA_RCV_INTERVAL 1
// Mida màxima LoRa. No pot ser major a RADIOLIB_SX126X_MAX_PACKET_LENGTH
#define LORA_MAX_SIZE RADIOLIB_SX126X_MAX_PACKET_LENGTH

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
bool LoRa_receive(lora_data_t* data);
bool LoRa_isAvailable();
bool LoRa_isBusy();
int16_t LoRa_getLastRSSI();
int16_t LoRa_getLastSNR();
bool LoRa_sleep();
bool LoRa_setFrequency(float frequency);
bool LoRa_setTxPower(int power);
void LoRa_printDebug();

void LoRa_onReceive(lora_callback_t cb);
void LoRa_onSend(lora_callback_t cb);

#endif
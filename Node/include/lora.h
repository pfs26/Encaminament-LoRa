/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

#ifndef _LORA_H
#define _LORA_H

#include <stdint.h>
#include "lora_common.h"

// Interval comprovació recepcions 
#define LORA_IRQFLAGS_CHECK_INTERVAL 1 // interval comprovació flags IRQ tasca programada en ms

// Errors en TX
typedef int lora_tx_error_t;
#define LORA_SUCCESS                0
#define LORA_ERROR                  -1
#define LORA_ERROR_TX_MAX_LENGTH    1
#define LORA_ERROR_TX_BUSY          2
#define LORA_ERROR_TX_TIMEOUT       3
#define LORA_ERROR_TX_PENDING       4

bool LoRa_init(); 
void LoRa_deinit();

lora_tx_error_t LoRa_send(const lora_data_t data, size_t length); 
bool LoRa_receive(lora_data_t data, size_t* length);
bool LoRa_isAvailable();
bool LoRa_isBusy();
int16_t LoRa_getLastRSSI();
int16_t LoRa_getLastSNR();
bool LoRa_sleep();
bool LoRa_wakeup();
bool LoRa_setFrequency(float frequency);
bool LoRa_setTxPower(int power);
long LoRa_getTimeOnAir(int length);
void LoRa_startReceiving();

void LoRa_onReceive(lora_callback_t cb);

#endif
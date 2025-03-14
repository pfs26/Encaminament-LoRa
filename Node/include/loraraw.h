/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

#ifndef _LORARAW_H
#define _LORARAW_H

#include <stdint.h>
#include "lora_common.h"

// Interval comprovació recepcions 
#define LORA_IRQFLAGS_CHECK_INTERVAL 1 // interval comprovació flags IRQ tasca programada en ms

bool LoRaRAW_init();
void LoRaRAW_deinit();
lora_tx_error_t LoRaRAW_send(const lora_data_t data, size_t length); 
bool LoRaRAW_receive(lora_data_t data, size_t* length);
bool LoRaRAW_isAvailable();
bool LoRaRAW_isBusy();
int16_t LoRaRAW_getLastRSSI();
int16_t LoRaRAW_getLastSNR();
bool LoRaRAW_sleep();
bool LoRaRAW_wakeup();
bool LoRaRAW_setFrequency(float frequency);
bool LoRaRAW_setTxPower(int power);
long LoRaRAW_getTimeOnAir(int length);

void LoRaRAW_startReceiving();
void LoRaRAW_stopReceiving();

void LoRaRAW_onReceive(lora_callback_t cb);

#endif
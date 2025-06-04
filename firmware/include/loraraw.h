/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

#ifndef _LORARAW_H
#define _LORARAW_H

#include <stdint.h>
#include "lora_common.h"

// Interval comprovació recepcions, en `ms` 
#define LORA_IRQFLAGS_CHECK_INTERVAL 1

/// @brief Inicialitza i configura LoRa en mode RAW (sense LoRaWAN). Requereix `LoRa_init()`
bool LoRaRAW_init();

/// @brief  Desinicialitza LoRa en mode RAW (sense LoRaWAN).
void LoRaRAW_deinit();

/// @brief Envia dades a través de LoRa en mode RAW.
/// @param data Dades a enviar
/// @param length Longitud de les dades
/// @return lora_tx_error_t amb estat de transmissió
lora_tx_error_t LoRaRAW_send(const lora_data_t data, size_t length); 

/// @brief Obté les últimes dades rebudes per lora
/// @param data Apuntador a l'espai on guardar les dades rebudes
/// @param length Apuntador a la longitud de les dades rebudes
/// @return 
bool LoRaRAW_receive(lora_data_t data, size_t* length);

/// @brief Obté si es poden enviar dades pel canal, a través de CAD
/// @return `true` si es pot enviar dades, `false` si no
bool LoRaRAW_isAvailable();

/// @brief Mètode contrari a `LoRaRAW_isAvailable()`, retorna si el canal està ocupat
/// @return  `true` si el canal està ocupat
bool LoRaRAW_isBusy();

/// @brief Obté l últim RSSI (Receiver Signal Strength Indicator) mesurat
/// @return RSSI mesurat
int16_t LoRaRAW_getLastRSSI();

/// @brief Obté l'últim SNR (Signal-to-Noise Ratio) mesurat de l'últim missatge
/// @return SNR mesurat
int16_t LoRaRAW_getLastSNR();

/// @brief Posa la ràdio en mode de baix consum
/// @return `true` si correcte
bool LoRaRAW_sleep();

/// @brief Desperta la ràdio de baix consum
/// @return `true` si correcte
bool LoRaRAW_wakeup();

/// @brief Modifica la freqüència a utilitzar
/// @param frequency Freqüència a utilitzar en MHz
/// @return `true` si s'ha pogut canviar la freqüència, `false` si no
bool LoRaRAW_setFrequency(float frequency);

/// @brief Configura la potència de transmissió de LoRa
/// @param power Potència de transmissió en dBm
/// @return `true` si s'ha pogut canviar la potència, `false` si no
bool LoRaRAW_setTxPower(int power);

/// @brief Retorna el temps de transmissió d'un paquet en `us`, a partir del BW, SF, CR...
/// @param length Mida del paquet a transmetre
long LoRaRAW_getTimeOnAir(int length);

/// @brief Inicia la recepció de dades a través de LoRa en mode RAW
void LoRaRAW_startReceiving();

/// @brief Atura la recepció de dades a través de LoRa en mode RAW
void LoRaRAW_stopReceiving();

/// @brief Configura un callback que s'executarà quan es rebin dades a través de LoRa en mode RAW
void LoRaRAW_onReceive(lora_callback_t cb);

#endif
/*
    Abstraction to manage RadioLib's radio, providing a centralized
    place to synchronize both LoRaRAW and LoRaWAN.
    It also allows for easy LoRa transceivers swapping.
*/

#include "utils.h"
#include "lora.h"

bool isLoraInitialized = false;

// No static, ja que és la mateixa radio utilitzada pels dos LoRa (RAW i WAN)
SX1262 radio = new Module(LORA_SS, LORA_DIO1, LORA_NRESET, LORA_BUSY); 

bool LoRa_init() {
    /*  1. Inicialitza radiolib. 
        2. Configura LoRa a paràmetres configurats. */

    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CODERATE, LORA_SYNC_WORD, LORA_TX_POW);
    if(state != RADIOLIB_ERR_NONE) {
        _PE("[LORA] Error initializing radio: %d", state);
        return false;
    }
    isLoraInitialized = true;
    _PI("[LORA] Init");
    return true;
}

void LoRa_deinit() {
    _PI("[LORA] Deinit");
    isLoraInitialized = false;
    radio.reset();
}

void LoRa_setModeRAW() {
    // Mirant implementació de 'begin' es veu que no depèn de l'estat (si ja inicialtizat)
    // i únicament estableix els paràmetres proporcionats.
    // Configurarà també el SyncWord per defecte (@todo: potser mirar d'oferir opció de configuració?)
    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CODERATE, LORA_SYNC_WORD, LORA_TX_POW);
    if(state != RADIOLIB_ERR_NONE) {
        _PE("[LORA] Error setting mode RAW: %d", state);
    }
    
    LoRaRAW_startReceiving();
}   

void LoRa_setModeWAN() {
    LoRaRAW_stopReceiving();
}

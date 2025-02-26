/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

// Instal·lar versió .zip (i no des del gestor de llibreries)
// per tenir compatibilitat amb sx1262
#include <Arduino.h>

#include "utils.h"
#include "scheduler.h"
#include "lora.h"

void _received(void);
void _sent(void);
void _CADfinished(void);
void _handleInterrupts(void);

static lora_callback_t onReceive = NULL;
static lora_callback_t onSend = NULL;

volatile static bool transmitting = false;

static SX1262 radio = new Module(LORA_SS, LORA_DIO1, LORA_NRESET, LORA_BUSY); 

bool LoRa_init() {
    /*  1. Inicialitza radiolib. 
        2. Configura LoRa a paràmetres configurats.
        3. Inicialitza scheduler per comprovar recepció de missatges de forma periòdica */

    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CODERATE, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POW);
    
    if(state != RADIOLIB_ERR_NONE) {
        _PP("[LORA] ERROR: "); _PL(state);
        return false;
    }

    radio.setDio1Action(_handleInterrupts);
    // radio.setPacketReceivedAction(_received);
    // radio.setPacketSentAction(_sent);
    // radio.setChannelScanAction(_CADfinished);

    radio.startReceive();

    _PL("[LORA] Init");
    return true;
}

void LoRa_deinit() {
    _PL("[LORA] Deinit");
    radio.reset();
}


lora_tx_error_t LoRa_send(const lora_data_t* data) {
    /*
    Envia dades a través de LoRa utilitzant RadioHead.
    Si es pot programar enviament, bloqueja fins que s'han enviat.
        1. Comprova mida de les dades
        2. Comprova si el canal està ocupat
        3. Envia dades
        4. Espera que s'hagin enviat
    */
    _PL("[LORA] Send");

    if(data->length > LORA_MAX_SIZE) { 
        _PL("[LORA] TX MAX LENGTH");
        return LORA_ERROR_TX_MAX_LENGTH; 
    }

    // TODO: No bloquejar, retornar codi error específic
    while(transmitting) {
        _PL("pending tx");
    }

    if(!LoRa_isAvailable()) { 
        _PL("[LORA] TX CHAN BUSY");
        return LORA_ERROR_TX_BUSY; 
    }  

    // cli();
    // NO DESACTIVAR INTERRUPCIONS, SPI LES UTILITZA (i prints també)
    int state = radio.startTransmit(data->data, data->length);
    // sei();

    if(state != RADIOLIB_ERR_NONE) {
        _PL("[LORA] TX ERR");
        return LORA_ERROR; 
    }

    _PP("[LORA] TX QUEUE: "); _PL(state);
    transmitting = true; // flag per evitar múltiples TX i sobreescriure buffer de Tx de radio
    return LORA_SUCCESS;
} 

bool LoRa_receive(lora_data_t* data) {
    /*
    Obté les dades rebudes del driver.
    S'hauria d'executar dins de la implementació 
    del callback de recepeció configurat.
    */
    // RadioHead utilitza `length` per saber quants bytes de buffer rebuts copiar. Si és 0, no copiarà res.
    // S'ocuparà, com a màxim, tot el buffer de `data`. Length tindrà el valor correcte després de rebre
    data->length = radio.getPacketLength();
    int state = radio.readData(data->data, data->length);
    return state == RADIOLIB_ERR_NONE;
}

bool LoRa_isAvailable() {
    // TODO: Incloure TX en curs aquí?
    // return radio.scanChannel() == RADIOLIB_CHANNEL_FREE;
    if(radio.scanChannel() == RADIOLIB_CHANNEL_FREE) { 
        _PL("[LORA] TX CHAN FREE");
        return true; 
    }  
    return false;
}

bool LoRa_isBusy() {
    return !LoRa_isAvailable();
}

int16_t LoRa_getLastRSSI() {
    /*
    Retorna l'últim RSSI (Receiver Signal Strength Indicator)
    */
    return radio.getRSSI();
}

int16_t LoRa_getLastSNR() {
    /*
    Retorna l'últim SNR mesurat (pel receptor) de l'últim missatge
    */
    return radio.getSNR();
}

bool LoRa_sleep() {
    /*
    Posa la radio en mode de baix consum.
    Es desperta automàticament en posar-la en un altre mode (RX, TX, IDLE),
    a través de `LoRa_send()`, `LoRa_isAvailable()`
    */
    return radio.sleep();
}

bool LoRa_wakeup() {
    return radio.standby();
}

bool LoRa_setFrequency(float frequency) {
    /*
    Canvia la freq. de la radio. En decimal i MHz.
    Retorna true si s'ha pogut fer el canvi, false si no.
    */
    return radio.setFrequency(frequency) == RADIOLIB_ERR_NONE;
}

bool LoRa_setTxPower(int power) {
    int8_t checked_pow = 0;
    if(radio.checkOutputPower(power, &checked_pow))
        return false;
    return radio.setOutputPower(checked_pow) == RADIOLIB_ERR_NONE;
}

void LoRa_onReceive(lora_callback_t cb) {
    /*
    Permet registrar un callback que s'executarà quan es rebi alguna cosa
    */
    onReceive = cb;
}

void LoRa_onSend(lora_callback_t cb) {
    /*
    Permet registrar un callback que s'executarà quan s'acabi l'enviament
    */
    onSend = cb;
}


void LoRa_printDebug() {
    // _PL("===============");
    // _PL("[LORA] DEBUG");
    // _PP("\tMODE: "); _PL(radio.receive());
    // _PP("\tTX OK: "); _PL(driver.txGood());
    // _PP("\tRX OK: "); _PL(driver.rxGood());
    // _PP("\tRX BAD: "); _PL(driver.rxBad());
    // _PP("\tFreq Err: "); _PL((int) (driver.getFrequencyError()*1000));
    // _PL("===============");
}

void _startReceiving() {
    /*
    Iniciar mode de recepció. 
    Cal que no s'estigui transmetent, sinó no es podrà transmetre
    */
    if(transmitting) {
        // Si està en TX, prova-ho de nou en 10ms
        _PM("Sched new");
        scheduler_once(_startReceiving, 100);
        return;
    }
    radio.startReceive();
    _PM("[LORA] _startRcv");
}

void _handleInterrupts(void) {
    uint32_t flags = radio.getIrqFlags();
    _PP("Handle int: 0x"); _PX(flags>>16); _PP(" "); _PX(flags & 0xFF); _PL();
    // delay(20);
    if(flags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        _received();
    }
    if(flags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        _sent();
    }
    // TODO: Potser fa falta clear de flags?
}

void _received(void) {
    _PM("[LORA] _received");
    if (onReceive != NULL) {
        _PL("[LORA] onReceive");
        // Programar execució: recordar que s'executa dins de ISR, i és necessari sortir-ne ràpid
        scheduler_once(onReceive);
    }
    // Iniciar el receive requereix d'interrupcions, desactivades dins d'una ISR
    // i pot ser lent; programar execució FORA d'ISR.
    scheduler_once(_startReceiving);
}

void _sent(void) {
    _PM("[LORA] _sent()");
    if(onSend != NULL) {
        scheduler_once(onSend);
    }
    transmitting = false;
    scheduler_once(_startReceiving);
}
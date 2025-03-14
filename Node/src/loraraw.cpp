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
#include "loraraw.h"

void _received_lora(void);
void _onReceive(void);
int16_t _startReceiving();
void LoRaRAW_stopReceiving();
void _checkReceived(void);
void _printLora(const lora_data_t data, size_t length);

volatile bool received = false;
static lora_callback_t onReceive = nullptr;
static Task* checkIRQTask;

bool LoRaRAW_init() {
    /* Inicialitza scheduler per comprovar interrupcions de forma periòdica.
       Requereix LoRaRAW_init() previ per poder funcionar correctament! */

    if(!isLoraInitialized) {
        _PE("[LR] Lora not initialized, call LoRa_init() first");
        return false;
    }

    // ISR a executar que es dona interrupció de DIO1. NOMES es gestiona RxDone
    // Tasca que comprova flags obtinguts per ISR de forma recurrrent (per no bloquejar ISR)
    checkIRQTask = scheduler_infinite(LORA_IRQFLAGS_CHECK_INTERVAL, _checkReceived);
    // Iniciem en mode de recepció per defecte

    _startReceiving();

    _PI("[LR] Init");
    return true;
}

void LoRaRAW_deinit() {
    _PI("[LR] Deinit");
    onReceive = nullptr;
    scheduler_stop(checkIRQTask);
}


lora_tx_error_t LoRaRAW_send(const lora_data_t data, size_t length) {
    _PI("[LR] Preparing to send");
    
    LoRaRAW_stopReceiving();

    if(length > LORA_MAX_SIZE) { 
        _PW("[LR] Max length exceeded (length = %d)", length);
        _startReceiving();
        return LORA_ERROR_TX_MAX_LENGTH; 
    }

    if(!LoRaRAW_isAvailable()) { 
        _PW("[LR] Channel busy");
        _startReceiving();
        return LORA_ERROR_TX_BUSY; 
    }  

    _printLora(data, length);

    int16_t state = radio.transmit(data, length);        

    _startReceiving();

    if(state != RADIOLIB_ERR_NONE) {
        _PE("[LR] Error transmitting (code = %d)", state);
        return LORA_ERROR; 
    }
    return LORA_SUCCESS;
} 

bool LoRaRAW_receive(lora_data_t data, size_t* length) {
    /*
    Obté les dades rebudes del driver.
    S'hauria d'executar dins de la implementació del callback de recepeció configurat.
    Si hi ha més dades de les que hi caben al buffer de `data`, es tallaran
    */
    *length = radio.getPacketLength();
    
    if (*length > LORA_MAX_SIZE) {
        _PW("[LR] Length received (%d) trimmed to %d", *length, LORA_MAX_SIZE);
        *length = LORA_MAX_SIZE;
    }     

    int state = radio.readData(data, *length);
    
    _startReceiving();

    if (state != RADIOLIB_ERR_NONE) {
        _PW("[LR] Error reading data (code = %d)", state);
        return false;
    }

    return state == RADIOLIB_ERR_NONE;
}

bool LoRaRAW_isAvailable() {
    // DESACTIVAR INTERRUPCIONS, O GENERARÀ INTERRUPCIONS QUE NO TOQUEN!
    LoRaRAW_stopReceiving();
    int16_t result = radio.scanChannel();
    // TODO: Potser fa falta _startReceiving()?
    // _startReceiving();
    if(result == RADIOLIB_CHANNEL_FREE) { 
        return true; 
    }  
    return false;
}

bool LoRaRAW_isBusy() { return !LoRaRAW_isAvailable(); }

/* Retorna l'últim RSSI (Receiver Signal Strength Indicator) */
int16_t LoRaRAW_getLastRSSI() { return radio.getRSSI(); }

/* Retorna l'últim SNR mesurat (pel receptor) de l'últim missatge */
int16_t LoRaRAW_getLastSNR() { return radio.getSNR(); }

/* Posa la radio en mode de baix consum. */
bool LoRaRAW_sleep() { return radio.sleep(); }

bool LoRaRAW_wakeup() { return _startReceiving(); }

/*
Canvia la freq. de la radio. En decimal i MHz.
Retorna true si s'ha pogut fer el canvi, false si no.
*/
bool LoRaRAW_setFrequency(float frequency) { return radio.setFrequency(frequency) == RADIOLIB_ERR_NONE; }

bool LoRaRAW_setTxPower(int power) {
    int8_t checked_pow = 0;
    if(radio.checkOutputPower(power, &checked_pow))
        return false;
    return radio.setOutputPower(checked_pow) == RADIOLIB_ERR_NONE;
}

long LoRaRAW_getTimeOnAir(int length) {
    return radio.getTimeOnAir(length);
}

void LoRaRAW_startReceiving() {
    _startReceiving();
}

void LoRaRAW_stopReceiving() { radio.clearDio1Action(); }

/*
Permet registrar un callback que s'executarà quan es rebi alguna cosa
*/
void LoRaRAW_onReceive(lora_callback_t cb) { onReceive = cb; }

int16_t _startReceiving() {
    // Activar interrupció en recepció
    radio.setDio1Action(_onReceive);

    // Posar radio en mode recepció
    int16_t state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        _PW("[LR] Couldn't start receiving (code = %d)", state);
        // TODO: Potser és interessant fer reset de radio, per si queda bloquejada?
        return _startReceiving();
    }
    _PI("[LR] Started Receiving");
    return state;
}

// per guardar ISR a RAM per accés més ràpid
ICACHE_RAM_ATTR
void _onReceive(void) {
    // Si es gestionen interrupcions
    // aquí (amb els callbacks), es generen crashes a l'ESP.
    // El temps d'execució dins d'ISR passa a ser molt alt i peta.
    // (Fins i tot si no s'executa i es programa amb scheduler)
    // S'evita fent que aquí únicament es guardin els flags, sortint-ne ràpid.
    // S'inicia una tasca en iniciar que comprova els flags cada pocs ms
    // dins de LOOP, evitant bloquejar ISR.

    received = true;
}

void _checkReceived(void) {
    if (received) {
        received = false;
        _received_lora();
    }
}

void _received_lora(void) {
    /*
    Executat quan es produeix interrupció
    de recepció.
    Comprova si hi ha CB configurat i l'executa.
    Torna a posar-se en mode de recepció, ja que després 
    de fer lectura de les dades rebudes es posa en mode standby.
    */
    _PI("[LR] Data received. SNR: %d, RSSI: %d", LoRaRAW_getLastSNR(), LoRaRAW_getLastRSSI());
    if (onReceive != nullptr) {
        onReceive();
        // scheduler_once(onReceive);
    }
    // _startReceiving();
}

#if LOG_LEVEL <= LOG_LEVEL_INFO
void _printLora(const lora_data_t data, size_t length) {
    Serial.print("[LR] Packet's data: ");
    for (uint8_t i = 0; i < length; ++i) {
        Serial.printf("%02X", ((char*)data)[i]);
    }
    Serial.printf(" (%d)", length);
    Serial.println();
}
#else
void _printLora(const lora_data_t data, size_t length) {}
#endif
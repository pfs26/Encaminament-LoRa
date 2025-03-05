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

void _received_lora(void);
void _onReceive(void);
int16_t _startReceiving();
void _clearInterrupts();
void _checkReceived(void);

volatile bool received = false;
// void _sent_lora(void);
// void _checkIRQFlags(void);

static lora_callback_t onReceive = NULL;
// static lora_callback_t onSend = NULL;

// volatile static bool transmitting = false;

static Task* checkIRQTask;
// volatile uint32_t IRQFlags = 0;

static SX1262 radio = new Module(LORA_SS, LORA_DIO1, LORA_NRESET, LORA_BUSY); 

bool LoRa_init() {
    /*  1. Inicialitza radiolib. 
        2. Configura LoRa a paràmetres configurats.
        3. Inicialitza scheduler per comprovar interrupcions de forma periòdica */

    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CODERATE, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POW);
    if(state != RADIOLIB_ERR_NONE) {
        _PE("[LORA] %d", state);
        return false;
    }
    // ISR a executar que es dona interrupció de DIO1. NOMES es gestiona RxDone
    // Tasca que comprova flags obtinguts per ISR de forma recurrrent (per no bloquejar ISR)
    checkIRQTask = scheduler_infinite(LORA_IRQFLAGS_CHECK_INTERVAL, _checkReceived);
    // Iniciem en mode de recepció per defecte

    _startReceiving();
    // radio.startReceive();

    _PI("[LORA] Init");
    return true;
}

void LoRa_deinit() {
    _PI("[LORA] Deinit");
    scheduler_stop(checkIRQTask);
    radio.reset();
}


lora_tx_error_t LoRa_send(const lora_data_t data, size_t length) {
    _PI("[LORA] Preparing to send");
    
    _clearInterrupts();

    if(length > LORA_MAX_SIZE) { 
        _PW("[LORA] Max length exceeded (length = %d)", length);
        _startReceiving();
        return LORA_ERROR_TX_MAX_LENGTH; 
    }

    if(!LoRa_isAvailable()) { 
        _PW("[LORA] Channel busy");
        _startReceiving();
        return LORA_ERROR_TX_BUSY; 
    }  

    _printLora(data, length);

    int16_t state = radio.transmit(data, length);        

    _startReceiving();

    if(state != RADIOLIB_ERR_NONE) {
        _PE("[LORA] Error transmitting (code = %d)", state);
        return LORA_ERROR; 
    }
    return LORA_SUCCESS;
} 

bool LoRa_receive(lora_data_t data, size_t* length) {
    /*
    Obté les dades rebudes del driver.
    S'hauria d'executar dins de la implementació 
    del callback de recepeció configurat.
    Si hi ha més dades de les que hi caben al buffer de `data`, es tallaran
    */
    *length = radio.getPacketLength();
    
    if (*length > LORA_MAX_SIZE) {
        _PW("[LORA] Length received (%d) trimmed to %d", *length, LORA_MAX_SIZE);
        *length = LORA_MAX_SIZE;
    }     

    int state = radio.readData(data, *length);
    
    _startReceiving();

    if (state != RADIOLIB_ERR_NONE) {
        _PW("[LORA] Error reading data (code = %d)", state);
        return false;
    }

    return state == RADIOLIB_ERR_NONE;
}

bool LoRa_isAvailable() {
    // DESACTIVAR INTERRUPCIONS, O GENERARÀ INTERRUPCIONS QUE NO TOQUEN!
    // TODO: Incloure TX en curs aquí?
    // return radio.scanChannel() == RADIOLIB_CHANNEL_FREE;
    _clearInterrupts();
    int16_t result = radio.scanChannel();
    // TODO: Potser fa falta _startReceiving()?
    // _startReceiving();
    if(result == RADIOLIB_CHANNEL_FREE) { 
        return true; 
    }  
    return false;
}

bool LoRa_isBusy() { return !LoRa_isAvailable(); }

/*
Retorna l'últim RSSI (Receiver Signal Strength Indicator)
*/
int16_t LoRa_getLastRSSI() { return radio.getRSSI(); }

/*
Retorna l'últim SNR mesurat (pel receptor) de l'últim missatge
*/
int16_t LoRa_getLastSNR() { return radio.getSNR(); }

/*
Posa la radio en mode de baix consum.
Es desperta automàticament en posar-la en un altre mode (RX, TX, IDLE),
a través de `LoRa_send()`, `LoRa_isAvailable()`
*/
bool LoRa_sleep() { return radio.sleep(); }

bool LoRa_wakeup() { return _startReceiving(); }

/*
Canvia la freq. de la radio. En decimal i MHz.
Retorna true si s'ha pogut fer el canvi, false si no.
*/
bool LoRa_setFrequency(float frequency) { return radio.setFrequency(frequency) == RADIOLIB_ERR_NONE; }

bool LoRa_setTxPower(int power) {
    int8_t checked_pow = 0;
    if(radio.checkOutputPower(power, &checked_pow))
        return false;
    return radio.setOutputPower(checked_pow) == RADIOLIB_ERR_NONE;
}

long LoRa_getTimeOnAir(int length) {
    return radio.getTimeOnAir(length);
}

void LoRa_startReceiving() {
    _startReceiving();
}

/*
Permet registrar un callback que s'executarà quan es rebi alguna cosa
*/
void LoRa_onReceive(lora_callback_t cb) { onReceive = cb; }

int16_t _startReceiving() {
    // Activar interrupció en recepció
    radio.setDio1Action(_onReceive);

    // Posar radio en mode recepció
    int16_t state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        _PW("[LORA] Couldn't start receiving (code = %d)", state);
        // TODO: Potser és interessant fer reset de radio, per si queda bloquejada?
        return _startReceiving();
    }
    _PI("[LORA] Started Receiving");
    return state;
}

void _clearInterrupts() { radio.clearDio1Action(); }

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

    // OR per no perdre interrupcions. Sino, si es generen dues interrupcions ABANS que checkIRQflags ho gestioni, la primer es perd.
    // Ja es posaran a 0 després que checkIRQFlags les comprovi.
    // IRQFlags |= radio.getIrqFlags();
    // _PI("[LORA] DIO1 INT");
    received = true;
    // scheduler_once(_received_lora);
}

void _checkReceived(void) {
    if (received) {
        _received_lora();
        received = false;
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
    _PI("[LORA] Running _received()");
    if (onReceive != NULL) {
        onReceive();
        // scheduler_once(onReceive);
    }
    // _startReceiving();
}

#if LOG_LEVEL <= LOG_LEVEL_INFO
void _printLora(const lora_data_t data, size_t length) {
    Serial.print("[LORA] Packet's data: ");
    for (uint8_t i = 0; i < length; ++i) {
        Serial.printf("%02X", ((char*)data)[i]);
    }
    Serial.printf(" (%d)", length);
    Serial.println();
}
#else
void _printLora(const lora_data_t data, size_t length) {}
#endif
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
void _sent_lora(void);
void _handleInterrupts(void);
void _checkIRQFlags(void);

static lora_callback_t onReceive = NULL;
static lora_callback_t onSend = NULL;

volatile static bool transmitting = false;

static Task* checkIRQTask;
volatile uint32_t IRQFlags = 0;

static SX1262 radio = new Module(LORA_SS, LORA_DIO1, LORA_NRESET, LORA_BUSY); 

bool LoRa_init() {
    /*  1. Inicialitza radiolib. 
        2. Configura LoRa a paràmetres configurats.
        3. Inicialitza scheduler per comprovar interrupcions de forma periòdica */

    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CODERATE, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POW);
    if(state != RADIOLIB_ERR_NONE) {
        _PP("[LORA] ERROR: "); _PL(state);
        return false;
    }
    // ISR a executar que es dona interrupció de DIO1 (txDone, rxDone, etc.)
    radio.setDio1Action(_handleInterrupts);
    // Tasca que comprova flags obtinguts per ISR de forma recurrrent (per no bloquejar ISR)
    checkIRQTask = scheduler_infinite(LORA_IRQFLAGS_CHECK_INTERVAL, _checkIRQFlags);
    // Iniciem en mode de recepció per defecte
    radio.startReceive();

    _PL("[LORA] Init");
    return true;
}

void LoRa_deinit() {
    _PL("[LORA] Deinit");
    scheduler_stop(checkIRQTask);
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

    if(transmitting)
        return LORA_ERROR_TX_PENDING;

    if(!LoRa_isAvailable()) { 
        _PL("[LORA] TX CHAN BUSY");
        return LORA_ERROR_TX_BUSY; 
    }  

    _PP("[LORA] DATA TO SEND: ");
    _printLora(data);
    // startTransmit ja copia les dades a buffer de sx1262
    // NO DESACTIVAR INTERRUPCIONS, SPI LES UTILITZA (i prints també)
    int state = radio.startTransmit(data->data, data->length);

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
    Si hi ha més dades de les que hi caben al buffer de `data`, es tallaran
    */
    data->length = radio.getPacketLength();
    if(data->length > LORA_MAX_SIZE) 
        data->length = LORA_MAX_SIZE-1; // -1 ja que últim és NULL
    int state = radio.readData(data->data, data->length);
    
    // TODO: Hauria realment de ser '\0' l'últim, si el valor de length ja és el que toca?
    data->data[data->length] = '\0'; // escrivim l'últim a '\0' 
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

long LoRa_getTimeOnAir(int length) {
    return radio.getTimeOnAir(length);
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

void _startReceiving() {
    /*
    Iniciar mode de recepció. 
    Cal que no s'estigui transmetent, sinó no es podrà transmetre.
    En el cas d'haver un TX pendent, programa l'execució d'aquesta funció
    al cap de pocs ms, fins que s'ha acabat TX.
    Això evita bloquejar el fil principal (i per tant que no s'executin 
    tasques programades)
    */
    if(transmitting) {
        // Si està en TX, retornar. Quan es genera interrupció fi TX ja es posa
        // en startReceiving
        // scheduler_once(_startReceiving, 100);
        return;
    }
    radio.startReceive();
    _PL("[LORA] _startRcv");
}

void _handleInterrupts(void) {
    // Si es gestionen interrupcions
    // aquí (amb els callbacks), es generen crashes a l'ESP.
    // El temps d'execució dins d'ISR passa a ser molt alt i peta.
    // (Fins i tot si no s'executa i es programa amb scheduler)
    // S'evita fent que aquí únicament es guardin els flags, sortint-ne ràpid.
    // S'inicia una tasca en iniciar que comprova els flags cada pocs ms
    // dins de LOOP, evitant bloquejar ISR.

    IRQFlags = radio.getIrqFlags();
}

void _checkIRQFlags(void) {
    /*
    Executat a través del gestor de tasques de forma recurrent
    després d'iniciar-se el mòdul.
    Comprova els flags i actua conseqüentment.
    Implementat així per executar-se a través de LOOP,
    com si fossin interrupcions per software.
    Evita bloquejar ISR.
    */
    // Bloc atòmic?
    if(!IRQFlags) // retorn prematur si no flags
        return;

    _PF("[LORA] Handle int: 0x%04X\n", IRQFlags);
    // _PP("[LORA] Handle int: 0x"); _PX(IRQFlags>>16); _PP(" "); _PX(IRQFlags & 0xFF); _PL();
    if (IRQFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        _sent_lora();
    }
    if (IRQFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        _received_lora();
    }
    IRQFlags = 0;
}

void _received_lora(void) {
    /*
    Executat quan es produeix interrupció
    de recepció.
    Comprova si hi ha CB configurat i l'executa.
    Torna a posar-se en mode de recepció, ja que després 
    de fer lectura de les dades rebudes es posa en mode standby.
    */
    _PL("[LORA] _received");
    if (onReceive != NULL) {
        _PL("[LORA] onReceive");
        onReceive();
        // scheduler_once(onReceive);
    }
    _startReceiving();
}

void _sent_lora(void) {
    /*
    Executat quan es produeix interrupció de fi TX.
    Comprova si hi ha CB configurat i l'executa.
    Posa flag de TX en curs a fals. Torna a posar-se en mode de recepció,
    ja que després d'enviar està en standby.
    */
    _PL("[LORA] _sent()");
    transmitting = false; // posar al principi. Si onSend volgués tornar a enviar, no podria
    if(onSend != NULL) {
        // scheduler_once(onSend);
        onSend();
    }
    _startReceiving();
}

void _printLora(const lora_data_t* const data) {
    for (uint8_t i = 0; i < data->length; ++i) {
        _PX(((char*)data)[i]);
    }
    _PL();
}
/*
LoRa Layer (Physical) implementation.

Abstraction to RadioHead library, making it easier to
change LoRa transceivers.
*/

// Instal·lar versió .zip (i no des del gestor de llibreries)
// per tenir compatibilitat amb sx1262

#include "config.h"
#include "lora.h"
#include "scheduler.h"

void _check_received();
void _check_sent();
bool _setModemConfig(uint8_t datarate, uint8_t code_rate_denom = LORA_CODERATE);

static lora_callback_t onReceive = NULL;
static lora_callback_t onSend = NULL;
static Task* check_rcv_task;
static Task* check_send_task;

static RH_SX126x driver(LORA_SS, LORA_DIO1, LORA_BUSY, LORA_NRESET); 

bool LoRa_init() {
    /*  1. Inicialitza radiohead. 
        2. Configura LoRa a paràmetres configurats.
        3. Inicialitza scheduler per comprovar recepció de missatges de forma periòdica */
    _PL("[LORA] Init");

    if(!driver.init())
        return false;

    if(!driver.setFrequency(LORA_FREQ))
        return false;
    
    if(!_setModemConfig(LORA_DATARATE))
        return false;

    if(!driver.setTxPower(LORA_TX_POW))
        return false;

    // Radio Head implementa 4 headers (TO, FROM, ID, FLAGS)
    // no els utilitzem, ho implementa capa superior.
    driver.enableRawMode(true);

    // Crea tasca després d'haver iniciat tot, per si alguna cosa falla abans
    check_rcv_task = scheduler_infinite(LORA_RCV_INTERVAL, &_check_received);
    return true;
}

void LoRa_deinit() {
    _PL("[LORA] Deinit");
    scheduler_stop(check_rcv_task);
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
    if(driver.isChannelActive()) { 
        _PL("[LORA] TX CHAN BUSY");
        return LORA_ERROR_TX_BUSY; 
    }  
    // `send` ja s'assegura que no hi hagi una transmissió en curs
    if(!driver.send(data->data, data->length)) {
        _PL("[LORA] TX ERR");
        return LORA_ERROR; 
    }

    // Inici tasca per comprovar si finalitza transmissió.
    // Interval a 1ms (mínim) per assegurar retard mínim
    check_send_task = scheduler_infinite(1, &_check_sent);
    
    _PL("[LORA] TX QUEUE");
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
    data->length = LORA_MAX_SIZE;
    return driver.recv(data->data, &(data->length));
}

bool LoRa_isAvailable() {
    if(driver.isChannelActive()) { 
        _PL("[LORA] TX CHAN BUSY");
        return false; 
    }  
    return true;
}

bool LoRa_isBusy() {
    return !LoRa_isAvailable();
}

int16_t LoRa_getLastRSSI() {
    /*
    Retorna l'últim RSSI (Receiver Signal Strength Indicator)
    */
    return driver.lastRssi();
}

int16_t LoRa_getLastSNR() {
    /*
    Retorna l'últim SNR mesurat (pel receptor) de l'últim missatge
    */
    return driver.lastSNR();
}

// bool LoRa_sleep() {
//     /*
//     Posa la radio en mode de baix consum.
//     Es desperta automàticament en posar-la en un altre mode (RX, TX, IDLE),
//     a través de `LoRa_send()`, `LoRa_isAvailable()`
//     */
//     return driver.sleep();
// }

bool LoRa_setFrequency(float frequency) {
    /*
    Canvia la freq. de la radio. En decimal i MHz.
    Retorna true si s'ha pogut fer el canvi, false si no.
    */
    return driver.setFrequency(frequency);
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
    _PL("===============");
    _PL("[LORA] DEBUG");
    _PP("\tMODE: "); _PL(driver.mode());
    _PP("\tTX OK: "); _PL(driver.txGood());
    _PP("\tRX OK: "); _PL(driver.rxGood());
    _PP("\tRX BAD: "); _PL(driver.rxBad());
    _PP("\tFreq Err: "); _PL((int) (driver.getFrequencyError()*1000));
    _PL("===============");
}

void _check_received() {
    /*
    Comprva si hi ha dades rebudes pendents.
    Cal executar-ho sovintment per evitar perdre 
    dades.
    Internament s'executa a través del scheduler, 
    després d'haver inicialitzat.
    */
    _PM("[LORA] check rcv");
    if(driver.available()) {
        if (onReceive != NULL) {
            _PL("[LORA] onReceive");
            onReceive();
        }
    }
}

void _check_sent() {
    /*
    Comprova si s'ha acabat TX.
    Segueix la mateixa implementació que `waitPacketSent()` de 
    RadioHead, però sense bloquejar. 
    
    Només s'hauria d'executar després de posar un paquet en TX  amb `LoRa_send()`. 
    En acabar enviament, s'ha de cancel·lar timeout per evitar falsos positius:
        Si continués comprovant, estaria en mode idle i executaria callback (sense ser correcte)

    Podria generar problemes la simultaneïtat de les tasques de recepció i fi de TX?
        - En tx el mode de driver és TX.
        - En cridar `available()` el mode passa a ser RX
        - Potser en fer canvi de mode no es gestionarà bé la interrupció?
    No, en veure implementació `available()` es veu com abans fa la comprovació
    de si el mode és TX; si ho és, sempre retornarà `False`
    */
    _PM("[LORA] check send");
    if(driver.mode() != RH_SX126x::RHModeTx) {
        scheduler_stop(check_send_task);
        if(onSend != NULL) {
            _PL("[LORA] onSend");
            onSend();
        }
    }
}

bool _setModemConfig(uint8_t datarate, uint8_t code_rate_denom) {
    /* Configura el modem de RadioHead a partir d'un datarate entre 0 i 6. 
    Permet especificar també el code rate (redundància) de la capa física, a
    través del denominador. Un valor entre 5 i 8, resultant amb code
    rates entre 4/5 i 4/8.
    Datarates majors es limiten a 6.
    Denominadors de code rate es limiten entre 5 i 8.
    
    DR         SF / BW          bps
    0 	LoRa: SF12 / 125 kHz 	250
    1 	LoRa: SF11 / 125 kHz 	440
    2 	LoRa: SF10 / 125 kHz 	980
    3 	LoRa: SF9 / 125 kHz 	1760
    4 	LoRa: SF8 / 125 kHz 	3125
    5 	LoRa: SF7 / 125 kHz 	5470
    6 	LoRa: SF7 / 250 kHz 	11000
    */
    
    // Limitem datarate i denominador de code rate
    datarate = datarate > 6 ? 6 : datarate;
    code_rate_denom = code_rate_denom > 8 ? 8 : (code_rate_denom < 5 ? 5 : code_rate_denom); 

    // Paràmetres: p1=SF, p2=BW, p3=Code Rate, p4=low DR optimize, p5..p8 = 0
    RH_SX126x::ModemConfig config; 
    // Per tots és paquet de tipus LoRa
    config.packetType = RH_SX126x::PacketTypeLoRa;
    // BW és 125kHz en tots, excepte DR6 que és 250kHz
    config.p2 = datarate == 6 ? RH_SX126x_LORA_BW_250_0 : RH_SX126x_LORA_BW_125_0;
    // Code rate es configura com paràmetre entre 1 i 4, sent 1 CR=4/5 i 4 CR=4/8
    // Hi ha relació entre denominador i valor paràmetre: VAL = DEN - 4
    config.p3 = code_rate_denom - 4;
    // Radiohead ho defineix a 1 per SF > 7 a les config per defecte
    config.p4 = datarate < 5 ? 0x01 : 0x00; 
    // La resta són 0
    config.p5 = config.p6 = config.p7 = config.p8 = 0;
    
    // Hi ha relació entre datarate i SF (com es veu a la taula)
    // excepte per DR6, on SF no pot ser 6 (mínim és 7).
    config.p1 = 12 - datarate;
    if (config.p1 < 7)
        config.p1 = 7;

    return driver.setModemRegisters(&config);
}
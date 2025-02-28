#include <Arduino.h>

#include "mac.h"
#include "lora.h"
#include "scheduler.h"
#include "utils.h"

enum mac_event_t {
    TX,             // Iniciar TX
    TOUT_BUSY,      // Fi tout de canal ocupat 
    TOUT_ACK,       // Timeout recepció ACK
    RX,             // Nova recepció
};

enum lora_event_t {
    BUSY,           // Canal LORA ocupat
    IDLE            // Canal LORA lliure
};

enum mac_state_t {
    IDLE,           // Esperant
    TRANSMITTING    // TX propia en curs
};

volatile static mac_state_t fsmState = mac_state_t::IDLE;

static mac_callback_t onSend = NULL;
static mac_callback_t onReceive = NULL;
static mac_callback_t onTxFailed = NULL;

static mac_addr_t self;

static mac_pdu_t txPDU, rxPDU;

volatile static uint8_t currentTxRetry = 0;
volatile static uint8_t currentBEBRetry = 0;
static bool isGateway = false;

static Task* txTimeoutTask;

mac_id_t _getRandomID();
mac_crc_t _computeCRC(const mac_pdu_t* const pdu);
void _getPDU(const mac_data_t * const data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx);
void _printPDU(const mac_pdu_t* const pdu);
bool _verifyCRC(const mac_pdu_t* const pdu);
void _mac_fsm(mac_event_t e);
void _mac_fsm_event_tout_ack(void);
void _mac_fsm_event_tout_busy(void);

void _onLoraSent(void);
void _onLoraReceived(void);
void _received(void);
void _sent(void);
void _txError(void);

bool MAC_init(mac_addr_t selfAddr, bool is_gateway) {
    // Filtrar que selfAddr no sigui adreça restringida (0x00 i 0xFF)
    self = selfAddr;
    LoRa_onSend(_onLoraSent);
    LoRa_onReceive(_onLoraReceived);
}

bool MAC_deinit();

mac_err_t MAC_send(mac_addr_t rx, const mac_data_t data, uint8_t length) {
    // 1. Verificar paràmetres (mida data, rx)
    // 2. Generar PDU (seran dades de LoRa)
    // 3. Iniciar tasca amb tout
    // 4. Enviar LoRa_send()
    if(length > MAC_MAX_DATA_SIZE)
        return MAC_ERR_MAX_LENGTH;
    if(rx == self)
        return MAC_ERR_INVALID_ADDR;

    _getPDU((const mac_data_t*)data, length, &txPDU, rx);
    _mac_fsm(mac_event_t::TX);

    return MAC_ERR_SUCCESS;
}

bool MAC_receive(mac_data_t const * data, uint8_t* length) {

}

bool MAC_isAvailable() {
    // Només interessa que nosaltres estiguem IDLE.
    // Si LoRa ocupat ja ho gestionarem amb backoff a FSM.
    return fsmState == mac_state_t::IDLE;
}

void MAC_onReceive(mac_callback_t cb) {
    onReceive = cb;
}

void MAC_onSend(mac_callback_t cb) {
    onSend = cb;
}

void MAC_onTxFailed(mac_callback_t cb) {
    onTxFailed = cb;
}

// ============== MÈTODES PRIVATS ==============

// --- GENERACIÓ PDU ---

void _getPDU(const mac_data_t * const data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx) {
    pdu->tx = self;
    pdu->rx = rx;
    memcpy(pdu->data, data, length);
    pdu->id = _getRandomID();
    pdu->length = length;
    pdu->crc = _computeCRC(pdu);
    _PL("[MAC] GET PDU");
    _printPDU(pdu);
    _PF("\tLen: %d", pdu->length); _PL();
    _PF("\tTX: %d", pdu->tx); _PL();
    _PF("\tRX: %d", pdu->rx); _PL();
    _PF("\tData: %s", pdu->data); _PL();
    _PF("\tID: %d", pdu->id); _PL();
    _PF("[MAC] CRC: %d", pdu->crc); _PL();
}

// https://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
// CRC-8/SMBUS
mac_crc_t _computeCRC(const mac_pdu_t* const pdu) {
    // Valor inicial crc
    mac_crc_t crc = 0x00; 
    
    // Processar dades de PDU
    for (uint8_t i = 0; i < (pdu->length+2*MAC_ADDRESS_SIZE+MAC_ID_SIZE); ++i) {
        crc = crc ^ ((uint8_t*)pdu)[i];  
        // Processar bits del byte de dades actual
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x80) {  
                crc = (crc << 1) ^ MAC_CRC8_POLY;  
            } else {
                crc <<= 1; 
            }
        }
    }
    return crc; 
}

bool _verifyCRC(const mac_pdu_t* const pdu) {
    mac_crc_t expected = _computeCRC(pdu);
    mac_crc_t obtained = pdu->crc;
    return expected == obtained;
}

mac_id_t _getRandomID() {
    return (mac_id_t)random(0, MAC_MAX_ID);
}

// ------------------------------

void _onLoraSent(void) {
   _PL("[MAC] Frame rcv");
}

void _onLoraReceived(void) {
   _PL("[MAC] Frame sent");
   // Generar esdeveniment a FSM
   _mac_fsm(mac_event_t::RX);
}

// --- FSM ---

void _mac_fsm(mac_event_t e) {
    // Retorna bool (0, 1): 0 si ocupat (not available) -> lora_event_t::busy
    lora_event_t lora_e = (lora_event_t)LoRa_isAvailable();
    if(fsmState == mac_state_t::IDLE) {
        if(e == mac_event_t::TX) {
            if(lora_e == lora_event_t::IDLE) { 
                /*
                    MAC i LORA lliures. Volem enviar. 
                    1. Obtenim temps de timeout segons mida de dades.
                    2. Programem tout amb scheduler i enviem amb LoRa
                    3. Si error en enviar, aturar tasca anterior
                */
                _PL("[MAC] FSM: MAC Idle + Lora IDLE + TX ");
                fsmState = mac_state_t::TRANSMITTING;
                long airtime_us = LoRa_getTimeOnAir(txPDU.length+MAC_PDU_HEADER_SIZE);
                // Deixem de marge de timeout per rebre "ACK" 3 vegades més de l'esperat 
                // (l'esperat és 2*timeOnAir, ha d'anar i tornar la resposta, de mateixa mida com a molt)
                txTimeoutTask = scheduler_once(_mac_fsm_event_tout_ack, 6*airtime_us/1000);
                lora_tx_error_t state = LoRa_send((const lora_data_t*)&txPDU);
                if(state != LORA_SUCCESS) {
                    scheduler_stop(txTimeoutTask); // cancel·lem abans que s'executi (per molt que passi el temps no s'haurà executat ja que necessita bucle de loop)
                    _txError();
                }
            }
            else if (lora_e == lora_event_t::BUSY) {
                /*
                    MAC lliure. LORA ocupat. Volem enviar. 
                    1. No és possible enviar si LoRa ocupat.
                    2. Nou estat és TX
                    3. Obtenir temps aleatori de backoff exponencial a partir de reintents de backoff
                    4. Generar un esdeveniment un cop ha passat el temps
                */
                _PL("[MAC] FSM: MAC Idle + Lora BUSY + TX");
                fsmState = mac_state_t::TRANSMITTING;
                // Incrementar i limitar intent actual de backoff
                currentBEBRetry++;
                currentBEBRetry = MIN(currentBEBRetry, MAC_MAX_BEB_RETRY);
                // Calcular timeout de backoff
                uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
                // Programar reintent
                txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
            }
        } 
        else if (e == mac_event_t::TOUT_ACK){
            /*
                MAC lliure i es genera timeout de recepció d'ACK. No hauria de ser possible. 
            */
           _PL("[MAC] FSM: MAC Idle + TOUT_ACK impossible");
        }
        else if (e == mac_event_t::TOUT_BUSY){
            /*
                MAC lliure i es genera timeout de canal ocupat generat per BEB. No hauria de ser possible
            */
           _PL("[MAC] FSM: MAC Idle + TOUT_BUSY impossible");
        }
        else if (e == mac_event_t::RX) {
            /*
                MAC lliure i es produeix recepció. És frame a retransmetre o per nosaltres. No pot ser d'ack.
            */
           _PL("[MAC] FSM: MAC Idle + RX");
           _received();
        }
    }
    else if (fsmState == mac_state_t::TRANSMITTING){
        if(e == mac_event_t::TX) {
            /*
                MAC ocupada i es volen enviar noves dades. No s'ha de permetre.
            */
        } 
        else if (e == mac_event_t::RX) {
            /*
                MAC ocupada i es produeix recepció. És frame a retransmetre o per nosaltres o ACK.
            */
           _PL("[MAC] FSM: MAC TX + RX");
           _received();
        }
        else if (e == mac_event_t::TOUT_ACK) {
            /*
                MAC ocupada i es genera timeout de recepció d'ACK.
                1. Incrementar comptador reintents
                2. Generar intent enviament de nou
            */
        }
        else if (e == mac_event_t::TOUT_BUSY){}
    }
}

void _mac_fsm_event_tout_ack(void) {
    _mac_fsm(mac_event_t::TOUT_ACK);
}

void _mac_fsm_event_tout_busy(void) {
    _mac_fsm(mac_event_t::TOUT_BUSY);
}

void _received(void) {
    /*
    Executat per FSM quan es rep alguna cosa.
    1. Filtrar si és per nosaltres o no
    2. Filtrar CRC
    3. Mirar si ID ja rebut anteriorment
        3.1. Si rebut, enviar un "ACK" (frame amb RX = 0x00, ID = ID esperat)
        3.2. Si no rebut, passar a capa superior
    */
}

void _sent(void) {
}

void _txError(void) {
    if(onTxFailed != NULL)
        onTxFailed();
}

#ifdef DEBUG
void _printPDU(const mac_pdu_t* const pdu) {
    // En escriure, tipus com uint16_t (ID) es guarden en little endian
    // Així, si ID és 60505 = 0xEC59 es mostraria com 59EC
    for (uint8_t i = 0; i < (pdu->length+2*MAC_ADDRESS_SIZE+MAC_ID_SIZE); ++i) {
        _PX(((char*)pdu)[i]);
    }
    // CRC
    _PX(pdu->crc); _PL();
}
#endif

/* === Proves ===
mac_pdu_t pdu;
mac_data_t data = "PROVA";

_getPDU((mac_data_t*)data, strlen((char*)data), &pdu, 0xff);

mac_crc_t crc = pdu.crc;
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // vàlid
pdu.tx ^= 0x01; // canviar un bit
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // no vàlid
pdu.tx ^= 0x01; // canviar bit de nou
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // vàlid

*/
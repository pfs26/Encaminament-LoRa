#include <Arduino.h>

#include "mac.h"
#include "lora.h"
#include "scheduler.h"
#include "utils.h"
#include "RingBuffer.h"

enum mac_event_t {
    TX_E,             // Iniciar TX
    TX_DONE_E,         // Fi TX
    TOUT_BUSY_E,      // Fi tout de canal ocupat 
    TOUT_ACK_E,       // Timeout recepció ACK
    RX_E,             // Nova recepció
    RX_ACK_E,         // Recepció ACK
};

enum lora_event_t {
    BUSY_E,           // Canal LORA ocupat
    IDLE_E            // Canal LORA lliure
};

enum mac_state_t {
    IDLE_S,           // Esperant
    TX_PENDING_S,     // Esperant TX
    WAIT_ACK_S,       // Esperant recepció ACK
    WAIT_CHAN_FREE_S  // Esperant canal LoRa lliure  
};

volatile static mac_state_t fsmState = mac_state_t::IDLE_S;
static mac_callback_t onSend = NULL;
static mac_callback_t onReceive = NULL;
static mac_callback_t onTxFailed = NULL;

static mac_addr_t self;

static mac_pdu_t txPDU, rxPDU;

volatile static uint8_t currentTxRetry = 0;
volatile static uint8_t currentBEBRetry = 0;
static bool isGateway = false;

// static mac_id_t lastFramesIDs[MAC_QUEUE_SIZE];
static RingBuffer lastFramesIDs(MAC_QUEUE_SIZE);

static Task* txTimeoutTask;

mac_id_t _getRandomID();
mac_crc_t _computeCRC(const mac_pdu_t* const pdu);
void _getPDU(const mac_data_t data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx, mac_id_t id = 0);
void _printPDU(const mac_pdu_t* const pdu);
bool _verifyCRC(const mac_pdu_t* const pdu);
bool _is_ack_pdu_valid(const mac_pdu_t * const pdu);
void _mac_fsm(mac_event_t e);
void _mac_fsm_event_tout_ack(void);
void _mac_fsm_event_tout_busy(void);
bool _lora_data_to_mac_pdu(lora_data_t * const lora, mac_pdu_t * const pdu);
bool _mac_pdu_to_lora_data(lora_data_t * const lora, mac_pdu_t * const pdu);

void _onLoraSent(void);
void _onLoraReceived(void);
void _received_mac(void);
void _sent_mac(void);
void _txError_mac(void);

bool MAC_init(mac_addr_t selfAddr, bool is_gateway) {
    // Filtrar que selfAddr no sigui adreça restringida (0x00 i 0xFF)
    self = selfAddr;
    LoRa_onSend(_onLoraSent);
    LoRa_onReceive(_onLoraReceived);
    _PL("[MAC] Init");
    return LoRa_init();
}

void MAC_deinit() {
    _PL("[MAC] Deinit");
    LoRa_deinit();
    onReceive = onSend = onTxFailed = NULL;
}

mac_err_t MAC_send(mac_addr_t rx, const mac_data_t data, uint8_t length) {
    // 1. Verificar paràmetres (mida data, rx)
    // 2. Generar PDU (seran dades de LoRa)
    // 3. Iniciar tasca amb tout
    // 4. Enviar LoRa_send()
    if(length > MAC_MAX_DATA_SIZE)
        return MAC_ERR_MAX_LENGTH;
    if(rx == self)
        return MAC_ERR_INVALID_ADDR;
    if(!MAC_isAvailable()) // No permetre segon enviament si un està pendent. Sobreescriuria PDU TX actual
        return MAC_ERR_TX_PENDING;

    _getPDU(data, length, &txPDU, rx);
    _mac_fsm(mac_event_t::TX_E);

    return MAC_ERR_SUCCESS;
}

mac_addr_t MAC_receive(mac_data_t* data, uint8_t* length) {
    *length = rxPDU.length;
    memcpy(data, rxPDU.data, rxPDU.length);
    return rxPDU.tx;
}

bool MAC_isAvailable() {
    // Només interessa que nosaltres estiguem IDLE.
    // Si LoRa ocupat ja ho gestionarem amb backoff a FSM.
    return fsmState == mac_state_t::IDLE_S;
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

mac_err_t _send_ack(const mac_pdu_t * const refPdu) {
    if (!MAC_isAvailable()) {
        _PF("\tMAC not available to send ACK\n");
        return MAC_ERR_TX_PENDING;
    }
    _PF("\tMAC available to send ACK. Trying to send\n");

    // Podem sobreescriure PDU de transmissió; estat actual és IDLE sí o sí, sinó MAC_isAvailable retorna False i no podem enviar ACK
    _getPDU((const uint8_t*)"", 0, &txPDU, (mac_addr_t) 0x00, refPdu->id + self);
    // Enviem dades SENSE PASSAR PER FSM, implicaria esperar un ACK del mateix ACK, bucle.
    lora_data_t data;
    _mac_pdu_to_lora_data(&data, &txPDU);
    lora_tx_error_t state = LoRa_send(&data);
    
    if(state != LORA_SUCCESS)
        return MAC_ERR;
    return MAC_ERR_SUCCESS;
}

// --- GENERACIÓ PDU ---

void _getPDU(const mac_data_t data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx, mac_id_t id) {
    pdu->tx = self;
    pdu->rx = rx;
    memcpy(pdu->data, data, length);
    pdu->id = id ? id : _getRandomID(); // si es proporciona ID s'utilitza aquell; sino, es genera aleatori.
    pdu->length = length;
    pdu->crc = _computeCRC(pdu);
    _PL("[MAC] GET PDU");
    _printPDU(pdu);
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
    return (mac_id_t)random(1, MAC_MAX_ID);
}

// ------------------------------

void _onLoraSent(void) {
    /*
        Callback executat per capa inferior LoRa quan es produeix una transmissió.
    */
   _PL("[MAC] Frame sent");
   // Generem esdeveniment de TX done
   _mac_fsm(mac_event_t::TX_DONE_E);
}

void _onLoraReceived(void) {
    /*
        Callback executat quan es produeix una recepció a capa inferior LoRa.
        Filtra que les dades siguin vàlides i per nosaltres.
        Si és així, converteix dades rebudes en PDU de MAC, 
        i genera esdeveniment de recepció; si no, ignora recepció.
    */
    _PL("[MAC] Frame rcv");

    lora_data_t data;
    if(!LoRa_receive(&data)) {
        _PL("\tRecieve ERR");
        return;
    }

    _lora_data_to_mac_pdu(&data, &rxPDU);

    _PL("Received PDU from LORA");
    _printPDU(&rxPDU);
    if(!_verifyCRC(&rxPDU)) {
        _PL("\tCRC ERR");
        return;
    }

    mac_id_t rcvID = rxPDU.id;
    bool seen = lastFramesIDs.contains(rcvID);

    if (seen) {
        // Si ja l'hem vist abans és perquè era un frame per nosaltres;
        _PF("\tAlready seen id received: %d\n", rcvID);
        _send_ack(&rxPDU);
    }
    else {
        _PF("\tNew id received: %d\n", rcvID);
        // Mirem si el rebut és ACK
        if (_is_ack_pdu_valid(&rxPDU)) {
            _PF("\tACK Received from %d\n", rxPDU.tx);
            lastFramesIDs.enqueue(rcvID);
            _mac_fsm(mac_event_t::RX_ACK_E);
        }
        // Si no és ACK, mirem si som el receptor
        else if (rxPDU.rx == self) {
            lastFramesIDs.enqueue(rcvID);
            _PF("\tFrame for higher layer");

            // FICTICI, ELIMINAR! HO GESTIONARIA CAPA ROUTING
            _send_ack(&rxPDU);
        }
    }
}

// --- FSM ---
void _mac_fsm(mac_event_t e) {
    /*
        FSM principal capa MAC. Gestiona enviaments, aplicant BEB,
        recepció d'"ACK" i recepció d'altres dades.
    */

    // Retorna bool (0, 1): 0 si ocupat (not available) -> lora_event_t::busy
    lora_event_t lora_e = (lora_event_t)LoRa_isAvailable();
    _PF("[MAC] FSM:\tSTATE %d\tMAC %d\tLORA %d\n", fsmState, e, lora_e);
    if (fsmState == mac_state_t::IDLE_S) {
        if (e == mac_event_t::TX_E && lora_e == lora_event_t::IDLE_E) {
            _PL("\tSENDING");
            fsmState = mac_state_t::TX_PENDING_S;
            lora_data_t data;
            _mac_pdu_to_lora_data(&data, &txPDU);
            currentTxRetry = 0;
            lora_tx_error_t state = LoRa_send(&data);
            if(state != LORA_SUCCESS) {
                fsmState == mac_state_t::IDLE_S;
                _txError_mac();
            }
        }
        else if (e == mac_event_t::TX_E && lora_e == lora_event_t::BUSY_E) {
            fsmState = mac_state_t::WAIT_CHAN_FREE_S;
            currentBEBRetry = 0;
            currentTxRetry = 0;
            _PF("\tWAITING CHANN FREE (%d)\n", currentBEBRetry);
            // Calcular timeout de backoff
            uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
            // Programar reintent
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
            _PF("\tTimeout BEB: %d\n", bebTimeout);
        }
    }
    else if (fsmState == mac_state_t::WAIT_CHAN_FREE_S) {
        if (e == mac_event_t::TOUT_BUSY_E && lora_e == lora_event_t::BUSY_E) {
            fsmState = mac_state_t::WAIT_CHAN_FREE_S;
            currentBEBRetry++;
            _PF("\tWAITING CHANN FREE (%d)\n", currentBEBRetry);
            // Limitar BEB màxim per no tenir un temps de retard molt elevat
            currentBEBRetry = MIN(currentBEBRetry, MAC_MAX_BEB_RETRY);
            // Calcular timeout de backoff
            uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
            // Programar reintent
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
            _PF("\tTimeout BEB: %d\n", bebTimeout);
        }
        else if (e == mac_event_t::TOUT_BUSY_E && lora_e == lora_event_t::IDLE_E) { 
            _PL("\tCHANN FREE; SENDING");
            fsmState = mac_state_t::TX_PENDING_S;
            lora_data_t data;
            _mac_pdu_to_lora_data(&data, &txPDU);
            lora_tx_error_t state = LoRa_send(&data);
            if(state != LORA_SUCCESS) {
                fsmState == mac_state_t::IDLE_S;
                _txError_mac();
            }
        }
    }
    else if (fsmState == mac_state_t::TX_PENDING_S) {
        if(e == mac_event_t::TX_DONE_E) {
            fsmState = mac_state_t::WAIT_ACK_S;
            // Iniciem timeout de recepció d'ACK, estimant el time on air (depenent de SF, BW, etc.)
            // i deixant de marge 5 vegades més de l'esperat (l'esperat és 2*timeOnAir, ha d'anar i tornar la resposta, de mateixa mida com a molt)
            long airtime_us = LoRa_getTimeOnAir(txPDU.length+MAC_PDU_HEADER_SIZE);
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_ack, 2*MAC_ACK_TIMEOUT_FACTOR*airtime_us/1000);
            _PF("[MAC] Timeout d'ACK: %dms (2*%d*%d/1000)\n", 2*MAC_ACK_TIMEOUT_FACTOR*airtime_us/1000, MAC_ACK_TIMEOUT_FACTOR, airtime_us);
        }

    }
    else if (fsmState == mac_state_t::WAIT_ACK_S) {
        if(e == mac_event_t::RX_ACK_E) {
            // recepció ACK, transmissió DONE
            _PL("\tACK RECEIVED");
            fsmState = mac_state_t::IDLE_S;
            scheduler_stop(txTimeoutTask);
            _sent_mac();
        }
        else if (e == mac_event_t::TOUT_ACK_E && lora_e == lora_event_t::IDLE_E) {
            currentTxRetry++;
            if (currentTxRetry == MAC_MAX_RETRIES) {
                fsmState = mac_state_t::IDLE_S;
                _txError_mac();
                return;
            }
            fsmState = mac_state_t::TX_PENDING_S;
            lora_data_t data;
            _mac_pdu_to_lora_data(&data, &txPDU);
            lora_tx_error_t state = LoRa_send(&data);
            if(state != LORA_SUCCESS) {
                fsmState == mac_state_t::IDLE_S;
                _txError_mac();
            }
        }
        else if (e == mac_event_t::TOUT_ACK_E && lora_e == lora_event_t::BUSY_E) {
            fsmState = mac_state_t::WAIT_CHAN_FREE_S;
            currentBEBRetry = 0;
            _PF("\tWAITING CHANN FREE (%d)\n", currentBEBRetry);
            // Calcular timeout de backoff
            uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
            // Programar reintent
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
            _PF("\tTimeout BEB: %d\n", bebTimeout);
        }
    }
}

void _mac_fsm_event_tout_ack(void) { _mac_fsm(mac_event_t::TOUT_ACK_E); }
void _mac_fsm_event_tout_busy(void) { _mac_fsm(mac_event_t::TOUT_BUSY_E); }

bool _mac_pdu_to_lora_data(lora_data_t * const lora, mac_pdu_t * const pdu) {
    uint8_t* ptr = lora->data;
    memcpy(ptr, pdu, pdu->length+MAC_PDU_HEADER_SIZE-MAC_CRC_SIZE);
    ptr += pdu->length+MAC_PDU_HEADER_SIZE-MAC_CRC_SIZE;
    memcpy(ptr, &pdu->crc, sizeof(mac_crc_t));
    lora->length = txPDU.length+MAC_PDU_HEADER_SIZE;
    return true;
}

bool _lora_data_to_mac_pdu(lora_data_t * const lora, mac_pdu_t * const pdu) {
    if (lora->length < sizeof(mac_addr_t) * 2 + sizeof(mac_id_t) + sizeof(mac_crc_t)) {
        _PF("[MAC] lora_to_mac_pdu: Invalid data size: %dB\n", lora->length);
        return false;
    }

    uint8_t* ptr = lora->data; 
    memcpy(&pdu->tx, ptr, sizeof(mac_addr_t));
    ptr += sizeof(mac_addr_t);
    memcpy(&pdu->rx, ptr, sizeof(mac_addr_t));
    ptr += sizeof(mac_addr_t);
    memcpy(&pdu->id, ptr, sizeof(mac_id_t));
    ptr += sizeof(mac_id_t);
    pdu->length = lora->length - (sizeof(mac_addr_t) * 2 + sizeof(mac_id_t) + sizeof(mac_crc_t));

    memcpy(pdu->data, ptr, pdu->length);
    ptr += pdu->length;
    memcpy(&pdu->crc, ptr, sizeof(mac_crc_t));

    return true;
}

bool _is_ack_pdu_valid(const mac_pdu_t * const pdu) {
    // Pot ser ACK si @TX és la de l'anterior RX
    // i si ID rebut és ID anterio + ID TX
    return (pdu->tx == txPDU.rx && pdu->id == txPDU.id+txPDU.rx);
}

void _sent_mac(void) {
}

void _txError_mac(void) {
    _PL("[MAC] TX ERR");
    if(onTxFailed != NULL)
        onTxFailed();
}

#ifdef DEBUG
void _printPDU(const mac_pdu_t* const pdu) {
    // En escriure, tipus com uint16_t (ID) es guarden en little endian
    // Així, si ID és 60505 = 0xEC59 es mostraria com 59EC
    // for (uint8_t i = 0; i < (pdu->length+2*MAC_ADDRESS_SIZE+MAC_ID_SIZE); ++i) {
        // _PX(((char*)pdu)[i]);
    // }
    // _PX(pdu->crc); _PL();
    _PF("===== PDU =====\nTX\t%d\nRX\t%d\nID\t%d\nD\t%s\nCRC\t%d\n===============\n",
    pdu->tx, pdu->rx, pdu->id, pdu->data, pdu->crc);
}
#endif


void setup() {
    Serial.begin(115200);

    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println(esp_reset_reason());

    if(!MAC_init(0x03, false)) {
        _PL("ERR");
        while(1);
    }

    // mac_data_t data = "PROVAA";
    // MAC_send(0x03, data, strlen((char*)data));
}

void loop() {
    scheduler_run();
}



/* === Proves CRC ===
mac_pdu_t pdu;
mac_data_t data = "PROVA";

_getPDU((mac_data_t*)data, strlen((char*)data), &pdu, 0xff);

mac_crc_t crc = pdu.crc;
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // vàlid
pdu.tx ^= 0x01; // canviar un bit
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // no vàlid
pdu.tx ^= 0x01; // canviar bit de nou
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // vàlid


=== PROVES LORA_TO_PDU ===
lora_data_t data = {0x01,0x02,0x2C,0x82,0x50,0x52,0x4F,0x56,0x41,0x86};
data.length = 10;
_printLora(&data);

mac_pdu_t pdu;
_lora_data_to_mac_pdu(&data, &pdu);
_printPDU(&pdu);
_PF("CRC: %d\n", _verifyCRC(&pdu));
*/
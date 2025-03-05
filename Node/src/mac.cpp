#include <Arduino.h>

#include "mac.h"
#include "lora.h"
#include "scheduler.h"
#include "utils.h"
#include "RingBuffer.h"

enum mac_event_t {
    TX_E,             // Iniciar TX
    TX_ERROR_E,
    TX_SUCCESS_E,
    TOUT_BUSY_E,      // Fi tout de canal ocupat
    TOUT_ACK_E,       // Timeout recepció ACK
    RX_ACK_E,         // Recepció ACK
};

enum lora_event_t {
    BUSY_E,           // Canal LORA ocupat
    IDLE_E            // Canal LORA lliure
};

enum mac_state_t {
    IDLE_S,           // Esperant
    TX_S,               // Esperant TX
    WAIT_ACK_S,       // Esperant recepció ACK
    WAIT_CHAN_FREE_S  // Esperant canal LoRa lliure
};

volatile static mac_state_t fsmState = mac_state_t::IDLE_S;
static mac_callback_t onSend = NULL;
static mac_callback_t onReceive = NULL;
static mac_callback_t onTxFailed = NULL;

static mac_addr_t self;

static mac_pdu_t txPDU, rxPDU;
static size_t txLength = 0, rxLength = 0;

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
bool _is_ack_valid(const mac_pdu_t * const pdu);
void _mac_fsm(mac_event_t e);
void _mac_fsm_event_tout_ack(void);
void _mac_fsm_event_tout_busy(void);

size_t _computePDULength(const mac_pdu_t * const pdu);
size_t _computeDataLength(const mac_pdu_t * const pdu);

mac_err_t _inner_send(mac_addr_t rx, const mac_data_t data, size_t length, bool isAck = false, const mac_pdu_t * const referedPDU = NULL);
void _prepareTxPDU(mac_addr_t rx, const mac_data_t data, size_t length, uint8_t retry = 0, bool isAck = false, const mac_pdu_t * const PDUtoACK = NULL);
size_t _PDUtoLora(const mac_pdu_t * const pdu, lora_data_t lora);
void _LoraToPDU(const lora_data_t lora, size_t length, mac_pdu_t * pdu);

void _onLoraSent(void);
void _onLoraReceived(void);
void _received_mac(void);
void _sent_mac(void);
void _txError_mac(void);

bool MAC_init(mac_addr_t selfAddr, bool is_gateway) {
    // Filtrar que selfAddr no sigui adreça restringida (0x00 i 0xFF)
    self = selfAddr;
    LoRa_onReceive(_onLoraReceived);
    _PI("[MAC] Init");
    return LoRa_init();
}

void MAC_deinit() {
    _PI("[MAC] Deinit");
    LoRa_deinit();
    onReceive = onSend = onTxFailed = NULL;
}

mac_err_t MAC_send(mac_addr_t rx, const mac_data_t data, size_t length) {
    return _inner_send(rx, data, length);
}

mac_addr_t MAC_receive(mac_data_t* data, size_t* length) {
    *length = rxLength;
    memcpy(data, rxPDU.data, rxLength);
    // (*data)[*length] = '\0';
    return rxPDU.tx;
}

// Només interessa que nosaltres estiguem IDLE.
// Si LoRa ocupat ja ho gestionarem amb backoff a FSM.
bool MAC_isAvailable() { return fsmState == mac_state_t::IDLE_S; }

void MAC_onReceive(mac_callback_t cb) { onReceive = cb; }

void MAC_onSend(mac_callback_t cb) { onSend = cb; }

void MAC_onTxFailed(mac_callback_t cb) { onTxFailed = cb; }

// ============== MÈTODES PRIVATS ==============

mac_err_t _send_ack(const mac_pdu_t * const refPdu) {
    return _inner_send(0x00, (uint8_t*)"", 0, true, refPdu);
}

mac_err_t _inner_send(mac_addr_t rx, const mac_data_t data, size_t length, bool isAck, const mac_pdu_t * const PDUtoACK) {
    // 1. Verificar paràmetres (mida data, rx).
    // 2. Generar PDU (seran dades de LoRa)
    // 3. Iniciar tasca amb tout
    // 4. Enviar LoRa_send()
    _PW("[MAC] Preparing to send (ACK = %d)", isAck);

    if(length > MAC_MAX_DATA_SIZE) {
        _PW("[MAC] Max length exceeded (%d)", length);
        return mac_err_t::MAC_ERR_MAX_LENGTH;
    }
    if(rx == self) {
        _PW("[MAC] RX address (%d) cannot be self (%d)", rx, self);
        return mac_err_t::MAC_ERR_INVALID_ADDR;
    }
    
    // No permetre segon enviament si un està pendent. Sobreescriuria PDU TX actual
    if(!MAC_isAvailable()) { 
        _PW("[MAC] Not available for another transmission");
        return mac_err_t::MAC_ERR_TX_PENDING;
    }

    if(isAck && PDUtoACK == NULL) {
        _PW("[MAC] Cannot send ACK to null PDU");
        return mac_err_t::MAC_ERR;
    }

    _prepareTxPDU(rx, data, length, isAck, PDUtoACK);
    _PI("[MAC] Tx PDU ready");
    _printPDU(&txPDU);

    // Si és ACK NO es pot passar per FSM -> implicaria que esperaria ACK del mateix ACK, resultant en bucle infinit -> Enviar directament
    // No interfereix en possibles transmissions en curs, ja que abans es verifica estat MAC amb `MAC_isAvailable()`
    // Només enviarà ACK explícit si no hi ha transmissió en curs.
    if(isAck) {
        _PI("[MAC] Sending ACK. Ignoring FSM");
        lora_data_t data;
        size_t size = _PDUtoLora(&txPDU, data);
        lora_tx_error_t state = LoRa_send(data, size);
    }
    else {
        _mac_fsm(mac_event_t::TX_E);
    }
    return mac_err_t::MAC_SUCCESS;
}

// --- GENERACIÓ PDU ---
void _prepareTxPDU(mac_addr_t rx, const mac_data_t data, size_t length, uint8_t retry, bool isAck, const mac_pdu_t * const PDUtoACK) {
    txPDU.tx = self;
    txPDU.rx = rx;
    txPDU.id = isAck ? PDUtoACK->id+self : _getRandomID();
    txPDU.flags.isACK = isAck;
    txPDU.flags.retry = 0x00;
    txPDU.flags.reserved = 0b111;
    txPDU.dataLength = length;
    // REQUEREIX QUE DATA SIGUI NULL-TERMINATED (ACABAT AMB \0)
    // AMB STRCPY COPIARÀ \0 FINAL, NECESSARI PER CALCULAR CRC CORRECTAMENT!
    strcpy((char*)txPDU.data, (char*)data);
    txPDU.crc = _computeCRC(&txPDU);
}

size_t _PDUtoLora(const mac_pdu_t * const pdu, lora_data_t lora) {
    // PDU té el format [TX|RX|ID_H|ID_L|FLAGS|LEN|DATA|...|DATA|RUBISH|...|CRC]
    // Podem copiar tot el bloc fins a final de data, i manualment copiar CRC
    size_t index = 0;

    // Copiar tota la memòria fins a data
    size_t size_til_data_end = offsetof(mac_pdu_t, data) + pdu->dataLength;
    memcpy(&lora[index], pdu, size_til_data_end);
    index += size_til_data_end;

    // Copiar CRC
    memcpy(&lora[index], &pdu->crc, sizeof(mac_crc_t));

    // La mida final de LORA serà la mida de les dades més la mida dels headers/trailers PDU 
    return pdu->dataLength + MAC_PDU_HEADER_SIZE;
}

void _LoraToPDU(const lora_data_t lora, size_t length, mac_pdu_t * pdu) {
    // LORA té el format [TX|RX|ID_H|ID_L|FLAGS|LEN|DATA|...|DATA|CRC]
    size_t index = 0;

    // Copiar header + data sense CRC directament a pdu (segueix mateixa estructura)
    size_t data_length = length - sizeof(mac_crc_t); 
    memcpy(pdu, &lora[index], data_length);
    index += data_length;

    // Acabar dades PDU amb '\0'
    // ((char*)pdu)[index] = '\0';

    // Copiar CRC manualment a pdu
    // pdu->crc = lora[index];
    memcpy(&pdu->crc, &lora[index], sizeof(mac_crc_t));
}
// https://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
// CRC-8/SMBUS
mac_crc_t _computeCRC(const mac_pdu_t* const pdu) {
    // Valor inicial crc
    mac_crc_t crc = 0x00;

    size_t data_end_pos = offsetof(mac_pdu_t, data) + pdu->dataLength;
    for (size_t i = 0; i < data_end_pos; i++)
    {
        Serial.printf("%02X", ((char*)pdu)[i]);
        crc = crc ^ ((char*)pdu)[i];
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

bool _is_ack_valid(const mac_pdu_t * const pdu) {
    return pdu->flags.isACK && pdu->tx == txPDU.rx && pdu->id == txPDU.id + txPDU.rx;
}

// ------------------------------

void _onLoraReceived(void) {
    /*
        Callback executat quan es produeix una recepció a capa inferior LoRa.
        Filtra que les dades siguin vàlides i per nosaltres.
        Si és així, converteix dades rebudes en PDU de MAC,
        i genera esdeveniment de recepció; si no, ignora recepció.
    */
    _PI("[MAC] Frame rcv");

    lora_data_t data;
    size_t len;
    if(!LoRa_receive(data, &len)) {
        _PW("\tRecieve ERR");
        return;
    }

    _LoraToPDU(data, len, &rxPDU);

    _PI("Received PDU from LORA");
    _printPDU(&rxPDU);
    if(!_verifyCRC(&rxPDU)) {
        _PW("\tCRC ERR");
        return;
    }

    mac_id_t rcvID = rxPDU.id;
    bool seen = lastFramesIDs.contains(rcvID);

    if (seen) {
        // Si ja l'hem vist abans és perquè era un frame per nosaltres;
        _PI("\tAlready seen id received: %d\n", rcvID);
        _send_ack(&rxPDU);
    }
    else {
        _PI("\tNew id received: %d\n", rcvID);
        // Mirem si el rebut és ACK
        if (_is_ack_valid(&rxPDU)) {
            _PI("\tACK Received from %d\n", rxPDU.tx);
            lastFramesIDs.enqueue(rcvID);
            _mac_fsm(mac_event_t::RX_ACK_E);
        }
        // Si no és ACK, mirem si som el receptor
        else if (rxPDU.rx == self) {
            lastFramesIDs.enqueue(rcvID);
            _PI("\tFrame for higher layer");

            // FICTICI, ELIMINAR! HO GESTIONARIA CAPA ROUTING
            _send_ack(&rxPDU);

            _received_mac();
        }
        else {
            _PI("\tFrame not for self: %d\n", rxPDU.rx);
        }
    }
}

// // --- FSM ---
void _mac_fsm(mac_event_t e) {
    /*
        FSM principal capa MAC. Gestiona enviaments, aplicant BEB,
        recepció d'"ACK".
    */

    // Retorna bool (0, 1): 0 si ocupat (not available) -> lora_event_t::busy
    lora_event_t lora_e = (lora_event_t)LoRa_isAvailable();
    _PI("[MAC] FSM:\tSTATE %d\tMAC %d\tLORA %d\n", fsmState, e, lora_e);
    if (fsmState == mac_state_t::IDLE_S) {
        if (e == mac_event_t::TX_E && lora_e == lora_event_t::IDLE_E) {
            fsmState = mac_state_t::TX_S;
            currentTxRetry = 0;
            // txPDU.flags.retry = currentTxRetry;
            _prepareTxPDU(txPDU.rx, txPDU.data, txPDU.dataLength, currentTxRetry);
            lora_data_t data;
            size_t lora_length = _PDUtoLora(&txPDU, data);
            lora_tx_error_t state = LoRa_send(data, lora_length);
            if(state != LORA_SUCCESS) {
                _mac_fsm(mac_event_t::TX_ERROR_E);
            }
            else {
                _mac_fsm(mac_event_t::TX_SUCCESS_E);
            }
        }
        else if (e == mac_event_t::TX_E && lora_e == lora_event_t::BUSY_E) {
            fsmState = mac_state_t::WAIT_CHAN_FREE_S;
            currentBEBRetry = 0;
            currentTxRetry = 0;
            txPDU.flags.retry = currentTxRetry;
            _PI("\tWAITING CHANN FREE (%d)\n", currentBEBRetry);
            // Calcular timeout de backoff
            uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
            // Programar reintent
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
            _PI("\tTimeout BEB: %d\n", bebTimeout);
        }
    }
    else if (fsmState == mac_state_t::WAIT_CHAN_FREE_S) {
        if (e == mac_event_t::TOUT_BUSY_E && lora_e == lora_event_t::BUSY_E) {
            fsmState = mac_state_t::WAIT_CHAN_FREE_S;
            currentBEBRetry++;
            _PI("\tWAITING CHANN FREE (%d)\n", currentBEBRetry);
            // Limitar BEB màxim per no tenir un temps de retard molt elevat
            currentBEBRetry = MIN(currentBEBRetry, MAC_MAX_BEB_RETRY);
            // Calcular timeout de backoff
            uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
            // Programar reintent
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
            _PI("\tTimeout BEB: %d\n", bebTimeout);
        }
        else if (e == mac_event_t::TOUT_BUSY_E && lora_e == lora_event_t::IDLE_E) {
            _PI("\tCHANN FREE; SENDING");
            fsmState = mac_state_t::TX_S;
            _prepareTxPDU(txPDU.rx, txPDU.data, txPDU.dataLength, currentTxRetry);
            // txPDU.flags.retry = currentTxRetry;
            lora_data_t data;
            size_t lora_length = _PDUtoLora(&txPDU, data);
            lora_tx_error_t state = LoRa_send(data, lora_length);
            if(state != LORA_SUCCESS) {
                _mac_fsm(mac_event_t::TX_ERROR_E);
            }
            else {
                _mac_fsm(mac_event_t::TX_SUCCESS_E);
            }
        }
    }
    else if (fsmState == mac_state_t::TX_S) {
        if(e == mac_event_t::TX_ERROR_E) {
            fsmState = mac_state_t::IDLE_S;
            _txError_mac();
        }
        else if(e == mac_event_t::TX_SUCCESS_E) {
            fsmState = mac_state_t::WAIT_ACK_S;
            LoRa_startReceiving();
            // Iniciem timeout de recepció d'ACK, estimant el time on air (depenent de SF, BW, etc.)
            // i deixant de marge 5 vegades més de l'esperat (l'esperat és 2*timeOnAir, ha d'anar i tornar la resposta, de mateixa mida com a molt)
            long airtime_us = LoRa_getTimeOnAir(txPDU.dataLength+MAC_PDU_HEADER_SIZE);
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_ack, 2*MAC_ACK_TIMEOUT_FACTOR*airtime_us/1000);
            _PI("[MAC] Timeout d'ACK: %dms (%dus airtime)\n", 2*MAC_ACK_TIMEOUT_FACTOR*airtime_us/1000, airtime_us);
        }

    }
    else if (fsmState == mac_state_t::WAIT_ACK_S) {
        if(e == mac_event_t::RX_ACK_E) {
            // recepció ACK, transmissió DONE
            _PI("\tACK RECEIVED");
            fsmState = mac_state_t::IDLE_S;
            scheduler_stop(txTimeoutTask);
            _sent_mac();
        }
        else if (e == mac_event_t::TOUT_ACK_E && lora_e == lora_event_t::IDLE_E) {
            currentTxRetry++;
            if (currentTxRetry >= MAC_MAX_RETRIES) {
                _PW("\tMAX RETRIES REACHED\n");
                fsmState = mac_state_t::IDLE_S;
                _txError_mac();
                return;
            }
            fsmState = mac_state_t::TX_S;
            _prepareTxPDU(txPDU.rx, txPDU.data, txPDU.dataLength, currentTxRetry);
            // txPDU.flags.retry = currentTxRetry;
            lora_data_t data;
            
            size_t lora_length = _PDUtoLora(&txPDU, data);
            lora_tx_error_t state = LoRa_send(data, lora_length);
            if(state != LORA_SUCCESS) {
                _mac_fsm(mac_event_t::TX_ERROR_E);
            }
            else {
                _mac_fsm(mac_event_t::TX_SUCCESS_E);
            }
        }
        else if (e == mac_event_t::TOUT_ACK_E && lora_e == lora_event_t::BUSY_E) {
            fsmState = mac_state_t::WAIT_CHAN_FREE_S;
            currentTxRetry++;
            currentBEBRetry = 0;
            _PI("\tWAITING CHANN FREE (%d)\n", currentBEBRetry);
            // Calcular timeout de backoff
            uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
            // Programar reintent
            txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
            _PI("\tTimeout BEB: %d\n", bebTimeout);
        }
    }
}

void _mac_fsm_event_tout_ack(void) { _mac_fsm(mac_event_t::TOUT_ACK_E); }
void _mac_fsm_event_tout_busy(void) { _mac_fsm(mac_event_t::TOUT_BUSY_E); }

void _sent_mac(void) {
    _PI("[MAC] SENT");
    LoRa_startReceiving();
    if(onSend != NULL) {
        onSend();
    }
}

void _received_mac(void) {
    _PI("[MAC] RCV");
    if(onReceive != NULL) {
        onReceive();
    }
}

void _txError_mac(void) {
    _PI("[MAC] TX ERR");
    LoRa_startReceiving();
    if(onTxFailed != NULL)
        onTxFailed();
}

#if LOG_LEVEL <= LOG_LEVEL_INFO
void _printPDU(const mac_pdu_t* const pdu) {
    // Mostra info de PDU en format maco.
    // Les dades les intenta mostrar en format ASCII, i només mostra tants chars com marca dataLength (o fins que hi ha '\0', culpa de mostrar-ho com string)
    // Mostra també amb hexadecimal per si hi ha 0x00 entre mig saber si realment és correcte.
    Serial.printf("===== PDU =====\nTX\t%d\nRX\t%d\nID\t%d\nD-LEN\t%d\nDATA\t%.*s\nD-HEX\t", 
    pdu->tx, pdu->rx, pdu->id, pdu->dataLength, pdu->dataLength, pdu->data);
    for (int i = 0; i < pdu->dataLength; i++) 
        Serial.printf("%02X ", pdu->data[i]); 
    Serial.printf("\nCRC\t%d\nACK\t%d\nRETRY\t%d\n===============\n", pdu->crc, pdu->flags.isACK, pdu->flags.retry);
}
#else
void _printPDU(const mac_pdu_t* const pdu) {}
#endif



// #include <Arduino.h>
// #include "lora.h"
// #include "scheduler.h"
// #include "utils.h"

// void setup() {
//     Serial.begin(115200);
//     Serial.print(F("[SX1262] Initializing ... "));
//     Serial.print("Model: "); Serial.println(ESP.getChipModel());
//     Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
//     Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
// 	Serial.println("\n\nCompiled at " __DATE__ " " __TIME__);

//     if(!MAC_init(0x01, false)) {
//         Serial.println("LoRa init failed");
//         while(1);
//     }

//     mac_data_t data = "PROVA";
//     MAC_send(0x02, data, strlen((char*)data));
// }

// void loop() {
//     scheduler_run();
// }

/* === Proves CRC ===
mac_data_t data = "PROVA";

_prepareTxPDU(0xFF, data, strlen((char*) data));
_printPDU(&txPDU);

mac_crc_t crc = txPDU.crc;
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // vàlid
txPDU.tx ^= 0x01; // canviar un bit
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // no vàlid
txPDU.tx ^= 0x01; // canviar bit de nou
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // vàlid
txPDU.crc ^= 0x01; // canviar bit de crc
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // no vàlid

mac_data_t data2 = "";

_prepareTxPDU(0xFF, data2, strlen((char*) data2));
_printPDU(&txPDU);

crc = txPDU.crc;
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // vàlid
txPDU.tx ^= 0x01; // canviar un bit
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // no vàlid
txPDU.tx ^= 0x01; // canviar bit de nou
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // vàlid
txPDU.crc ^= 0x01; // canviar bit de crc
Serial.printf("Is CRC valid? %d\n", _verifyCRC(&txPDU)); // no vàlid


=== PROVES LORA_TO_PDU ===
lora_data_t data = {0x01,0x02,0x03, 0x04,0x05,0x04,'H','I','!','!',0x0A};
_printLora(data, 11);

mac_pdu_t pdu;
_LoraToPDU(data, 11, &pdu);
_printPDU(&pdu);
size_t len = _PDUtoLora(&pdu, data);
_printLora(data, len);

lora_data_t data2 = {0x01,0x02,0x03, 0x04,0x02,0x00, 0x0A};
_printLora(data2, 7);

_LoraToPDU(data2, 7, &pdu);
_printPDU(&pdu);
len = _PDUtoLora(&pdu, data2);
_printLora(data2, len);

// lora_data_t data3;
for (size_t i = offsetof(mac_pdu_t, data); i < LORA_MAX_SIZE; i++)
{
    data[i] = 'A' + (i % 26);  
}

data[offsetof(mac_pdu_t, dataLength)] = MAC_MAX_DATA_SIZE;

_printLora(data, 255);

_LoraToPDU(data, 255, &pdu);
_printPDU(&pdu);
len = _PDUtoLora(&pdu, data);
_printLora(data, len);
*/
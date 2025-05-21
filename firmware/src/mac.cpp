#include <Arduino.h>

#include "mac.h"
#include "lora.h"
#include "scheduler.h"
#include "utils.h"
#include "RingBuffer.h"
#include "mac_buffer.h"

enum mac_event_t {
    TX_E,             // Iniciar TX
    TOUT_BUSY_E,      // Fi tout de canal ocupat
    TOUT_ACK_E,       // Timeout recepció ACK
    RX_ACK_E,         // Recepció ACK

#ifdef MAC_DUTY_CYCLE
    TOUT_DUTY_E,      // Fi temps per duty cycle
#endif
};

enum lora_event_t {
    BUSY_E,           // Canal LORA ocupat
    IDLE_E            // Canal LORA lliure
};

enum mac_state_t {
    IDLE_S,           // Esperant
    WAIT_ACK_S,       // Esperant recepció ACK
    WAIT_CHAN_FREE_S, // Esperant canal LoRa lliure

#ifdef MAC_DUTY_CYCLE
    WAIT_DUTY_CYCLE_S, // Esperant temps per complir amb duty cycle establert
#endif
};


volatile static mac_state_t fsmState = mac_state_t::IDLE_S;
static mac_tx_callback_t onSend = nullptr;
static mac_tx_callback_t onTxFailed = nullptr;
static mac_rx_callback_t onReceive = nullptr;

static node_address_t self;

static mac_pdu_t txPDU; // PDU en transmissió

volatile static uint8_t currentTxRetry = 0;
volatile static uint8_t currentBEBRetry = 0;

static RingBuffer lastFramesIDs(MAC_QUEUE_SIZE);

// Valors per informació. Per si mai fan falta...
static int CRCErrors = 0, failedTransmissions = 0, succeededTransmissions = 0, framesReceived = 0; 

static Task* txTimeoutTask;

// Mètodes per generar i interactuar amb PDU
static void _preparePDU(mac_pdu_t* pdu, node_address_t rx, const mac_data_t data, size_t length, uint8_t retry = 0, bool isAck = false, const mac_pdu_t * const PDUtoACK = nullptr);
static void _printPDU(const mac_pdu_t* const pdu);
static void _set_retry_count(mac_pdu_t* pdu, uint8_t retry);
static mac_id_t _getRandomID();
static mac_crc_t _computeCRC(const mac_pdu_t* const pdu);
static bool _verifyCRC(const mac_pdu_t* const pdu);
static bool _is_ack_valid(const mac_pdu_t * const pdu);
static size_t _PDUtoLora(const mac_pdu_t * const pdu, lora_data_t lora);
static void _LoraToPDU(const lora_data_t lora, size_t length, mac_pdu_t * pdu);

// Mètodes i ajudes per FSM
static void _mac_fsm(mac_event_t e);
static void _mac_fsm_event_tout_ack(void);
static void _mac_fsm_event_tout_busy(void);
static void _mac_fsm_event_tx(void);
static void _mac_fsm_event_duty_timeout(void);
static void _start_beb_timeout(uint8_t attempt);
static void _setup_ack_reception(void);
static void _apply_duty_cycle_delay();

// Mètodes i ajudes per transmissions
static bool _attempt_transmission(uint8_t retry_count);
static mac_err_t _inner_send(node_address_t rx, const mac_data_t data, size_t length, bool isAck = false, const mac_pdu_t * const referedPDU = NULL);
static mac_err_t _send_pdu(const mac_pdu_t* const pdu);
static void _send_ack(const mac_pdu_t * const refPdu);

// Callbacks de capa inferior, i per generar els de superior
static void _onLoraReceived(void);
static void _received_mac(void);
static void _sent_mac(void);
static void _txError_mac(void);

// ============== MÈTODES PÚBLICS ==============

bool MAC_init(node_address_t selfAddr) {
    if (!IS_ADDRESS_VALID(selfAddr)) {
        _PE("[MAC] Invalid address (0x%02X)", selfAddr);
        return false;
    }

    if(!LoRa_init() || !LoRaRAW_init()) {
        return false;
    }
    
    self = selfAddr;
    LoRaRAW_onReceive(_onLoraReceived);
    _PI("[MAC] Init");
    return true;
}

void MAC_deinit() {
    _PI("[MAC] Deinit");
    LoRa_deinit();
    onSend = onTxFailed = nullptr;
    onReceive = nullptr;
}

mac_err_t MAC_send(node_address_t rx, const mac_data_t data, size_t length, uint16_t* ID) {
    _PI("[MAC] Preparing to send");

    if(length > MAC_MAX_DATA_SIZE) {
        _PW("[MAC] Max length exceeded (%d)", length);
        return mac_err_t::MAC_ERR_MAX_LENGTH;
    }
    if(rx == self) {
        _PW("[MAC] RX address (0x%02X) cannot be self (0x%02X)", rx, self);
        return mac_err_t::MAC_ERR_INVALID_ADDR;
    }
    
    mac_pdu_t tempPDU;
    _preparePDU(&tempPDU, rx, data, length, 0, false);
    _PI("[MAC] PDU ready");
    _printPDU(&tempPDU);

    // Verifiquem aquí i no després de push, ja que sinó sempre serà fals! No canviarà estat de MAC_isAvailable
    // ja que interrupció només estableix un flag, que no es comprova fins que s'executa la tasca (a partir de loop)
    bool isMacAvailable = MAC_isAvailable();

    // No s'utilitzen prioritats (de moment), però per si de cas. Per defecte, baixa
    MACbuff_pushTx(tempPDU, MACBUFF_PRIORITY_LOW); // Guardem dades a buffer de transmissió

    // Només generem esdeveniment si no hi ha transmissió en curs; si n'hi ha, en acabar-ne una ja farà comprovació de cua
    if(isMacAvailable) {
        scheduler_once(_mac_fsm_event_tx); // Programem per no bloquejar durant massa temps
        _PI("[MAC] FSM transmission scheduled");
    }
    else {
        _PI("[MAC] Queueing transmission (Position: %d)", MACbuff_getTxSize());
    }
    // Guardar ID de PDU a apuntador proporcionat
    if(ID)
        *ID = tempPDU.id;

    return mac_err_t::MAC_SUCCESS;
}

node_address_t MAC_receive(mac_data_t* data, size_t* length) {
    mac_pdu_t pdu;
    MACbuff_popRx(pdu);
    *length = pdu.dataLength;
    memcpy(data, pdu.data, pdu.dataLength);
    // (*data)[*length] = '\0';
    return pdu.tx;
}

// Retorna missatges pendents de ser "rebuts" per capa superior
size_t MAC_toReceive() { return MACbuff_getRxSize(); }

// Només podem enviar si estem en IDLE; si no, hi ha transmissió en curs
bool MAC_isAvailable() { return fsmState == mac_state_t::IDLE_S && MACbuff_isTxEmpty(); }

void MAC_onReceive(mac_rx_callback_t cb) { onReceive = cb; }

void MAC_onSend(mac_tx_callback_t cb) { onSend = cb; }

void MAC_onTxFailed(mac_tx_callback_t cb) { onTxFailed = cb; }

// ============== MÈTODES PRIVATS ==============

static void _send_ack(const mac_pdu_t * const refPdu) {
    mac_pdu_t ackPDU;
    _preparePDU(&ackPDU, refPdu->tx, (uint8_t*)"", 0, 0, true, refPdu);

    // Establim potència de transmissió de l'ACK segons el nombre de reintents que s'han fet
    // per rebre el frame. No té sentit que quan rebem frame sigui perquè la potència era màxima
    // però que enviem ACK amb potència mínima: receptor probablement NO rebrà ACK així.
    int power = LORA_TX_POW + (refPdu->flags.retry * MAC_TX_POW_STEP);
    LoRaRAW_setTxPower(power);

    // Enviem ACK
    _send_pdu(&ackPDU);

    // Reestablim potència a la mínima per no afectar següents transmissions
    LoRaRAW_setTxPower(LORA_TX_POW);
}

// Envia una PDU per LoRa, convertint de PDU a dades lora.
// Retorna mac_err_t amb l'estat de transmissió
static mac_err_t _send_pdu(const mac_pdu_t* const pdu) {
    lora_data_t data;
    size_t dataLen = _PDUtoLora(pdu, data);
    lora_tx_error_t state = LoRaRAW_send(data, dataLen);
    if (state != lora_tx_error_t::LORA_SUCCESS) {
        return mac_err_t::MAC_ERR;
    }
    return mac_err_t::MAC_SUCCESS;
}

// --- GENERACIÓ PDU ---
// Prepara una PDU a partir dels paràmetres donats
static void _preparePDU(mac_pdu_t* pdu, node_address_t rx, const mac_data_t data, size_t length, uint8_t retry, bool isAck, const mac_pdu_t * const PDUtoACK) {
    pdu->tx = self;
    pdu->rx = rx;
    pdu->id = isAck ? PDUtoACK->id : (retry > 0 ? txPDU.id : _getRandomID()); // si ACK, utilitzem PDU donada; si retry > 0, no modifiquem ID
    // pdu->id = isAck ? PDUtoACK->id+self : (retry > 0 ? txPDU.id : _getRandomID()); // versió anterior on ID = ID + ID_rx
    pdu->flags.isACK = isAck;
    pdu->flags.retry = retry;
    pdu->flags.reserved = 0b11111;
    pdu->dataLength = length;
    // strcpy((char*)pdu->data, (char*)data); @todo: revisar per si mai falla, abans estava així i no sé perque ho vaig posar
    memcpy((char*)pdu->data, (char*)data, length);
    pdu->crc = _computeCRC(pdu);
}

static size_t _PDUtoLora(const mac_pdu_t * const pdu, lora_data_t lora) {
    // PDU té el format [TX|RX|ID_H|ID_L|FLAGS|LEN|DATA|...|DATA|RUBISH|...|CRC]
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

static void _LoraToPDU(const lora_data_t lora, size_t length, mac_pdu_t * pdu) {
    // LORA té el format [TX|RX|ID_H|ID_L|FLAGS|LEN|DATA|...|DATA|CRC]
    size_t index = 0;

    // Copiar header + data sense CRC directament a pdu (segueix mateixa estructura)
    size_t data_length = length - sizeof(mac_crc_t); 
    memcpy(pdu, &lora[index], data_length);
    index += data_length;

    memcpy(&pdu->crc, &lora[index], sizeof(mac_crc_t));
}

// CRC-8/SMBUS: https://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
static mac_crc_t _computeCRC(const mac_pdu_t* const pdu) {
    mac_crc_t crc = 0x00;
    // Posició fins on cal calcular CRC (fins final de dades)
    size_t data_end_pos = offsetof(mac_pdu_t, data) + pdu->dataLength;
    for (size_t i = 0; i < data_end_pos; i++)
    {
        crc = crc ^ ((char*)pdu)[i];
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

// Verifica el CRC de la PDU donada, recalculant-lo i comparant amb el rebut
static bool _verifyCRC(const mac_pdu_t* const pdu) {
    mac_crc_t expected = _computeCRC(pdu);
    mac_crc_t obtained = pdu->crc;
    return expected == obtained;
}

// Genera un identificador aleatori pel frame. Random NO inclou valor màxim (així entre [1, 2^n-1])
static mac_id_t _getRandomID() { return (mac_id_t)random(1, (1 << (8 * sizeof(mac_id_t)))); }

// Verifica si l'ACK de la PDU donada és vàlid
// És vàlid si té flag d'ACK, el transmisor és el receptor de l'últim que hem enviat
// i l'ID del frame és l'ID de la trama que s'està transmetent
// A més, és necessari que l'estat de capa MAC sigui esperant ACK
static bool _is_ack_valid(const mac_pdu_t * const pdu) {
    return pdu->flags.isACK && pdu->tx == txPDU.rx && pdu->id == txPDU.id && fsmState == mac_state_t::WAIT_ACK_S;
    // return pdu->flags.isACK && pdu->tx == txPDU.rx && pdu->id == txPDU.id + txPDU.rx && fsmState == mac_state_t::WAIT_ACK_S;
}

/* *************************** */
/* * CALLBACKS CAPA INFERIOR * */
/* *************************** */

static void _onLoraReceived(void) {
    /*
        Callback executat quan es produeix una recepció a capa inferior LoRa.
        1. Obté dades de capa inferior
        2. Verifica CRC. Si no és vàlid, descarta.
        3. Comprova si s'ha rebut anteriorment aquest frame (per ID)
            3.1.1. Si no s'ha rebut, verifica si és un ACK (ja que si és ACK implícit no serem els recepetors directament)
            3.1.2. Si no és ACK, verifica si és per nosaltres. Si no ho és, descarta; 
            3.1.3. Si és per nosaltres, afegeix ID a cua d'últims IDs vists, i executa _received_mac() per avisar capa superior
            3.1.4. Si és ACK, avisa a FSM de la recepció d'ACK. No guarda ID cua d'últimes recepcions, 
                   no es poden repetir recepcions d'ACK i només ompliria la llista amb IDs innecessaris
            
            3.2.1. Si s'ha rebut anteriorment és perquè eren dades i per nosaltres.
                   El motiu de la re-recepció és que el transmissor no ha rebut ACK, o no el vam poder enviar.
            3.2.2. Intenta enviar un ACK explícit, amb adreça de receptor nul·la (0x00), l'ID que el TX espera
                   i el flag isAck establert.
    */
    _PI("[MAC] Frame rcv");

    lora_data_t data;
    size_t len;
    if(!LoRaRAW_receive(data, &len)) {
        _PW("[MAC] Recieve ERR");
        return;
    }

    // Per ser vàlid mínim a de tenir la mida del header, sense dades, que seria un ACK
    if(len < MAC_PDU_HEADER_SIZE) {
        _PW("[MAC] Frame too short (%d)", len);
        return;
    }

    mac_pdu_t receivedPDU;

    _LoraToPDU(data, len, &receivedPDU);

    if(!_verifyCRC(&receivedPDU)) {
        CRCErrors++;
        _PW("[MAC] CRC error (%d)", CRCErrors);
        DUMP_ARRAY(&receivedPDU, len);
        return;
    }
    
    _PI("Received valid PDU from LORA");
    _printPDU(&receivedPDU);
    
    mac_id_t rcvID = receivedPDU.id;
    bool seen = lastFramesIDs.contains(rcvID);

    if (seen) { // Si ja l'hem vist abans és perquè era un frame per nosaltres -> enviar ACK sense notificar
        _PI("[MAC] ID already received: %d", rcvID);
        _send_ack(&receivedPDU);
    }
    else if (receivedPDU.rx == self) {
        if (_is_ack_valid(&receivedPDU)) { // Si és ACK, generem esdeveniment a FSM; no s'ha d'enviar ACK
            _PI("[MAC] ACK Received from 0x%02X", receivedPDU.tx);
            _mac_fsm(mac_event_t::RX_ACK_E);
        }
        else { // Si no és ACK, són dades
            lastFramesIDs.enqueue(rcvID);
            framesReceived++;
            _PI("[MAC] Frame for higher layer");

            _send_ack(&receivedPDU); // Enviar ACK explícit 
            
            // Prioritats no utilitzades (de moment) per res; per defecte a baixa
            MACbuff_pushRx(receivedPDU, MACBUFF_PRIORITY_LOW); // Guardar recepció a buffer

            // @todo: Potser es pot programar amb scheduler per sortir-ne abans?
            _received_mac(); // Notificar capa superior de nova recepció
        }
    }
    else {
        _PI("[MAC] Frame not for self (rx=0x%02X)", receivedPDU.rx);
    }
}

/* *************************** */
/* *   MÀQUINA ESTATS MAC    * */
/* *************************** */
static void _mac_fsm(mac_event_t e) {
    // Obtenim estat canal per poder-ho utilitzar com a esdeveniment (aplicar beb)
    lora_event_t lora_e = (lora_event_t)LoRaRAW_isAvailable();
    // lora_e = lora_event_t (random(0, 3)/2); // FICTICI, ELIMINAR, NOMÉS PER SIMULAR ESTAT CANAL (33%)
    // _PI("[MAC] FSM:\tSTATE %d\tMAC %d\tLORA %d", fsmState, e, lora_e);
    
    switch (fsmState) {
        case IDLE_S:
            if (e == TX_E && !MACbuff_isTxEmpty()) {
                MACbuff_popTx(txPDU);
                currentTxRetry = 0; 
                if (lora_e == IDLE_E) {
                    _attempt_transmission(currentTxRetry);
                } else { // Channel busy
                    _PI("[MAC] Channel busy for retry, applying BEB");
                    _start_beb_timeout(currentBEBRetry++);
                }
            } else if (e == TX_E) {
                _PI("[MAC] TX requested but queue empty");
                LoRaRAW_startReceiving();
            }
            break;
            
        case WAIT_CHAN_FREE_S:
            if (e == TOUT_BUSY_E) {
                if (lora_e == IDLE_E) {
                    _PI("[MAC] Channel now free after BEB, attempting transmission");
                    _attempt_transmission(currentTxRetry);
                } else { // Canal ocupat, apliquem BEB de nou
                    _PI("[MAC] Channel still busy after BEB timeout, retrying BEB");
                    _start_beb_timeout(currentBEBRetry++);
                }
            }
            break;
            
        case WAIT_ACK_S:
            if (e == RX_ACK_E) {
                _PI("[MAC] ACK received");
                scheduler_stop(txTimeoutTask);
                succeededTransmissions++; // si rebem ack és perquè ja eren dades
                _sent_mac();  //  @todo; IMPORTANT SI TEMPS MOLT ELEVAT, EXECUTAR AMB SCHEDULER!
                _apply_duty_cycle_delay();
            } else if (e == TOUT_ACK_E) {
                _PI("[MAC] ACK timeout");
                // Comprovar si s'ha arribat a màxim de reintents
                if (currentTxRetry > MAC_MAX_RETRIES) {
                    _PW("[MAC] Max retries (%d) reached, transmission failed", MAC_MAX_RETRIES);
                    _txError_mac();
                    _apply_duty_cycle_delay();
                } else {
                    // Encara queden reintents
                    _PI("[MAC] Retry %d/%d", currentTxRetry, MAC_MAX_RETRIES);
                    
                    // Enviar o aplicar BEB segons estat canal
                    if (lora_e == IDLE_E) {
                        _attempt_transmission(currentTxRetry);
                    } else { // Channel busy
                        _PI("[MAC] Channel busy for retry, moving to WAIT_CHAN_FREE");
                        _start_beb_timeout(currentBEBRetry++);
                    }
                }
            }
            break;
        #ifdef MAC_DUTY_CYCLE
        case WAIT_DUTY_CYCLE_S:
            if (e == TOUT_DUTY_E) {
                _PI("[MAC] Duty cycle timeout");
                fsmState = mac_state_t::IDLE_S;
                scheduler_once(_mac_fsm_event_tx);
            }
            break;
        #endif
        default:
            _PE("[MAC] Unknown state: %d", fsmState);
            fsmState = IDLE_S; // Reset a estat conegut
            break;
    }
}

static void _apply_duty_cycle_delay() {
    
    #ifdef MAC_DUTY_CYCLE
        fsmState = mac_state_t::WAIT_DUTY_CYCLE_S;
        long airtime = LoRaRAW_getTimeOnAir(txPDU.dataLength + MAC_PDU_HEADER_SIZE);
        long airtime_with_retry_ms = airtime*(currentTxRetry)/1000;
        long duty_cycle_delay = airtime_with_retry_ms*(100-MAC_DUTY_CYCLE);
        _PE("[MAC] Duty cycle delay: %dms", duty_cycle_delay);
        scheduler_once(_mac_fsm_event_duty_timeout, duty_cycle_delay);
    #else
        fsmState = mac_state_t::IDLE_S;
        // @todo: si es vol afegir un retard per evitar enviar de nou després d'una transmissió, modificar aquí!
        scheduler_once(_mac_fsm_event_tx);
    #endif
}

// Mètode d'ajuda per establir valor de reintents, recalculant CRC
static void _set_retry_count(mac_pdu_t* pdu, uint8_t retry) {
    pdu->flags.retry = retry;
    pdu->crc = _computeCRC(pdu);
}

// Intenta enviar PDU guardada a txPDU; recalcula PDU amb nombre intents donat i nou CRC
// Ajusta potència de TX en funció de reintent
static bool _attempt_transmission(uint8_t retry_count) {
    _set_retry_count(&txPDU, retry_count); // Estableix nombre reintents i nou CRC

    // Modificar potència TX segons reintent
    int power = LORA_TX_POW + (retry_count * MAC_TX_POW_STEP);
    LoRaRAW_setTxPower(power);

    mac_err_t state = _send_pdu(&txPDU); // Envia PDU per LoRa

    if (state == MAC_SUCCESS) {
        _PI("[MAC] Frame sent successfully%s, waiting for ACK", retry_count > 0 ? " after retry" : "");
        currentBEBRetry = 0; // S'ha aconseguit enviar, posem a 0 
        currentTxRetry++; // Hem fet un intent de TX
        _setup_ack_reception(); // En enviament OK, esperem ACK
    } else {
        _PI("[MAC] Send failed%s", retry_count > 0 ? " after retry" : "");
        _start_beb_timeout(currentBEBRetry++);
    }
    return true;
}

// Inicia recepció d'ACK, calculant timeout
// @todo: potser es pot introduir una mica d'aleatorietat, per evitar col·lisions múltiples
// si dos nodes intenten enviar just a la mateixa vegada? Reintents els farien al mateix temps sino
static void _setup_ack_reception(void) {
    fsmState = WAIT_ACK_S;

    LoRaRAW_startReceiving();
    
    // Calcula i programa timeout
    // long airtime_us = LoRaRAW_getTimeOnAir(txPDU.dataLength + MAC_PDU_HEADER_SIZE);
    long ack_airtime_us = LoRaRAW_getTimeOnAir(MAC_PDU_HEADER_SIZE);

    // uint32_t timeout_ms = 3 * MAC_ACK_TIMEOUT_FACTOR * ack_airtime_us / 1000;
    uint32_t timeout_ms = MAC_ACK_TIMEOUT_FACTOR * ack_airtime_us / 1000;
    txTimeoutTask = scheduler_once(_mac_fsm_event_tout_ack, timeout_ms);
    _PI("[MAC] Timeout d'ACK: %dms (%dus airtime)", timeout_ms, ack_airtime_us);
}

// Mètodes per generar esdeveniments a FSM a través de scheduler
static void _mac_fsm_event_tout_ack(void) { _mac_fsm(mac_event_t::TOUT_ACK_E); }
static void _mac_fsm_event_tout_busy(void) { _mac_fsm(mac_event_t::TOUT_BUSY_E); }
static void _mac_fsm_event_tx(void) { _mac_fsm(mac_event_t::TX_E); }
static void _mac_fsm_event_duty_timeout(void) { 
    #ifdef MAC_DUTY_CYLCE 
        _mac_fsm(mac_event_t::TOUT_DUTY_E); 
    #endif
}

static void _start_beb_timeout(uint8_t attempt) {
    _PI("[MAC] Waiting chann free (%d)", attempt);
    fsmState = WAIT_CHAN_FREE_S;
    attempt = MIN(attempt, MAC_MAX_BEB_RETRY); // Limitar a valor màxim
    uint32_t bebTimeout = random(0, (1 << attempt) + 1) * MAC_BEB_SLOT_MS; // Calcular timeout de backoff. Random + 1 perquè no inclou extrem màxim
    txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout); // Programar timeout
    _PI("[MAC] Timeout BEB: %dms", bebTimeout);
    LoRaRAW_startReceiving();
}


/* *************************** */
/* * CALLBACKS CAPA SUPERIOR * */
/* *************************** */

static void _sent_mac(void) {
    _PI("[MAC] Sent. Notify higher layer?");
    LoRaRAW_startReceiving();
    if(!txPDU.flags.isACK && onSend != nullptr) { // Notificar només si dades (no ACK)
        onSend(txPDU.id); // Notifiquem proporcionant ID
    }
}

static void _received_mac(void) {
    if(onReceive != nullptr) {
        onReceive();
    }
}

static void _txError_mac(void) {
    failedTransmissions++;
    _PW("[MAC] TX error (%d)", failedTransmissions);
    LoRaRAW_startReceiving();
    if(!txPDU.flags.isACK && onTxFailed != nullptr) { // Notificar només si dades (no ACK) @note: no sembla ser necessari si sendack no passa per fsm
        onTxFailed(txPDU.id); // Notifiquem proporcionant ID
    }
}

#if LOG_LEVEL <= LOG_LEVEL_INFO
static void _printPDU(const mac_pdu_t* const pdu) {
    // Intenta mostrar en ASCII; mostra també en HEX per si caràcters no imprimibles4
    _PI("[MAC] FRAME: TX=%02X RX=%02X ID=%d D-LEN=%d DATA=%.*s CRC=%d ACK=%d RETRY=%d", 
        pdu->tx, pdu->rx, pdu->id, pdu->dataLength, pdu->dataLength, pdu->data, pdu->crc, pdu->flags.isACK, pdu->flags.retry);
    // Serial.printf("===== PDU =====\nTX\t%d\nRX\t%d\nID\t%d\nD-LEN\t%d\nDATA\t%.*s\nD-HEX\t", 
    // pdu->tx, pdu->rx, pdu->id, pdu->dataLength, pdu->dataLength, pdu->data);
    // for (int i = 0; i < pdu->dataLength; i++) 
    //     Serial.printf("%02X ", pdu->data[i]); 
    // Serial.printf("\nCRC\t%d\nACK\t%d\nRETRY\t%d\n===============\n", pdu->crc, pdu->flags.isACK, pdu->flags.retry);
}
#else
static void _printPDU(const mac_pdu_t* const pdu) {}
#endif
// #include <Arduino.h>

// #include "mac.h"
// #include "lora.h"
// #include "scheduler.h"
// #include "utils.h"
// #include "RingBuffer.h"

// // enum mac_event_t {
// //     TX_E,             // Iniciar TX
// //     TOUT_BUSY_E,      // Fi tout de canal ocupat 
// //     TOUT_ACK_E,       // Timeout recepció ACK
// //     RX_E,             // Nova recepció
// //     RX_ACK_E,         // Recepció ACK
// // };

// // enum lora_event_t {
// //     BUSY_E,           // Canal LORA ocupat
// //     IDLE_E            // Canal LORA lliure
// // };

// // enum mac_state_t {
// //     IDLE_S,           // Esperant
// //     TRANSMITTING_S    // TX propia en curs
// // };

// enum mac_event_t {
//     TX_E,             // Iniciar TX
//     TOUT_BUSY_E,      // Fi tout de canal ocupat 
//     TOUT_ACK_E,       // Timeout recepció ACK
//     RX_E,             // Nova recepció
//     RX_ACK_E,         // Recepció ACK
// };

// enum lora_event_t {
//     BUSY_E,           // Canal LORA ocupat
//     IDLE_E            // Canal LORA lliure
// };

// enum mac_state_t {
//     IDLE_S,           // Esperant
//     TRANSMITTING_S    // TX propia en curs
// };

// volatile static mac_state_t fsmState = mac_state_t::IDLE_S;
// static mac_callback_t onSend = NULL;
// static mac_callback_t onReceive = NULL;
// static mac_callback_t onTxFailed = NULL;

// static mac_addr_t self;

// static mac_pdu_t txPDU, rxPDU;

// volatile static uint8_t currentTxRetry = 0;
// volatile static uint8_t currentBEBRetry = 0;
// static bool isGateway = false;

// // static mac_id_t lastFramesIDs[MAC_QUEUE_SIZE];
// static RingBuffer lastFramesIDs(MAC_QUEUE_SIZE);

// static Task* txTimeoutTask;

// mac_id_t _getRandomID();
// mac_crc_t _computeCRC(const mac_pdu_t* const pdu);
// void _getPDU(const mac_data_t * const data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx, mac_id_t id = 0);
// void _printPDU(const mac_pdu_t* const pdu);
// bool _verifyCRC(const mac_pdu_t* const pdu);
// bool _is_ack_pdu_valid(const mac_pdu_t * const pdu);
// void _mac_fsm(mac_event_t e);
// void _mac_fsm_event_tout_ack(void);
// void _mac_fsm_event_tout_busy(void);
// bool _lora_data_to_mac_pdu(lora_data_t * const lora, mac_pdu_t * const pdu);
// bool _mac_pdu_to_lora_data(lora_data_t * const lora, mac_pdu_t * const pdu);

// void _onLoraSent(void);
// void _onLoraReceived(void);
// void _received_mac(void);
// void _sent_mac(void);
// void _txError_mac(void);

// bool MAC_init(mac_addr_t selfAddr, bool is_gateway) {
//     // Filtrar que selfAddr no sigui adreça restringida (0x00 i 0xFF)
//     self = selfAddr;
//     LoRa_onSend(_onLoraSent);
//     LoRa_onReceive(_onLoraReceived);
//     _PL("[MAC] Init");
//     return LoRa_init();
// }

// void MAC_deinit() {
//     _PL("[MAC] Deinit");
//     LoRa_deinit();
//     onReceive = onSend = onTxFailed = NULL;
// }

// mac_err_t MAC_send(mac_addr_t rx, const mac_data_t data, uint8_t length) {
//     // 1. Verificar paràmetres (mida data, rx)
//     // 2. Generar PDU (seran dades de LoRa)
//     // 3. Iniciar tasca amb tout
//     // 4. Enviar LoRa_send()
//     if(length > MAC_MAX_DATA_SIZE)
//         return MAC_ERR_MAX_LENGTH;
//     if(rx == self)
//         return MAC_ERR_INVALID_ADDR;
//     if(!MAC_isAvailable()) // No permetre segon enviament si un està pendent. Sobreescriuria PDU TX actual
//         return MAC_ERR_TX_PENDING;

//     _getPDU((const mac_data_t*)data, length, &txPDU, rx);
//     _mac_fsm(mac_event_t::TX_E);

//     return MAC_ERR_SUCCESS;
// }

// mac_addr_t MAC_receive(mac_data_t* data, uint8_t* length) {
//     *length = rxPDU.length;
//     memcpy(data, rxPDU.data, rxPDU.length);
// }

// bool MAC_isAvailable() {
//     // Només interessa que nosaltres estiguem IDLE.
//     // Si LoRa ocupat ja ho gestionarem amb backoff a FSM.
//     return fsmState == mac_state_t::IDLE_S;
// }

// void MAC_onReceive(mac_callback_t cb) {
//     onReceive = cb;
// }

// void MAC_onSend(mac_callback_t cb) {
//     onSend = cb;
// }

// void MAC_onTxFailed(mac_callback_t cb) {
//     onTxFailed = cb;
// }

// // ============== MÈTODES PRIVATS ==============

// // --- GENERACIÓ PDU ---

// void _getPDU(const mac_data_t * const data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx, mac_id_t id) {
//     pdu->tx = self;
//     pdu->rx = rx;
//     memcpy(pdu->data, data, length);
//     pdu->id = id ? id : _getRandomID(); // si es proporciona ID s'utilitza aquell; sino, es genera aleatori.
//     pdu->length = length;
//     pdu->crc = _computeCRC(pdu);
//     _PL("[MAC] GET PDU");
//     _printPDU(pdu);
//     _PF("\tLen: %d", pdu->length); _PL();
//     _PF("\tTX: %d", pdu->tx); _PL();
//     _PF("\tRX: %d", pdu->rx); _PL();
//     _PF("\tData: %s", pdu->data); _PL();
//     _PF("\tID: %d", pdu->id); _PL();
//     _PF("[MAC] CRC: %d", pdu->crc); _PL();
// }

// // https://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
// // CRC-8/SMBUS
// mac_crc_t _computeCRC(const mac_pdu_t* const pdu) {
//     // Valor inicial crc
//     mac_crc_t crc = 0x00; 
    
//     // Processar dades de PDU
//     for (uint8_t i = 0; i < (pdu->length+2*MAC_ADDRESS_SIZE+MAC_ID_SIZE); ++i) {
//         crc = crc ^ ((uint8_t*)pdu)[i];  
//         // Processar bits del byte de dades actual
//         for (uint8_t j = 0; j < 8; ++j) {
//             if (crc & 0x80) {  
//                 crc = (crc << 1) ^ MAC_CRC8_POLY;  
//             } else {
//                 crc <<= 1; 
//             }
//         }
//     }
//     return crc; 
// }

// bool _verifyCRC(const mac_pdu_t* const pdu) {
//     mac_crc_t expected = _computeCRC(pdu);
//     mac_crc_t obtained = pdu->crc;
//     return expected == obtained;
// }

// mac_id_t _getRandomID() {
//     return (mac_id_t)random(1, MAC_MAX_ID);
// }

// bool _sendACK(const mac_pdu_t * const pdu) {
//     /*
//     Donada una PDU rebuda, genera un ACK.
//     Utilitzat únicament per a frames ja rebuts anteriorment (i processats).
//     Si és la 1a vegada que es reben, no es gestiona ACK per aquí,
//     sinó que es passa a capa superior (routing) i ja serà aquesta qui decidirà.
//     */
// }

// // ------------------------------

// void _onLoraSent(void) {
//     /*
//         Callback executat per capa inferior LoRa quan es produeix una transmissió.
//     */
//    _PL("[MAC] Frame sent");
// }

// void _onLoraReceived(void) {
//     /*
//         Callback executat quan es produeix una recepció a capa inferior LoRa.
//         Filtra que les dades siguin vàlides i per nosaltres.
//         Si és així, converteix dades rebudes en PDU de MAC, 
//         i genera esdeveniment de recepció; si no, ignora recepció.
//     */
//     _PL("[MAC] Frame rcv");

//     lora_data_t data;
//     if(!LoRa_receive(&data)) {
//         _PL("[MAC] _received_mac: ERR");
//         return;
//     }

//     _PP("Received Lora data: ");

//     _lora_data_to_mac_pdu(&data, &rxPDU);
//     if(!_verifyCRC(&rxPDU)) {
//         _PL("[MAC] _received_mac: CRC ERR");
//         return;
//     }

//     // if(rxPDU.rx != self) {
//     //     _PL("[MAC] _received_mac: NOT FOR SELF");
//     //     return;
//     // }

//     // Generar esdeveniment a FSM. El missatge rebut pot ser o no per nosaltres,
//     // depenent de si estem esperant ACK o no.
//     _mac_fsm(mac_event_t::RX_E);
// }

// // --- FSM ---
// void _mac_fsm(mac_event_t e) {
//     /*
//         FSM principal capa MAC. Gestiona enviaments, aplicant BEB,
//         recepció d'"ACK" i recepció d'altres dades.
//     */

//     // Retorna bool (0, 1): 0 si ocupat (not available) -> lora_event_t::busy
//     lora_event_t lora_e = (lora_event_t)LoRa_isAvailable();
//     if(fsmState == mac_state_t::IDLE_S) {
//         if(e == mac_event_t::TX_E) {
//             if(lora_e == lora_event_t::IDLE_E) { 
//                 /*
//                     MAC i LORA lliures. Volem enviar. 
//                     1. Obtenim temps de timeout segons mida de dades.
//                     2. Programem tout amb scheduler i enviem amb LoRa
//                     3. Si error en enviar, aturar tasca anterior

//                     TODO: Falta filtrar si és gateway o no
//                 */
//                 _PM("[MAC] FSM: MAC Idle + Lora IDLE + TX ");
//                 fsmState = mac_state_t::TRANSMITTING_S;
//                 long airtime_us = LoRa_getTimeOnAir(txPDU.length+MAC_PDU_HEADER_SIZE);
//                 // Deixem de marge de timeout per rebre "ACK" 3 vegades més de l'esperat 
//                 // (l'esperat és 2*timeOnAir, ha d'anar i tornar la resposta, de mateixa mida com a molt)
//                 txTimeoutTask = scheduler_once(_mac_fsm_event_tout_ack, 6*airtime_us/1000);
//                 _PF("[MAC] Timeout ACK: %d\n", 6*airtime_us/1000);
//                 lora_data_t data;
//                 _mac_pdu_to_lora_data(&data, &txPDU);
//                 lora_tx_error_t state = LoRa_send(&data);
//                 if(state != LORA_SUCCESS) {
//                     scheduler_stop(txTimeoutTask); // cancel·lem abans que s'executi (per molt que passi el temps no s'haurà executat ja que necessita bucle de loop)
//                     _txError_mac();
//                 }
//             }
//             else if (lora_e == lora_event_t::BUSY_E) {
//                 /*
//                     MAC lliure. LORA ocupat. Volem enviar. 
//                     1. No és possible enviar si LoRa ocupat.
//                     2. Nou estat és TX
//                     3. Obtenir temps aleatori de backoff exponencial a partir de reintents de backoff
//                     4. Generar un esdeveniment un cop ha passat el temps
//                 */
//                 _PM("[MAC] FSM: MAC Idle + Lora BUSY + TX");
//                 fsmState = mac_state_t::TRANSMITTING_S;
//                 // Incrementar i limitar intent actual de backoff
//                 currentBEBRetry++;
//                 currentBEBRetry = MIN(currentBEBRetry, MAC_MAX_BEB_RETRY);
//                 // Calcular timeout de backoff
//                 uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
//                 // Programar reintent
//                 txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
//                 _PF("[MAC] Timeout BEB: %d\n", bebTimeout);
//             }
//         } 
//         else if (e == mac_event_t::TOUT_ACK_E){
//             /*
//                 MAC lliure i es genera timeout de recepció d'ACK. No hauria de ser possible. 
//             */
//            _PM("[MAC] FSM: MAC Idle + TOUT_ACK impossible");
//         }
//         else if (e == mac_event_t::TOUT_BUSY_E){
//             /*
//                 MAC lliure i es genera timeout de canal ocupat generat per BEB. No hauria de ser possible
//             */
//            _PM("[MAC] FSM: MAC Idle + TOUT_BUSY impossible");
//         }
//         else if (e == mac_event_t::RX_E) {
//             /*
//                 MAC lliure i es produeix recepció. És frame a retransmetre o per nosaltres. No pot ser d'ack (ja que estat MAC seria TX).
//                 Estat no canvia, únicament cal processar el rebut i, si és el cas, ja generarà altres esdeveniments
//             */
//            _PM("[MAC] FSM: MAC Idle + RX");
//            _received_mac();
//         }
//         else if (e == mac_event_t::RX_ACK_E) {

//         }
//     }
//     else if (fsmState == mac_state_t::TRANSMITTING_S){
//         if(e == mac_event_t::TX_E) {
//             if(lora_e == lora_event_t::IDLE_E) { 
//                 /*
//                     MAC ocupada i es vol tornar a enviar.
//                     Només pot significar un reintent de TX degut a TOUT_ACK.
//                     MAC_send() ja filtra nous TX amb dades diferents.
//                     S'ha de fer una retransmissió.
//                     @todo: Hauria de tenir en compte el valor actual de reintents
//                 */
//                 _PM("[MAC] FSM: MAC TX + LoRa IDLE + TX");

//                 fsmState = mac_state_t::TRANSMITTING_S;
//                 long airtime_us = LoRa_getTimeOnAir(txPDU.length+MAC_PDU_HEADER_SIZE);
//                 // Deixem de marge de timeout per rebre "ACK" 3 vegades més de l'esperat 
//                 // (l'esperat és 2*timeOnAir, ha d'anar i tornar la resposta, de mateixa mida com a molt)
//                 txTimeoutTask = scheduler_once(_mac_fsm_event_tout_ack, 6*airtime_us/1000);
//                 _PF("[MAC] Timeout ACK: %d\n", 6*airtime_us/1000);
//                 lora_data_t data;
//                 _mac_pdu_to_lora_data(&data, &txPDU);
//                 lora_tx_error_t state = LoRa_send(&data);
//                 if(state != LORA_SUCCESS) {
//                     scheduler_stop(txTimeoutTask); // cancel·lem abans que s'executi (per molt que passi el temps no s'haurà executat ja que necessita bucle de loop)
//                     _txError_mac();
//                 }
//             }
//             else if (lora_e == lora_event_t::BUSY_E) {
//                 /*
//                     MAC lliure. LORA ocupat. Volem enviar. 
//                     1. No és possible enviar si LoRa ocupat.
//                     2. Nou estat és TX
//                     3. Obtenir temps aleatori de backoff exponencial a partir de reintents de backoff
//                     4. Generar un esdeveniment un cop ha passat el temps
//                 */
//                 _PM("[MAC] FSM: MAC Idle + Lora BUSY + TX");
//                 fsmState = mac_state_t::TRANSMITTING_S;
//                 // Incrementar i limitar intent actual de backoff
//                 currentBEBRetry++;
//                 currentBEBRetry = MIN(currentBEBRetry, MAC_MAX_BEB_RETRY);
//                 // Calcular timeout de backoff
//                 uint32_t bebTimeout = random(0, 1 << currentBEBRetry)*MAC_BEB_FACTOR_MS;
//                 // Programar reintent
//                 txTimeoutTask = scheduler_once(_mac_fsm_event_tout_busy, bebTimeout);
//                 _PF("[MAC] Timeout BEB: %d\n", bebTimeout);
//             }
//         } 
//         else if (e == mac_event_t::RX_E) {
//             /*
//                 MAC ocupada i es produeix recepció. És frame a retransmetre o per nosaltres o ACK.
//                 Cal aturar tasca de timeout si ACK és vàlid (TX del rebut és anterior RX nostre, i ID actual és ID de TX + @RX)
//                 @todo: De moment únicament es processarà un missatge a la vegada. S'ignorarà recepció si no és ACK.
//             */
//             _PM("[MAC] FSM: MAC TX + RX");
//             if(_is_ack_pdu_valid(&rxPDU)) {
//                 _PM("[MAC] ACK Received");
//                 fsmState == mac_state_t::IDLE_S;
//                 scheduler_stop(txTimeoutTask);
//             }
//         }
//         else if (e == mac_event_t::TOUT_ACK_E) {
//             /*
//                 MAC ocupada i es genera timeout de recepció d'ACK.
//                 1. Incrementar comptador reintents
//                 2. Generar intent enviament de nou
//             */
//             _PM("[MAC] FSM: MAC TX + TOUT_ACK");
//             if(++currentTxRetry == MAC_MAX_RETRIES)
//                 _txError_mac();
//             else 
//                 _mac_fsm(mac_event_t::TX_E); // generar esdeveniment TX. Ja tindrà en compte currentTxRetry.
//         }
//         else if (e == mac_event_t::TOUT_BUSY_E) {
//             /*
//                 MAC Ocupada i es genera esdeveniment de fi de TOUT_BUSY.
//                 Cal fer un intent de transmissió de nou.
//             */
//            _PM("[MAC] FSM: MAC TX + TOUT_BUSY");
//            _mac_fsm(mac_event_t::TX_E);

//         }
//     }
// }

// void _mac_fsm_event_tout_ack(void) { _mac_fsm(mac_event_t::TOUT_ACK_E); }
// void _mac_fsm_event_tout_busy(void) { _mac_fsm(mac_event_t::TOUT_BUSY_E); }

// bool _mac_pdu_to_lora_data(lora_data_t * const lora, mac_pdu_t * const pdu) {
//     uint8_t* ptr = lora->data;
//     memcpy(ptr, pdu, pdu->length+MAC_PDU_HEADER_SIZE-MAC_CRC_SIZE);
//     ptr += pdu->length+MAC_PDU_HEADER_SIZE-MAC_CRC_SIZE;
//     memcpy(ptr, &pdu->crc, sizeof(mac_crc_t));
//     lora->length = txPDU.length+MAC_PDU_HEADER_SIZE;
//     return true;
// }

// bool _lora_data_to_mac_pdu(lora_data_t * const lora, mac_pdu_t * const pdu) {
//     if (lora->length < sizeof(mac_addr_t) * 2 + sizeof(mac_id_t) + sizeof(mac_crc_t)) {
//         _PF("[MAC] lora_to_mac_pdu: Invalid data size: %dB\n", lora->length);
//         return false;
//     }

//     uint8_t* ptr = lora->data; 
//     memcpy(&pdu->tx, ptr, sizeof(mac_addr_t));
//     ptr += sizeof(mac_addr_t);
//     memcpy(&pdu->rx, ptr, sizeof(mac_addr_t));
//     ptr += sizeof(mac_addr_t);
//     memcpy(&pdu->id, ptr, sizeof(mac_id_t));
//     ptr += sizeof(mac_id_t);
//     pdu->length = lora->length - (sizeof(mac_addr_t) * 2 + sizeof(mac_id_t) + sizeof(mac_crc_t));

//     memcpy(pdu->data, ptr, pdu->length);
//     ptr += pdu->length;
//     memcpy(&pdu->crc, ptr, sizeof(mac_crc_t));

//     return true;
// }

// bool _is_ack_pdu_valid(const mac_pdu_t * const pdu) {
//     return (pdu->tx == txPDU.rx && pdu->id == txPDU.id+txPDU.rx);
// }


// void _received_mac(void) {
//     /*
//     Executat per FSM quan s'ha rebut un frame que no és ACK (MAC està IDLE) i és
//     per nosaltres. CRC s'ha verificat en el moment de rebre frame `_onLoraReceived`
//     1. Mirar si ID ja rebut anteriorment
//         1.1. Si rebut, enviar un "ACK" (frame amb RX = 0x00, ID = ID esperat)
//         1.2. Si no rebut, passar a capa superior i guardar ID
//     */
//     _PM("[MAC] Received non-ack frame");
//     // Cal mirar si el missatge rebut és per nosaltres.
//     if(rxPDU.rx != self) {
//         _PL("[MAC] _received_mac: NOT FOR SELF");
//         return;
//     }

//     // Filtrem si rebut
//     mac_id_t rcvID = rxPDU.id;
//     bool seen = lastFramesIDs.contains(rcvID);
//     // for(int i = 0; i < MAC_QUEUE_SIZE; i++) {
//     //     if(lastFramesIDs[i] == rcvID) {
//     //         seen = true;
//     //         break;
//     //     }
//     // }

//     if (seen) {

//     }
//     else {
//         // Podem sobreescriure PDU de transmissió; estat actual és IDLE sí o sí, sino no s'executa aquest mètode
//         lastFramesIDs.enqueue(rcvID);
//         lastFramesIDs.printBuffer();
//         _getPDU((const mac_data_t *)"", 0, &txPDU, (mac_addr_t) 0x00, rxPDU.id + self);
//         _mac_fsm(mac_event_t::TX_E);
//     }
// }

// void _sent_mac(void) {
// }

// void _txError_mac(void) {
//     _PL("[MAC] TX ERR");
//     if(onTxFailed != NULL)
//         onTxFailed();
// }

// #ifdef DEBUG
// void _printPDU(const mac_pdu_t* const pdu) {
//     // En escriure, tipus com uint16_t (ID) es guarden en little endian
//     // Així, si ID és 60505 = 0xEC59 es mostraria com 59EC
//     for (uint8_t i = 0; i < (pdu->length+2*MAC_ADDRESS_SIZE+MAC_ID_SIZE); ++i) {
//         _PX(((char*)pdu)[i]);
//     }
//     // CRC
//     _PX(pdu->crc); _PL();
// }
// #endif


// void setup() {
//     Serial.begin(115200);

//     Serial.print(F("[SX1262] Initializing ... "));
//     Serial.print("Model: "); Serial.println(ESP.getChipModel());
//     Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
//     Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
//     Serial.println(esp_reset_reason());

//     if(!MAC_init(0x01, false)) {
//         _PL("ERR");
//         while(1);
//     }

//     mac_data_t data = "PROVAA";
//     MAC_send(0x03, data, strlen((char*)data));
// }

// void loop() {
//     scheduler_run();
// }



// /* === Proves CRC ===
// mac_pdu_t pdu;
// mac_data_t data = "PROVA";

// _getPDU((mac_data_t*)data, strlen((char*)data), &pdu, 0xff);

// mac_crc_t crc = pdu.crc;
// Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // vàlid
// pdu.tx ^= 0x01; // canviar un bit
// Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // no vàlid
// pdu.tx ^= 0x01; // canviar bit de nou
// Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu)); // vàlid


// === PROVES LORA_TO_PDU ===
// lora_data_t data = {0x01,0x02,0x2C,0x82,0x50,0x52,0x4F,0x56,0x41,0x86};
// data.length = 10;
// _printLora(&data);

// mac_pdu_t pdu;
// _lora_data_to_mac_pdu(&data, &pdu);
// _printPDU(&pdu);
// _PF("CRC: %d\n", _verifyCRC(&pdu));
// */
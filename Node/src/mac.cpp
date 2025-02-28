#include <Arduino.h>

#include "mac.h"
#include "scheduler.h"
#include "utils.h"

static mac_callback_t onSend = NULL;
static mac_callback_t onReceive = NULL;

static mac_addr_t self;

mac_id_t _getRandomID();
mac_crc_t _computeCRC(const mac_pdu_t* const pdu);
bool _getPDU(const mac_data_t * const data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx);
void _printPDU(const mac_pdu_t* const pdu);
bool _verifyCRC(const mac_pdu_t* const pdu);

bool MAC_init(mac_addr_t selfAddr, bool is_gateway) {
    self = selfAddr;
}

bool MAC_deinit();


bool MAC_send(const mac_data_t* data, uint8_t lenght) {
    //
}

bool MAC_receive(mac_data_t const * data, uint8_t* length) {

}

void MAC_onReceive(mac_callback_t cb) {
    onReceive = cb;
}

void MAC_onSend(mac_callback_t cb) {
    onSend = cb;
}

// ============== MÈTODES PRIVATS ==============

bool _getPDU(const mac_data_t * const data, uint8_t length, mac_pdu_t * const pdu, mac_addr_t rx) {
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

    return true;
}


// Polinomi per CRC8 (x^8+x^2+1)
#define CRC8_POLY 0x07

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
                crc = (crc << 1) ^ CRC8_POLY;  
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

void _printPDU(const mac_pdu_t* const pdu) {
    // En escriure, tipus com uint16_t (ID) es guarden en little endian
    // Així, si ID és 60505 = 0xEC59 es mostraria com 59EC
    for (uint8_t i = 0; i < (pdu->length+2*MAC_ADDRESS_SIZE+MAC_ID_SIZE); ++i) {
        _PX(((char*)pdu)[i]);
    }
    // CRC
    _PX(pdu->crc);
}

mac_id_t _getRandomID() {
    return (mac_id_t)random(0, MAC_MAX_ID);
}



void setup() {
    Serial.begin(115200);

    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println(esp_reset_reason());

    mac_pdu_t pdu;
    mac_data_t data = "PROVA";

    _getPDU((mac_data_t*)data, strlen((char*)data), &pdu, 0xff);

    mac_crc_t crc = pdu.crc;
    Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu));
    pdu.tx ^= 0x01; // canviar un bit
    Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu));
    pdu.tx ^= 0x01; // canviar un bit
    Serial.printf("Is CRC valid? %d\n", _verifyCRC(&pdu));
}

void loop() {
}
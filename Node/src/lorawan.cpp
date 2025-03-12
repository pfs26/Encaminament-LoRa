#include "lorawan.h"
#include "utils.h"
#include "scheduler.h"

// joinEUI - previous versions of LoRaWAN called this AppEUI
// for development purposes you can use all zeros - see wiki for details
#define RADIOLIB_LORAWAN_JOIN_EUI  0x0000000000000000

// the Device EUI & two keys can be generated on the TTN console 
#define RADIOLIB_LORAWAN_DEV_EUI   0x70B3D57ED006B575
#define RADIOLIB_LORAWAN_APP_KEY   0xA5, 0x08, 0xBF, 0x7B, 0xC0, 0x6C, 0xB4, 0xDF, 0x6D, 0xE0, 0xEB, 0xDF, 0x6C, 0x67, 0x2A, 0x9D
#define RADIOLIB_LORAWAN_NWK_KEY   0x00, 0x95, 0x9C, 0x86, 0x73, 0xF4, 0x09, 0x2A, 0x1B, 0xCE, 0x43, 0xCE, 0xA3, 0xFD, 0xE6, 0x0A

static uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
static uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
static uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
static uint8_t nwkKey[] = { RADIOLIB_LORAWAN_NWK_KEY };

// @todo: potser permetre configurar regió?
static LoRaWANNode node(&radio, &EU868, 0);

static bool isSessionSaved = false;
// guarda comptadors up/downs, etc. utilitzats per evitar atacs repetició
static uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE]; 
// claus generades en fer un JoinRequest
static uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];									

static lora_callback_t onReceive = nullptr;

static lora_data_t receivedBuffer;
static size_t receivedLength = sizeof(receivedBuffer);

uint16_t _reuseSession();
void _saveSession();

bool LW_init() {
    // LoRa_init() MUST be called before LW_init(), as radio initialization is done there
    int16_t state = RADIOLIB_ERR_UNKNOWN;
    _PI("[LW] Initializing...");
    
    node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    uint8_t maxPayloadLength = node.getMaxPayloadLen();
    if(maxPayloadLength > LORA_MAX_SIZE) { // @todo: revisar!
        _PE("[LW] LoRa max payload size (%d) greater than LoRaWAN's max size (%d)!", LORA_MAX_SIZE, maxPayloadLength);
        return false;
    }
    _PI("[LW] Max LoRaWAN payload size: %d", maxPayloadLength);

    state = RADIOLIB_ERR_NETWORK_NOT_JOINED;
    uint8_t failedJoins = 0;
    while (state != RADIOLIB_LORAWAN_NEW_SESSION) {
        state = node.activateOTAA();
        if(state != RADIOLIB_LORAWAN_NEW_SESSION) {
            _PE("[LW] Error activating OTAA (code = %d)", state);
            return false;
        }
        _PI("[LW] Activated OTAA");

        _PI("[LW] Saving nonces");
        memcpy(nonces, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE); 
        
        if (state != RADIOLIB_LORAWAN_NEW_SESSION) {
            _PW("[LW] Join failed (code = %d)", state);
            uint32_t retryInSeconds = min((failedJoins++ + 1UL) * 60UL, 3UL * 60UL);
            _PW("[LW] Retrying in %d seconds. WARNING: this is a blocking delay!", retryInSeconds);
            delay(retryInSeconds*1000);
        } 
    } 

    _saveSession();
    
    _PI("[LW] Joined the network successfully");
    // reset the failed join count
    failedJoins = 0;
    
    // hold off off hitting the airwaves again too soon - an issue in the US
    delay(1000);  
    
    return true;
}

// Desinicialitzar LoRaWAN (desconnectar, i eliminar credencials LoRaWAN)
void LW_deinit() {
    node.clearSession();
    isSessionSaved = false;
}

// Enviar dades a través de LoRaWAN
bool LW_send(const lora_data_t data, size_t length) {   
    int16_t state = _reuseSession();
    if(state != RADIOLIB_ERR_NONE) {
        _PW("[LW] Trying to rejoin");
        LW_init(); // @todo: potser abans deinit
    }

    state = node.sendReceive((uint8_t*)data, length, (uint8_t)1, 
                             receivedBuffer, &receivedLength, true, 
                             (LoRaWANEvent_t*)nullptr, (LoRaWANEvent_t*)nullptr);

    if(state < RADIOLIB_ERR_NONE) {
        _PW("[LW] Error sending data (code = %d)", state);
    }
    else if (state == 0) {
        _PI("[LW] Data sent; none received");
    }
    else {
        _PI("[LW] Data sent; received in window %d; size %d; data: %s", state, receivedLength, receivedBuffer);
        if(onReceive != nullptr) {
            // Programar execució, ja que sinó s'executaria callback sense retornar de "LW_send()"
            // i no tindria massa sentit per la capa superior. Es manté l'estructura que capa inferior notifica a superior
            scheduler_once(onReceive);
        }
    }
}

// Comprovar si hi ha connexió establerta amb xarxa LoRaWAN
bool LW_isConnected() {
    return node.isActivated();
}

// Configurar callback per recepció de dades a través de LW
void LW_onReceive(lora_callback_t cb) {
    onReceive = cb;
}

void _saveSession() {
    memcpy(LWsession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    isSessionSaved = true;
}

uint16_t _reuseSession() {
    if(isSessionSaved) {
        _PE("[LW] Cannot restore unsaved session. Initialize LW again");
        return RADIOLIB_ERR_UNKNOWN;
    }

    int16_t state = RADIOLIB_ERR_UNKNOWN;
    _PI("[LW] Using past nonce and session");
    state = node.setBufferNonces(nonces); 	
    if(state != RADIOLIB_ERR_NONE) {
        _PW("[LW] Could not restore saved LoRaWAN nonce. Initialize LW again (code = %d)", state);
    }													

    state = node.setBufferSession(LWsession); 
    if(state != RADIOLIB_ERR_NONE) {
        _PW("[LW] Could not restore saved LoRaWAN session. Initialize LW again (code = %d)", state);
    }	

    if (state == RADIOLIB_ERR_NONE) {
        _PI("[LW] Restored session, activating");
        state = node.activateOTAA();
        if(state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
            _PW("[LW] Failed to activate restored session. Initialize LW again (code = %d)", state);
        }	
    }
    return state;
}
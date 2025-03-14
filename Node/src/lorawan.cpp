#include "lorawan.h"
#include "utils.h"
#include "scheduler.h"

static uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
static uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
static uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
static uint8_t nwkKey[] = { RADIOLIB_LORAWAN_NWK_KEY };

static LoRaWANNode node(&radio, &EU868, 0);

static bool isSessionSaved = false;
// guarda comptadors up/downs, etc. utilitzats per evitar atacs repetició
static uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE]; 
// claus generades en fer un JoinRequest
static uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];									

static lora_callback_t onReceive = nullptr;

static lora_data_t receivedBuffer;
static size_t receivedLength = sizeof(receivedBuffer);

bool _reuseSession();
void _saveSession();

bool LW_init() {
    int16_t state = RADIOLIB_ERR_UNKNOWN;
    _PI("[LW] Initializing...");
    
    // LoRa_init() MUST be called before LW_init(), as radio initialization is done there
    if(!isLoraInitialized) {
        _PE("[LW] Lora not initialized, call LoRa_init() first");
        return false;
    }

    LoRa_setModeWAN();
    
    node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    node.setADR(false);

    state = RADIOLIB_ERR_NETWORK_NOT_JOINED;
    uint8_t failedJoins = 0;
    while (state != RADIOLIB_LORAWAN_NEW_SESSION) {
        state = node.activateOTAA();
        if (state != RADIOLIB_LORAWAN_NEW_SESSION) {
            _PW("[LW] Join failed (code = %d)", state);
            uint32_t retryInSeconds = min((failedJoins++ + 1UL) * 5, 3UL * 20UL);
            _PW("[LW] Retrying in %d seconds. WARNING: this is a blocking delay!", retryInSeconds);
            delay(retryInSeconds*1000);
            continue;
        } 
        
        _PI("[LW] Activated OTAA. Saving nonces");
        memcpy(nonces, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE); 
    } 

    uint8_t maxPayloadLength = node.getMaxPayloadLen();
    if(maxPayloadLength > LORA_MAX_SIZE) { // @todo: revisar!
        _PE("[LW] LoRa max payload size (%d) greater than LoRaWAN's max size (%d)!", LORA_MAX_SIZE, maxPayloadLength);
        return false;
    }
    _PI("[LW] Max LoRaWAN payload size: %d", maxPayloadLength);

    // Cal guardar sessió després de cada uplink (comptadors canvien!)
    _saveSession();
    
    _PI("[LW] Joined the network successfully");
    // reset the failed join count
    failedJoins = 0;

    // hold off off hitting the airwaves again too soon - an issue in the US
    delay(1000);  

    LoRa_setModeRAW();
    
    return true;
}

// Desinicialitzar LoRaWAN (desconnectar, i eliminar credencials LoRaWAN)
void LW_deinit() {
    node.clearSession();
    isSessionSaved = false;
}

// Enviar dades a través de LoRaWAN
bool LW_send(const lora_data_t data, size_t length, uint8_t port, bool confirmed) {
    LoRa_setModeWAN();
    bool reused = _reuseSession();
    if(!reused) {
        _PW("[LW] Could not restore session");
        // LW_init(); // @todo: potser abans deinit; retornar error i programar amb scheduler
        return false;
    }

    int16_t state;
    state = node.sendReceive((uint8_t*)data, length, (uint8_t)port, 
                             receivedBuffer, &receivedLength, confirmed, 
                             (LoRaWANEvent_t*)nullptr, (LoRaWANEvent_t*)nullptr);

    if(state < RADIOLIB_ERR_NONE) {
        _PW("[LW] Error sending data (code = %d)", state);
        return false;
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

    LoRa_setModeRAW();

    return true;
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
    DUMP_ARRAY(LWsession, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
}


bool _reuseSession() {
    if(!isSessionSaved) {
        _PE("[LW] Cannot restore unsaved session. Call LW_init()");
        return RADIOLIB_ERR_UNKNOWN;
    }

    int16_t state = RADIOLIB_ERR_UNKNOWN;
    _PI("[LW] Using past nonce and session");
    state = node.setBufferNonces(nonces); 	
    if(state != RADIOLIB_ERR_NONE) {
        _PW("[LW] Could not restore saved LoRaWAN nonce. Initialize LW again (code = %d)", state);
        return false;
    }													

    state = node.setBufferSession(LWsession); 
    if(state != RADIOLIB_ERR_NONE) {
        _PW("[LW] Could not restore saved LoRaWAN session. Initialize LW again (code = %d)", state);
        return false;
    }	

    _PI("[LW] Restored session, activating");
    state = node.activateOTAA();
    if(state != RADIOLIB_LORAWAN_SESSION_RESTORED && state != RADIOLIB_ERR_NONE) {
        _PW("[LW] Failed to activate restored session. Initialize LW again (code = %d)", state);
        return false;
    }	
    return true;
}
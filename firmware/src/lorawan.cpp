#include "lorawan.h"
#include "utils.h"
#include "scheduler.h"

#include "Preferences.h"

// Arrays per guardar credencials de xarxa LoRaWAN
static uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
static uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
static uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
static uint8_t nwkKey[] = { RADIOLIB_LORAWAN_NWK_KEY };

// Node LoRaWAN de RadioLib
static LoRaWANNode node(&radio, &EU868, 0);

// Flag per determinar si es pot re-utilitzar una sessió anterior
static bool isSessionSaved = false;
// guarda comptadors up/downs, etc. utilitzats per evitar atacs repetició
// Guardat a memoria RTC per persistir entre deep-sleeps (per si mai s'utilitza deep sleep tenir-ho controlar)
static RTC_DATA_ATTR uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
// claus generades en fer un JoinRequest
// static uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];

// Callback executat quan hi ha una nova recepció
// En LoRaWAN classe A la recepció es fa després d'una transmissió; per coherència
// amb altres capes es manté estructura on capa inferior notifica a capa superior
// a través de callback. Es programarà amb scheduler per poder retornar de LW_send()
static lora_callback_t onReceive = nullptr;

// Estructura per guardar les dades de downlink rebudes
// No és necessari un buffer; per cada TX només hi pot haver un RX
// Responsabilitat de capa superior si no obté dades d'anterior RX
// abans de fer una nova transmissió.
static struct {
    lora_data_t data;
    size_t length;
    uint8_t port = 0;
} downlink_data;

static bool _reuseSession();
static void _saveSession();
static bool _lwActivate();

static Preferences prefs;

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

    if(_lwActivate()) {
        _PI("[LW] Reused session");
    }
    else {
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
            failedJoins = 0;
            uint8_t* nonces = node.getBufferNonces();
            prefs.begin("lorawan", false);
            prefs.putBytes("nonces", nonces, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
            prefs.end();
        }
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
// RadioLib ja gestiona la confirmació (tant d'uplink com de downlink), guardant confirmacions
// de downlinks a "piggy-back" (guardant que s'ha de fer confirmació a següent uplink);
// Així la gestió d'ACK no depèn de nosaltres; simplement hem d'enviar quan així ho volem
bool LW_send(const lora_data_t data, size_t length, uint8_t port, bool confirmed) {
    bool returnState = true; // no retornem directament si false per poder posar mode a RAW en acabar

    LoRa_setModeWAN();
    bool reused = _reuseSession();
    if(!reused) {
        _PW("[LW] Could not restore session");
        returnState = false;
    }

    if (returnState) {
        LoRaWANEvent_t dEvent;
        int16_t state = node.sendReceive((uint8_t*)data, length, (uint8_t)port,
                                        downlink_data.data, &downlink_data.length, confirmed,
                                        (LoRaWANEvent_t*)nullptr, &dEvent);
        _saveSession(); // guardar sessió ja que comptadors han canviat

        if(state < RADIOLIB_ERR_NONE) {
            _PW("[LW] Error sending data (code = %d)", state);
            returnState = false;
        }
        else if (state == 0) {
            _PI("[LW] Data sent; none received");
        }
        else {
            _PI("[LW] Data sent; downlink received");
            _PI("[LW] \tWindow: %d, Power: %d, fPort: %d, Confirmed: %d, Confirming: %d, Size: %d, Data: %s",
                state, dEvent.power, dEvent.fPort, dEvent.confirmed, dEvent.confirming, downlink_data.length, downlink_data.data);

            if(dEvent.fPort == 0) {
                _PW("[LW] Received downlink with port 0; ignoring");
            }
            else {
                downlink_data.port = dEvent.fPort;
                if(onReceive != nullptr) {
                    scheduler_once(onReceive);
                    _PW("[LW] Scheduled downlink reception for higher layer");
                }
            }
        }
    }
    LoRa_setModeRAW();

    return returnState;
}

// Retorna les dades guardades de l'últim downlink
bool LW_receive(lora_data_t data, size_t *length, uint8_t *port) {
    if(downlink_data.port == 0) {
        _PW("[LW] No downlink received");
        return false;
    }
    memcpy(data, downlink_data.data, downlink_data.length);
    *length = downlink_data.length;
    *port = downlink_data.port;

    // fPort no hauria de ser mai 0, ja que és port reservat per mac commands; radiolib ho gestiona i no hauria de notificar-ho
    downlink_data.port = 0;
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

// Guarda la sessió de LoRaWAN actual a array, permetent poder-la reutilitzar
// per establir nova connexió sense handshake d'OTAA.
static void _saveSession() {
    memcpy(LWsession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    isSessionSaved = true;
    _PI("[LW] Saving session:");
}

// Obté la sessió guardada, i l'utilitza per reconnectar-se a la xarxa
static bool _reuseSession() {
    return _lwActivate();
    /*
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
    */
}

static bool _lwActivate() {
    int16_t state = RADIOLIB_ERR_UNKNOWN;
    prefs.begin("lorawan", false);
    if (!prefs.isKey("nonces")) {
        _PI("[LW] No Nonces saved - starting fresh.");
        prefs.end();
        return false;
    }

    // Obtenim nonces de memòria NVS
    uint8_t buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    prefs.getBytes("nonces", &buffer, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    state = node.setBufferNonces(buffer);
    if(state != RADIOLIB_ERR_NONE) {
        _PW("[LW] Could not restore saved LoRaWAN nonce. Should connect to LW again (code = %d)", state);
    }

    // Obtenim sessió de memòria RTC (hauria d'existir, ja que nonce existeix)
    if (state == RADIOLIB_ERR_NONE) {
        state = node.setBufferSession(LWsession);
        if(state != RADIOLIB_ERR_NONE) {
            _PW("[LW] Could not restore saved LoRaWAN session. Should connect to LW again (code = %d)", state);
        }
    }

    // Amb nonces i sessió restaurats, podem activar fàcilment
    if (state == RADIOLIB_ERR_NONE) {
        state = node.activateOTAA();
        if(state != RADIOLIB_LORAWAN_SESSION_RESTORED && state != RADIOLIB_ERR_NONE) {
            _PW("[LW] Failed to activate restored session. Should connect to LW again (code = %d)", state);
        }
    }

    prefs.end();
    return state == RADIOLIB_LORAWAN_SESSION_RESTORED || state == RADIOLIB_ERR_NONE;
}
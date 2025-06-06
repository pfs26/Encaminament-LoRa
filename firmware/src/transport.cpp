#include "transport.h"
#include "scheduler.h"
#include "utils.h"
#include "routing.h"
#include "RingBuffer.h"

static Task* ackTimeoutTask;

static RingBuffer lastIds(TRANSPORT_QUEUE_SIZE);

struct transport_app_handlers {
    transport_callback_t onReceive = nullptr;
    transport_callback_t onSend = nullptr;
    transport_callback_t onSendError = nullptr;
};

static uint8_t initCount = 0;

// Array per guardar els callbacks de les aplicacions de capa superior
// facilitant la gestió de múltiples aplicacions. Veure issue #11
// TRANSPORT_MAX_PORT + 1, ja que hi ha 64 possibles ports (comptant el 0)
static transport_app_handlers appHandlers[TRANSPORT_MAX_PORT+1];

// Estructura per guardar dades d'una transmissió generada per `Transport_Send` amb reconeixament
// Guarda estat transmissió, timeout de retransmissió i informació necessaria per fer els reintents
typedef struct {
    transport_pdu_t pdu;
    uint16_t id;
    node_address_t rx;
    bool isSent = false;
    long ackTimeout = -1;
    uint8_t retries = 0;
    Task* ackTask;
} transport_tx_metadata;

static std::vector<transport_tx_metadata> txQueue;

static transport_pdu_t rxPDU; // PDU per guardar segment rebut
static node_address_t rxAddress = NODE_ADDRESS_NULL; // Adreça de node que ha enviat el segment rebut

static void _onRoutingReceived();
static void _onRoutingSent(uint16_t id);
static void _onRoutingTxError(uint16_t id);
static size_t _buildAck(transport_pdu_t* pdu, node_address_t rx, transport_id_t id);
static void _printSegment(const transport_pdu_t* const pdu);
static void _segmentReceived(transport_port_t port);
static void _segmentSent(transport_port_t port);
static void _segmentSentError(transport_port_t port);
static void _ackReceived(transport_pdu_t* pdu);
static void _checkTxQueueMetadata(void);
static void _resendSegment(transport_tx_metadata* meta);

bool Transport_init(node_address_t selfAddr, bool is_gateway) {
    _PI("[TRANSPORT] Initializing...");

    // Post-increment, així la primera vegada sí que farà inicialització com toca
    if(initCount++ > 0) {
        _PI("[TRANSPORT] Already initialized, ignoring...");
        return true;
    }

    if(!Routing_init(selfAddr, is_gateway)) {
        _PE("[TRANSPORT] Error initializing routing layer");
        return false;
    }

    Routing_onReceive(_onRoutingReceived);
    Routing_onSend(_onRoutingSent);
    Routing_onTxError(_onRoutingTxError);

    // Permet reservar memòria només iniciar. Avantatge que no cal realloc inicialment per cada segment
    // txQueue.reserve(10);

    _PI("[TRANSPORT] Initialized");
    return true;
}

void Transport_deinit(transport_port_t port) {
    // Netejar handlers del port que desconnectem
    appHandlers[port] = transport_app_handlers{};

    // Pre-decrement, així només es desinicialitza quan totes les aplicacions ho hagin fet
    if (--initCount > 0) {
        _PI("[TRANSPORT] Still initialized, removing callbacks for port %d", port);
    } else {
        txQueue.clear();
        Routing_deinit();
        _PI("[TRANSPORT] De-initialized");
    }
}

transport_err_t Transport_send(node_address_t rx, transport_port_t port, const transport_data_t data, size_t length, bool ackRequested) {
    _PI("[TRANSPORT] Preparing to send");

    if(length > TRANSPORT_MAX_DATA_SIZE) {
        _PW("[TRANSPORT] Max length exceeded (%d)", length);
        return TRANSPORT_ERR_MAX_LENGTH;
    }

    if(port > TRANSPORT_MAX_PORT) {
        _PW("[TRANSPORT] Invalid port (%d)", port);
        return TRANSPORT_ERR;
    }

    transport_pdu_t pdu;
    pdu.flags.port = port;
    pdu.flags.ACKRequest = ackRequested;
    pdu.flags.ACKResponse = 0;
    pdu.ID = random(1, (1 << (8 * sizeof(transport_id_t))) - 1);
    pdu.dataLength = length;
    memcpy(pdu.data, data, length);

    _printSegment(&pdu);

    uint16_t segmentID; 
    routing_err_t state = Routing_send(rx, (const uint8_t*) &pdu, length+TRANSPORT_HEADER_SIZE, &segmentID);

    if(state != ROUTING_SUCCESS) {
        _PW("[TRANSPORT] Error sending segment (state: %d)", state);
        return TRANSPORT_ERR;
    }

    // Guardem metadades per poder notificar sobre esdeveniments a capa superior
    transport_tx_metadata pduMeta;
    pduMeta.pdu = pdu;
    pduMeta.id = segmentID;
    pduMeta.rx = rx;
    pduMeta.isSent = false;
    txQueue.push_back(pduMeta);

    _PI("[TRANSPORT] Scheduled to send segment (Segment ID: %d, Frame ID: %d)", pdu.ID, segmentID);

    return TRANSPORT_SUCCESS;
}

node_address_t Transport_receive(transport_port_t* port, transport_data_t* data, size_t* length) {
    *port = rxPDU.flags.port;
    *length = rxPDU.dataLength;
    memcpy(data, rxPDU.data, *length);
    return rxAddress;
}

bool Transport_onEvent(transport_port_t port,
                        transport_callback_t onReceive, 
                        transport_callback_t onSend, 
                        transport_callback_t onSendError) {
    if(port > TRANSPORT_MAX_PORT) {
        _PW("[TRANSPORT] Invalid port (%d)", port);
        return false;
    }

    transport_app_handlers handler = appHandlers[port];

    if(handler.onReceive || handler.onSend || handler.onSendError) {
        _PW("[TRANSPORT] Port %d already in use", port);
        return false;
    }

    appHandlers[port].onReceive = onReceive;
    appHandlers[port].onSend = onSend;
    appHandlers[port].onSendError = onSendError;

    _PI("[TRANSPORT] Registered event(s) for port %d", port);
    return true;
}

// Executat per capa inferior (Routing) quan s'han rebut dades per nosaltres
static void _onRoutingReceived() {
    /*
        1. Enviar ACK si demana ACK
        2. Guardar ID a buffer de rebuts

        3. Verificar si és ACK per nosaltres
        4. Validar ACK
    */
    _PI("[TRANSPORT] Received segment");

    transport_pdu_t pdu;
    size_t RoutingLength;
    rxAddress = Routing_receive((routing_data_t*)&pdu, &RoutingLength);

    // La mida ha de ser com a mínim la del header, si no no és vàlid
    if (RoutingLength < TRANSPORT_HEADER_SIZE) {
        _PW("[TRANSPORT] Segment too short (%d)", RoutingLength);
        return;
    }

    _printSegment(&pdu);

    // Si sol·licita ACK, enviem
    if (pdu.flags.ACKRequest) { 
        _PI("[TRANSPORT] ACK requested for segment %d", pdu.ID);
        transport_pdu_t ack;
        size_t length = _buildAck(&ack, rxAddress, pdu.ID);
        routing_err_t state = Routing_send(rxAddress, (const uint8_t*) &ack, length);
    }
    else {
        _PI("[TRANSPORT] ACK not requested for segment %d", pdu.ID);
    }

    // Mirem si és ACK
    if (pdu.flags.ACKResponse) {
        _ackReceived(&pdu);
        // No retornem; podria arribar a permetre rebre ACK i segment a la vegada 
        return;
    }
    
    // Si no s'havia rebut abans, guardem que s'havia rebut, i avisem capa superior 
    if(!lastIds.contains(pdu.ID)) {
        _PI("[TRANSPORT] New segment received (ID: %d)", pdu.ID);
        lastIds.enqueue(pdu.ID);
        // Guardem dades rebudes perquè sigui accessible a capa superior
        memcpy(&rxPDU, &pdu, sizeof(transport_pdu_t));
        _segmentReceived(pdu.flags.port); // @todo: potser scheduler -> potser no, ja que no assegurem que mentre es no es produeix no es rebi una altra cosa
    } else {
        _PI("[TRANSPORT] Segment already received. Sender didn't receive ACK. ACK sent again. Ignoring...");
    }
}

static void _ackReceived(transport_pdu_t* pdu) {
    // Busquem ID d'ACK a txQueue, i si el trobem, eliminem de la llista i generem event de segment enviat
    int index = 0;
    for(transport_tx_metadata meta : txQueue) {
        if(meta.pdu.ID == pdu->ID) {
            scheduler_stop(meta.ackTask); // aturem tout ack
            txQueue.erase(txQueue.begin() + index); // eliminem registre
            _PI("[TRANSPORT] ACK received for segment %d", meta.pdu.ID);
            _segmentSent(meta.pdu.flags.port); // utilitzar meta i no PDU ja que meta conté port correcte. ACK s'envien per port 0
            return;
        }
        index++;
    }
    _PE("[TRANSPORT] ACK received for unknown segment %d. ACK received after TOUT, check TOUT settings", pdu->ID);
}

static void _onRoutingSent(uint16_t id) {
    for(size_t i = 0; i < txQueue.size(); i++) {
        // Per referència (com si fos un punter) per modificar-ho directament a cua
        transport_tx_metadata& meta = txQueue[i];
        if(meta.id == id) {
            // Si és UDP, notifiquem directament capa superior de TX "completada" (només sabem que s'ha pogut enviar a següent node, no final)
            if(!meta.pdu.flags.ACKRequest) {
                _PI("[TRANSPORT] TX done for UDP segment %d (frame %d)", meta.pdu.ID, id);
                txQueue.erase(txQueue.begin() + i);
                _segmentSent(meta.pdu.flags.port); // Notificar a capa superior de fi TX
                return;
            }
            // Si és TCP, esperem ACK 
            long toutDuration_ms = TRANSPORT_RETRY_DELAY*(1 << meta.retries);
            // TOUT amb marge per evitar que poca precisió de millis() afecti (o scheduler, etc.)
            long toutInstant = millis() + toutDuration_ms - 5; // 5ms de marge
            meta.isSent = true;
            meta.ackTimeout = toutInstant;
            meta.retries++;
            meta.ackTask = scheduler_once(_checkTxQueueMetadata, toutDuration_ms);
            _PI("[TRANSPORT] Frame %d sent. Waiting %d ms for ACK. Running at %dms", id, toutDuration_ms, toutInstant);
            return;
        }
    }
    _PE("[TRANSPORT] TX done for frame %d. Was not found on transport's send list. This is right if you are a gateway", id);
}

static void _onRoutingTxError(uint16_t id) {
    // Un txError pot venir únicament de MAC (propagat a través de routing)
    // pot significar que no ha rebut ACK de next hop, i ha esgotat reintents de MAC
    // Com a tal, no s'ha rebut ACK i per tant no s'ha executat esdeveniment de MAC onSend (i propagat per routing)

    // Una opció és reenviar automàticament de nou; una altra és aplicar TOUT com si no es rebés ACK
    // amb la possibilitat que si hi havia congestió o algun problema s'hagi corregit
    for(size_t i = 0; i < txQueue.size(); i++) {
        // Per referència (com si fos un punter) per modificar-ho directament a cua
        transport_tx_metadata& meta = txQueue[i];
        if(meta.id == id) {
            // Si és UDP, notifiquem directament capa superior de TX fallida
            if(!meta.pdu.flags.ACKRequest) {
                _PW("[TRANSPORT] TX Error for UDP segment %d (frame %d)", meta.pdu.ID, id);
                txQueue.erase(txQueue.begin() + i);
                _segmentSentError(meta.pdu.flags.port); // Notificar a capa superior que no s'ha pogut enviar
                return;
            }
            // Si s'han esgotat intents, no té sentit esperar TOUT (txerror implica que no s'ha pogut ni enviar, tampoc rebrem ack)
            if(meta.retries >= TRANSPORT_MAX_RETRIES) {
                _PW("[TRANSPORT] TX Error for TCP segment %d (frame %d). Max retries reached", meta.pdu.ID, id);
                txQueue.erase(txQueue.begin() + i);
                _segmentSentError(meta.pdu.flags.port); // Notificar a capa superior que no s'ha pogut enviar
                return;
            }
            // En altres casos (TCP i intents pendents), generem un TOUT com si esperessim rebre ACK

            long toutDuration_ms = TRANSPORT_RETRY_DELAY*(1 << meta.retries);
            // TOUT amb marge per evitar que poca precisió de millis() afecti (o scheduler, etc.)
            long toutInstant = millis() + toutDuration_ms - 5; // 5ms de marge
            meta.isSent = true; // cal establir-ho a `true` per poder verificar la fi d'ACK (només comprova els que s'han enviat)
            meta.ackTimeout = toutInstant;
            meta.retries++;
            meta.ackTask = scheduler_once(_checkTxQueueMetadata, toutDuration_ms);
            _PW("[TRANSPORT] TX Error for frame %d. Applying TOUT ACK delay (%d ms) to see if network corrects itself. Running at %dms", id, toutDuration_ms, toutInstant);
            return;
        }
    }
    _PE("[TRANSPORT] TX Error for frame %d. Was not found on transport's send list. This is right if you are a gateway", id);
    // _PW("[TRANSPORT] TX Error for frame %d. ACK was not expected, transmission will not succeed", id);
}

/* Executat per callback programar amb scheduler_once quan es produeix un ack timeout
 de dades enviades (i guardades a txQueue)
 Es podria donar el cas que, en una execució de callback, múltiples TX hagin tingut timeout
 no és problema ja que per cada tx es programa un callback; així, després n'hi haurà un altre
 Per l'ordre de retransmissió, s'itera de principi a fi, i al principi hi ha els que primer s'han
 enviat; així, també es manté l'ordre */
static void _checkTxQueueMetadata(void) {
    for(size_t pos = 0; pos < txQueue.size(); pos++) {
        // Per referència (com si fos un punter) per modificar-ho directament a cua
        transport_tx_metadata& meta = txQueue[pos];
        if(meta.ackTimeout != -1 && meta.isSent && millis() >= meta.ackTimeout) {
            if(meta.retries > TRANSPORT_MAX_RETRIES) {
                _PW("[TRANSPORT] Max retries for segment %d reached", meta.id);
                txQueue.erase(txQueue.begin() + pos);
                _segmentSentError(meta.pdu.flags.port); // Notificar a capa superior que no s'ha pogut enviar
                return;
            }
            _PI("[TRANSPORT] ACK timeout for segment %d. Retrying... (retry %d)", meta.id, meta.retries);
            _resendSegment(&txQueue[pos]);
            return;
        }
    }
    _PE("[TRANSPORT] ACK timeout callback without any pending segment. This should not happen. Check guard ms at TOUT");
}

static void _resendSegment(transport_tx_metadata* meta) {
    meta->isSent = false;
    meta->ackTimeout = -1;
    uint16_t segmentID;
    routing_err_t state = Routing_send(meta->rx, (const uint8_t*) &meta->pdu, meta->pdu.dataLength+TRANSPORT_HEADER_SIZE, &segmentID);

    if(state != ROUTING_SUCCESS) {
        _PW("[TRANSPORT] Error re-sending segment (state: %d)", state);
        return;
    }

    _PI("[TRANSPORT] Scheduled to resend segment (ID: %d -> %d) (%d retries)", meta->id, segmentID, meta->retries);

    // Actualitzar a nou id (ja que és nova transmissió)
    meta->id = segmentID;
}
    
static size_t _buildAck(transport_pdu_t* pdu, node_address_t rx, transport_id_t id) {
    pdu->flags.ACKResponse = 1;
    pdu->flags.ACKRequest = 0;
    // pdu->flags.port = 0; // @todo: mirar si utilitzar port 0 per acks?
    pdu->ID = id;
    pdu->dataLength = 0;

    return TRANSPORT_HEADER_SIZE;
}

// Notifica recepció de segment a la capa aplicació amb el port corresponent
static void _segmentReceived(transport_port_t port) {
    if(appHandlers[port].onReceive != nullptr) {
        appHandlers[port].onReceive();
    }
}

// Notifica a capa aplicació amb el port corresponent que s'ha enviat un segment
// No s'indica quin segment s'ha enviat; la capa d'aplicació no hauria de realitzar
// més d'una transmissió seguida
static void _segmentSent(transport_port_t port) {
    if(appHandlers[port].onSend != nullptr) {
        appHandlers[port].onSend();
    }
}

static void _segmentSentError(transport_port_t port) {
    if(appHandlers[port].onSendError != nullptr) {
        appHandlers[port].onSendError();
    }
}

#if LOG_LEVEL <= LOG_LEVEL_INFO
void _printSegment(const transport_pdu_t* const pdu) {
    // Mostra info de PDU en format maco.
    _PI("[TRANSPORT] SEGMENT: ID=%d PORT=%d ACKreq=%d ACKResp=%d D-LEN=%d DATA=%.*s", 
        pdu->ID, pdu->flags.port, pdu->flags.ACKRequest, pdu->flags.ACKResponse, pdu->dataLength, pdu->dataLength, pdu->data);
}
#else
void _printSegment(const transport_pdu_t* const pdu) {}
#endif
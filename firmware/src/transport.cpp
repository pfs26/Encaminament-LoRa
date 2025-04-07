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
    size_t dataLength;
    bool isSent = false;
    long ackTimeout = -1;
    uint8_t retries = 0;
    Task* ackTask;
} transport_tx_metadata;

static std::vector<transport_tx_metadata> txQueue;

static transport_pdu_t rxPDU;

void _onRoutingReceived();
void _onRoutingSent(uint16_t id);
void _onRoutingTxError(uint16_t id);
size_t _buildAck(transport_pdu_t* pdu, node_address_t rx, transport_id_t id);
void _printSegment(const transport_pdu_t* const pdu);
void _segmentReceived(transport_port_t port);
void _segmentSent(transport_port_t port);
void _ackReceived(transport_pdu_t* pdu);
void _checkTxQueueMetadata(void);
void _resendSegment(transport_tx_metadata* meta);

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

    // @todo: veure si ajuda o no
    // txQueue.reserve(10);

    _PI("[TRANSPORT] Initialized");
    return true;
}

void Transport_deinit(transport_port_t port) {
    // Pre-decrement, així només es desinicialitza quan totes les aplicacions ho hagin fet
    if(--initCount > 0) {
        _PI("[TRANSPORT] Still initialized, removing callbacks for port %d", port);
        appHandlers[port].onReceive = appHandlers[port].onSend = nullptr;
        return;
    }

    // La resta d'apphandlers ja seran nullptr, ja que ho farà a IF anterior
    appHandlers[port].onReceive = appHandlers[port].onSend = nullptr;
    txQueue.clear();
    Routing_deinit();
    _PI("[TRANSPORT] De-initialized");
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

    // Només ens interessa guardar info si demana ACK
    if (ackRequested) {
        transport_tx_metadata pduMeta;
        pduMeta.pdu = pdu;
        pduMeta.id = segmentID;
        pduMeta.rx = rx;
        pduMeta.dataLength = length;
        pduMeta.isSent = false;
        
        // Guardem a llista
        txQueue.push_back(pduMeta);
    }
    
    return TRANSPORT_SUCCESS;
}

node_address_t Transport_receive(transport_port_t* port, transport_data_t* data, size_t* length) {
    *port = rxPDU.flags.port;
    *length = rxPDU.dataLength;
    memcpy(data, rxPDU.data, *length);
    // @todo: no implementat; potser no cal ja que és tasca de capa inferior
    return NODE_ADDRESS_NULL;
}

bool Transport_onEvent(transport_port_t port, transport_callback_t onReceive, transport_callback_t onSend) {
    if(port > TRANSPORT_MAX_PORT) {
        _PW("[TRANSPORT] Invalid port (%d)", port);
        return false;
    }

    // AND, ja que potser una aplicació només vol registrar un esdeveniment
    if(appHandlers[port].onReceive != nullptr || appHandlers[port].onSend != nullptr) {
        _PW("[TRANSPORT] Port %d already in use", port);
        return false;
    }

    appHandlers[port].onReceive = onReceive;
    appHandlers[port].onSend = onSend;

    _PI("[TRANSPORT] Registered event(s) for port %d", port);
    return true;
}

// Executat per capa inferior (Routing) quan s'han rebut dades per nosaltres
void _onRoutingReceived() {
    /*
        1. Enviar ACK si demana ACK
        2. Guardar ID a buffer de rebuts

        3. Verificar si és ACK per nosaltres
        4. Validar ACK
    */
    _PI("[TRANSPORT] Received segment");

    transport_pdu_t pdu;
    size_t RoutingLength;
    node_address_t rx = Routing_receive((routing_data_t*)&pdu, &RoutingLength);
    _printSegment(&pdu);

    // Si sol·licita ACK, enviem
    if (pdu.flags.ACKRequest) { 
        _PI("[TRANSPORT] ACK requested for segment %d", pdu.ID);
        transport_pdu_t ack;
        size_t length = _buildAck(&ack, rx, pdu.ID);
        routing_err_t state = Routing_send(rx, (const uint8_t*) &ack, length);
    }
    else {
        _PI("[TRANSPORT] ACK not requested for segment %d", pdu.ID);
    }

    // Mirem si és ACK
    if (pdu.flags.ACKResponse) {
        _ackReceived(&pdu);
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

void _ackReceived(transport_pdu_t* pdu) {
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

void _onRoutingSent(uint16_t id) {
    for(size_t i = 0; i < txQueue.size(); i++) {
        // Per referència (com si fos un punter) per modificar-ho directament a cua
        transport_tx_metadata& meta = txQueue[i];
        if(meta.id == id) {
            long ackTimeout_ms = millis() + TRANSPORT_RETRY_DELAY*1000;
            meta.isSent = true;
            meta.ackTimeout = ackTimeout_ms*(1 << meta.retries);
            meta.retries++;
            meta.ackTask = scheduler_once(_checkTxQueueMetadata, ackTimeout_ms);
            return;
        }
    }
    // Si no es troba és perquè no s'esperava ACK (UDP)
    _PI("[TRANSPORT] Frame with ID %d sent without expecting ACK", id);
}

void _onRoutingTxError(uint16_t id) {
    // Un txError pot venir únicament de MAC (propagat a través de routing)
    // pot significar que no ha rebut ACK de next hop, i ha esgotat reintents de MAC
    // Com a tal, no s'ha rebut ACK i per tant no s'ha executat esdeveniment de MAC onSend (i propagat per routing)

    // Una opció és reenviar automàticament de nou; una altra és aplicar TOUT com si no es rebés ACK
    // amb la possibilitat que si hi havia congestió o algun problema s'hagi corregit
    for(size_t i = 0; i < txQueue.size(); i++) {
        // Per referència (com si fos un punter) per modificar-ho directament a cua
        transport_tx_metadata& meta = txQueue[i];
        if(meta.id == id) {
            long toutDuration_ms = TRANSPORT_RETRY_DELAY*(1 << meta.retries);
            // TOUT amb marge per evitar que poca precisió de millis() afecti (o scheduler, etc.)
            long toutInstant = millis() + toutDuration_ms - 5; // 5ms de marge
            meta.isSent = true;
            meta.ackTimeout = toutInstant;
            meta.retries++;
            meta.ackTask = scheduler_once(_checkTxQueueMetadata, toutDuration_ms);
            _PW("[TRANSPORT] TX Error for frame %d. Applying TOUT ACK delay (%d ms) to see if network corrects itself. Running at %dms", id, toutDuration_ms, toutInstant);
            return;
        }
    }
    _PW("[TRANSPORT] TX Error for frame %d. ACK was not expected, transmission will not succeed", id);

}

/* Executat per callback programar amb scheduler_once quan es produeix un ack timeout
 de dades enviades (i guardades a txQueue)
 Es podria donar el cas que, en una execució de callback, múltiples TX hagin tingut timeout
 no és problema ja que per cada tx es programa un callback; així, després n'hi haurà un altre
 Per l'ordre de retransmissió, s'itera de principi a fi, i al principi hi ha els que primer s'han
 enviat; així, també es manté l'ordre */
void _checkTxQueueMetadata(void) {
    for(size_t pos = 0; pos < txQueue.size(); pos++) {
        // Per referència (com si fos un punter) per modificar-ho directament a cua
        transport_tx_metadata& meta = txQueue[pos];
        if(meta.ackTimeout != -1 && meta.isSent && millis() >= meta.ackTimeout) {
            if(meta.retries > TRANSPORT_MAX_RETRIES) {
                _PW("[TRANSPORT] Max retries for segment %d reached", meta.id);
                txQueue.erase(txQueue.begin() + pos);
                // @todo: potser caldria notificar a capa superior que no s'ha pogut enviar?
                return;
            }
            _PI("[TRANSPORT] ACK timeout for segment %d. Retrying... (retry %d)", meta.id, meta.retries);
            _resendSegment(&txQueue[pos]); // Pass the address of the actual element in the vector
            return;
        }
    }
    _PE("[TRANSPORT] ACK timeout callback without any pending segment. This should not happen. Check guard ms at TOUT");
}

void _resendSegment(transport_tx_metadata* meta) {
    meta->isSent = false;
    meta->ackTimeout = -1;
    uint16_t segmentID;
    routing_err_t state = Routing_send(meta->rx, (const uint8_t*) &meta->pdu, meta->dataLength+TRANSPORT_HEADER_SIZE, &segmentID);

    if(state != ROUTING_SUCCESS) {
        _PW("[TRANSPORT] Error re-sending segment (state: %d)", state);
        return;
    }

    _PI("[TRANSPORT] Scheduled to resend segment (ID: %d -> %d) (%d retries)", meta->id, segmentID, meta->retries);

    // Actualitzar a nou id (ja que és nova transmissió)
    meta->id = segmentID;
}
    
size_t _buildAck(transport_pdu_t* pdu, node_address_t rx, transport_id_t id) {
    pdu->flags.ACKResponse = 1;
    pdu->flags.ACKRequest = 0;
    // pdu->flags.port = 0; // @todo: mirar si utilitzar port 0 per acks?
    pdu->ID = id;
    pdu->dataLength = 0;

    return TRANSPORT_HEADER_SIZE;
}


#if LOG_LEVEL <= LOG_LEVEL_INFO
void _printSegment(const transport_pdu_t* const pdu) {
    // Mostra info de PDU en format maco.
    Serial.printf("===== SEGMENT =====\nID\t%d\nPORT\t%d\nACKReq\t%d\nACKResp\t%d\nD-LEN\t%d\nDATA\t%.*s\nD-HEX\t", 
    pdu->ID, pdu->flags.port, pdu->flags.ACKRequest, pdu->flags.ACKResponse, pdu->dataLength, pdu->dataLength, pdu->data);
    for (int i = 0; i < pdu->dataLength; i++) 
        Serial.printf("%02X ", pdu->data[i]); 
    Serial.print("\n===================\n");
}
#else
void _printSegment(const transport_pdu_t* const pdu) {}
#endif

void _segmentReceived(transport_port_t port) {
    if(appHandlers[port].onReceive != nullptr) {
        appHandlers[port].onReceive();
    }
}

// Notifica a capa aplicació amb el port corresponent que s'ha enviat un segment
// No s'indica quin segment s'ha enviat; la capa d'aplicació no hauria de realitzar
// més d'una transmissió seguida
void _segmentSent(transport_port_t port) {
    if(appHandlers[port].onSend != nullptr) {
        appHandlers[port].onSend();
    }
}

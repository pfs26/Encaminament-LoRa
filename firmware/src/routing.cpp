#include "routing.h"
#include "utils.h"
#include "scheduler.h"

static node_address_t self;
static bool isGateway;

static routing_pdu_t txPDU, rxPDU;

static routing_rx_callback_t onPacketReceived = nullptr;
static routing_tx_callback_t onPacketSent = nullptr;
static routing_tx_callback_t onTxError = nullptr;

static std::vector<mac_id_t> higherLayerPackets;

void _printPacket(const routing_pdu_t* const pdu);
void _onMacReceived(void);
void _onMacSend(uint16_t);
void _onMacTxFailed(uint16_t);
void _onWANReceived(void);
void _processReceivedPacket(size_t length);
void _packetReceived();
void _packetSent(uint16_t id);
void _packetTxFailed(uint16_t id);
routing_err_t _sendThroughLoRaWAN(routing_pdu_t* pdu, uint16_t* id=nullptr);
void _WANPacketSent();
void _WANPacketTxFailed();

bool Routing_init(node_address_t selfAddr, bool is_gateway) {
    // Inicialitzar capa MAC
    // Inicialitzar taula de rutes
    // Configurar callbacks
    if (!IS_ADDRESS_VALID(selfAddr)) {
        _PE("[ROUTING] Invalid address (0x%02X)", selfAddr);
        return false;
    }
    
    self = selfAddr;
    isGateway = is_gateway;
    if(!MAC_init(selfAddr)) {
        _PE("[ROUTING] Error initializing MAC");
        return false;
    }

    if(isGateway && !LW_init()) {
        _PE("[ROUTING] Error initializing LoRaWAN");
        return false;
    }

    if(!RoutingTable_init()) {
        _PE("[ROUTING] Error initializing routing table");
        return false;
    }

    // Potser reservar memòria per un número N de paquets, amb possibilitat d'augmentar dinàmicament
    // higherLayerPackets.reserve(10);

    MAC_onReceive(_onMacReceived);
    MAC_onSend(_onMacSend);
    MAC_onTxFailed(_onMacTxFailed);

    LW_onReceive(_onWANReceived);

    _PI("[ROUTING] Initialized");
    return true;
}

void Routing_deinit() {
    onPacketReceived = nullptr;
    onPacketSent = onTxError = nullptr;
    higherLayerPackets.clear();
    RoutingTable_deinit();
    LW_deinit();
    MAC_deinit();
    _PI("[ROUTING] Deinitialized");
}

routing_err_t Routing_send(node_address_t dst, const routing_data_t data, size_t length, uint16_t* id) {
    if(length > ROUTING_MAX_DATA_SIZE) {
        _PW("[ROUTING] Data too long (%d)", length);
        return ROUTING_ERR_MAX_LENGTH;
    }

    txPDU.src = self;
    txPDU.dst = dst;
    txPDU.ttl = ROUTING_MAX_TTL;
    txPDU.dataLength = length;
    memcpy(txPDU.data, data, length);

    _PI("[ROUTING] Sending packet:");
    _printPacket(&txPDU);

    uint16_t packetID;
    routing_err_t state = ROUTING_ERR;

    // if (dst == self) {
    //     // Si destí és nosaltres mateixos, no cal enviar-ho a capa MAC
    //     // Només cal notificar capa superior
    //     _PI("[ROUTING] Trying to send packet to itself");
    //     scheduler_once(_WANPacketSent);
    //     _packetReceived();
    //     return ROUTING_SUCCESS;
    // }
    
    // Si som gateway i destí és gateway, enviar a través de lorawan
    // Només hauria de passar si capa superior vol enviar alguna cosa?
    if (isGateway && dst == NODE_ADDRESS_GATEWAY) {
        state = _sendThroughLoRaWAN(&txPDU, &packetID);
    }
    else {
        // En altres casos, el paquet és per la mateixa xarxa, i s'envia a través de MAC
        node_address_t nextHop = RoutingTable_getRoute(dst);
        if(nextHop == 0x00) {
            _PW("[ROUTING] No route to 0x%02X", dst);
            return ROUTING_ERR_NO_ROUTE;
        }

        mac_err_t err = MAC_send(nextHop, (uint8_t*)&txPDU, length+ROUTING_HEADERS_SIZE, &packetID);
        state = err == MAC_SUCCESS ? ROUTING_SUCCESS : ROUTING_ERR;

        // Si s'ha pogut enviar, afegir a llista de paquets que cal notificar a capa superior
        // És responabilitat de capa superior guardar-se ID per si vol actuar sobre aquest paquet i event
        // Només guardem si és per MAC; per LW ID serà sempre 0, ja que no hi ha retard entre ordre TX - TX - RX
        // Simplement és per formalitat i mantenir estructura d'esdeveniments a capa superior
        higherLayerPackets.push_back(packetID);
    }

    // Filtrem errors de TX
    if(state == ROUTING_ERR) {
        _PW("[ROUTING] Error sending PDU");
        return ROUTING_ERR;
    }

    if (id)
        *id = packetID;
    
    _PI("[ROUTING] Added packet to send queue (ID: %d)", packetID);
    return ROUTING_SUCCESS;
}

node_address_t Routing_receive(routing_data_t* data, size_t* length) {
    // Executat per capa superior després que s'executi el callback configurat

    *length = rxPDU.dataLength;
    memcpy(data, rxPDU.data, rxPDU.dataLength);

    return rxPDU.src;
}

void Routing_onReceive(routing_rx_callback_t cb) { onPacketReceived = cb; }
void Routing_onSend(routing_tx_callback_t cb) { onPacketSent = cb; }
void Routing_onTxError(routing_tx_callback_t cb) { onTxError = cb; }

routing_err_t _sendThroughLoRaWAN(routing_pdu_t* pdu, uint16_t* id) {
    _PI("[ROUTING] Sending packet to gateway. Forwarding to LoRaWAN");
    bool state = LW_send((uint8_t*)pdu, pdu->dataLength+ROUTING_HEADERS_SIZE);
    if (id) 
        *id = 0; // ID = 0 no es pot generar mai a capa MAC, per tant és un valor segur per identificar que és un paquet LoRaWAN
                 // Tampoc s'hauria de donar el cas que es vulguin fer dues transmissions WAN simultànies   
    // Programem execució d'esdeveniments simulats per mantenir coherència amb capa MAC
    if (state) {
        scheduler_once(_WANPacketSent);
        return ROUTING_SUCCESS;
    }
    else {
        scheduler_once(_WANPacketTxFailed);
        return ROUTING_ERR;
    }
}

void _processReceivedPacket(size_t length) {
    // Si destí de paquet som nosalters, notifiquem capa superior
    if(rxPDU.dst == self) {
        _PI("[ROUTING] Received packet from 0x%02X", rxPDU.src);
        _printPacket(&rxPDU);
        _packetReceived();
        return;
    }

    // si TTL és 1 significa que s'havia enviat amb TTL = 1 i, per tant, que únicament ha de fer un salt
    // Si filtressim per ttl == 0 en faria 2, ja que el primer salt el decrementa a 1
    if(rxPDU.ttl == 1) {
        _PW("[ROUTING] TTL expired");
        return;
    }

    // Actualitzar TTL
    rxPDU.ttl--;

    // Si som gateway i destí és gateway, enviar a través de lorawan
    // Passem a capa superior per si cal gestionar ACK!
    // Cuidado amb els temps! Si es fa un envio a LoRaWAN amb confirmació el temps de TX és, marcat per estàndard, 
    // mínim (2+/-1 segons aleatoris) + tx + rx + processament! Si TOUT ACK transport és inferior a això hi haurà retransmissió!
    // No només això, finestres de recepció separades per 1 segon, fent que temps de TX sigui en qualsevol cas major a aquest!
    if (isGateway && rxPDU.dst == NODE_ADDRESS_GATEWAY) {
        _PI("[ROUTING] Received packet for gateway. Forwarding to LoRaWAN");
        _sendThroughLoRaWAN(&rxPDU);
        _packetReceived();
        return;
    }

    // Si no som gateway, o destí no és gateway, enviar a través de MAC (lora raw)
    // obtenint següent hop de taula de rutes
    node_address_t nextHop = RoutingTable_getRoute(rxPDU.dst);
    if(nextHop == NODE_ADDRESS_NULL) {
        _PW("[ROUTING] No route to 0x%02X", rxPDU.dst);
        return;
    }

    // Reenviem amb MAC_send, i ens despreocupem de si s'acaba enviant o no; MAC ja ho intentarà gestionar tant bé com pugui (reintents, BEB, etc.)
    mac_err_t err = MAC_send(nextHop, (uint8_t*)&rxPDU, length); // MAClength no hauria de canviar si únicament es canvia TTL
    _PI("[ROUTING] Forwarded packet to 0x%02X", rxPDU.dst);
}

void _onWANReceived(void) {
    // Executat quan hi ha una nova recepció de dades a través de LoRaWAN
    // Cal processar-ho i actuar com si fos una recepció de MAC
    size_t length;
    uint8_t port;
    if(!LW_receive((uint8_t*)&rxPDU, &length, &port)) {
        _PW("[ROUTING] No data received from LoRaWAN");
        return;
    }
    _processReceivedPacket(length);
}

void _onMacReceived(void) {
    // Executat quan MAC obté un frame per nosaltres; cal que processem el paquet
    // i veure si és per nosaltres o cal reenviar-lo
    size_t MAClength = 0;
    // Podem copiar directament sobre PDU, ja es farà el mapeig correcte
    node_address_t tx = MAC_receive((mac_data_t*)&rxPDU, &MAClength);
    _processReceivedPacket(MAClength);
}

void _onMacSend(uint16_t id) {
    int pos = 0; // Per guardar quin element s'ha d'eliminar
    for(int value : higherLayerPackets) { // iterar per cada element del vector
        if(value == id) {
            higherLayerPackets.erase(higherLayerPackets.begin() + pos);
            _packetSent(id);
            return;
        }
        pos++;
    }
}

void _onMacTxFailed(uint16_t id) {
    int pos = 0; // Per guardar quin element s'ha d'eliminar
    for(int value : higherLayerPackets) { // iterar per cada element del vector
        if(value == id) {
            higherLayerPackets.erase(higherLayerPackets.begin() + pos);
            _packetTxFailed(id);
            return;
        }
        pos++;
    }
}

void _packetReceived() {
    if(onPacketReceived != nullptr) {
        onPacketReceived();
    }
}

void _packetSent(uint16_t id) {
    if(onPacketSent != nullptr) {
        onPacketSent(id);
    }
}

void _packetTxFailed(uint16_t id) {
    if(onTxError != nullptr) {
        onTxError(id);
    }
}

void _WANPacketSent() {
    // Executat quan s'ha enviat un paquet a través de LoRaWAN
    // Esdeveniments simulats per mantenir coherència amb capa MAC, a través de scheduler
    _packetSent(0);
}

void _WANPacketTxFailed() {
    // Executat quan s'ha intentat enviar un paquet a través de LoRaWAN i ha fallat
    _packetTxFailed(0);
}

#if LOG_LEVEL <= LOG_LEVEL_INFO
void _printPacket(const routing_pdu_t* const pdu) {
    // Mostra info de PDU en format maco.
    // Les dades les intenta mostrar en format ASCII, i només mostra tants chars com marca dataLength (o fins que hi ha '\0', culpa de mostrar-ho com string)
    // Mostra també amb hexadecimal per si hi ha 0x00 entre mig saber si realment és correcte.
    Serial.printf("===== PACKET =====\nSRC\t%d\nDST\t%d\nTTL\t%d\nD-LEN\t%d\nDATA\t%.*s\nD-HEX\t", 
    pdu->src, pdu->dst, pdu->ttl, pdu->dataLength, pdu->dataLength, pdu->data);
    for (int i = 0; i < pdu->dataLength; i++) 
        Serial.printf("%02X ", pdu->data[i]); 
    Serial.print("\n==================\n");
}
#else
void _printPacket(const routing_pdu_t* const pdu) {}
#endif
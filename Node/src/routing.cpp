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
void _onMacSend(mac_id_t);
void _onMacTxFailed(mac_id_t);
void _packetReceived();
void _packetSent();


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
    if(!MAC_init(selfAddr, isGateway)) {
        _PE("[ROUTING] Error initializing MAC");
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

    _PI("[ROUTING] Initialized");
    return true;
}

void Routing_deinit() {
    onPacketReceived = nullptr;
    onPacketSent = onTxError = nullptr;
    higherLayerPackets.clear();
    RoutingTable_deinit();
    MAC_deinit();
    _PI("[ROUTING] Deinitialized");
}

routing_err_t Routing_send(node_address_t dst, const routing_data_t data, size_t length, uint16_t* id) {
    // Comprovar si hi ha ruta a la taula
    // Si no hi ha ruta, enviar paquet de descobriment
    // Si hi ha ruta, enviar paquet amb la ruta
    // Si no es pot enviar, retornar error

    if(length > ROUTING_MAX_DATA_SIZE) {
        _PW("[ROUTING] Data too long (%d)", length);
        return ROUTING_ERR_MAX_LENGTH;
    }

    node_address_t nextHop = RoutingTable_getRoute(dst);
    if(nextHop == 0x00) {
        _PW("[ROUTING] No route to 0x%02X", dst);
        return ROUTING_ERR_NO_ROUTE;
    }

    txPDU.src = self;
    txPDU.dst = dst;
    txPDU.ttl = ROUTING_MAX_TTL;
    txPDU.dataLength = length;
    memcpy(txPDU.data, data, length);

    _PI("[RUTING] Sending packet:");
    _printPacket(&txPDU);
    // @todo: falta filtrar si som gateway, enviar a través de wan. Potser ho ha de fer MAC
    // a partir de l'adreça de destí?
    mac_id_t packetID;
    mac_err_t err = MAC_send(nextHop, (uint8_t*)&txPDU, length+ROUTING_HEADERS_SIZE, &packetID);

    if(err != MAC_SUCCESS) {
        _PW("[ROUTING] Error sending PDU (state: %d)", err);
        return ROUTING_ERR;
    }

    higherLayerPackets.push_back(packetID);
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

void _onMacReceived(void) {
    // Executat quan MAC obté un frame per nosaltres; cal que processem el paquet
    // i veure si és per nosaltres o cal reenviar-lo
    size_t MAClength = 0;
    // Podem copiar directament sobre PDU, ja es farà el mapeig correcte
    node_address_t tx = MAC_receive((mac_data_t*)&rxPDU, &MAClength);

    if(rxPDU.dst == self) {
        _PI("[ROUTING] Received packet from 0x%02X", rxPDU.src);
        _printPacket(&rxPDU);
        _packetReceived();
        return;
    }

    // @todo: Falta filtrar per si som gateway -> cal reenviar-ho a través de MAC cap a gateway!
    //        important que gateway estigui a taula de rutes correctament definit!

    if(rxPDU.ttl == 1) {
        _PW("[ROUTING] TTL expired");
        return;
    }

    node_address_t nextHop = RoutingTable_getRoute(rxPDU.dst);
    if(nextHop == NODE_ADDRESS_NULL) {
        _PW("[ROUTING] No route to 0x%02X", rxPDU.dst);
        return;
    }

    // Actualitzar TTL
    rxPDU.ttl--;

    // Reenviem amb MAC_send, i ens despreocupem de si s'acaba enviant o no; MAC ja ho intentarà gestionar tant bé com pugui (reintents, BEB, etc.)
    mac_err_t err = MAC_send(nextHop, (uint8_t*)&rxPDU, MAClength); // MAClength no hauria de canviar si únicament es canvia TTL
    _PI("[ROUTING] Forwarded packet to 0x%02X", rxPDU.dst);
}

void _onMacSend(mac_id_t id) {
    // Probablement no utilitzat
    if(onPacketSent == nullptr) {
        return;
    }
    int pos = 0; // Per guardar quin element s'ha d'eliminar
    for(int value : higherLayerPackets) { // iterar per cada element del vector
        if(value == id) {
            higherLayerPackets.erase(higherLayerPackets.begin() + pos);
            onPacketSent(id);
            return;
        }
        pos++;
    }
}

void _onMacTxFailed(mac_id_t id) {
    // Probablement no utilitzat; routing només es preocupa d'obtenir següent RX i enviar,
    // MAC és qui gestiona reintents
    if(onTxError == nullptr) {
        return;
    }
    int pos = 0; // Per guardar quin element s'ha d'eliminar
    for(int value : higherLayerPackets) { // iterar per cada element del vector
        if(value == id) {
            higherLayerPackets.erase(higherLayerPackets.begin() + pos);
            onTxError(id);
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



#include "routing.h"
#include "utils.h"
#include <scheduler.h>

static routing_addr_t self;
static bool isGateway;

static routing_pdu_t txPDU, rxPDU;

static routing_callback_t onPacketReceived = nullptr;

void _printPacket(const routing_pdu_t* const pdu);
void _onMacReceived(void);
void _onMacSend(void);
void _onMacTxFailed(void);
void _packetReceived();


bool Routing_init(routing_addr_t selfAddr, bool is_gateway) {
    // Inicialitzar capa MAC
    // Inicialitzar taula de rutes
    // Configurar callbacks

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

    MAC_onReceive(_onMacReceived);
    MAC_onSend(_onMacSend);
    MAC_onTxFailed(_onMacTxFailed);

    _PI("[ROUTING] Initialized");
    return true;
}

void Routing_deinit() {
    MAC_deinit();
    RoutingTable_deinit();
    _PI("[ROUTING] Deinitialized");
}

routing_err_t Routing_send(routing_addr_t dst, const routing_data_t data, size_t length) {
    // Comprovar si hi ha ruta a la taula
    // Si no hi ha ruta, enviar paquet de descobriment
    // Si hi ha ruta, enviar paquet amb la ruta
    // Si no es pot enviar, retornar error

    if(length > ROUTING_MAX_DATA_SIZE) {
        _PW("[ROUTING] Data too long (%d)", length);
        return ROUTING_ERR_MAX_LENGTH;
    }

    routing_addr_t nextHop = RoutingTable_getRoute(dst);
    if(nextHop == 0x00) {
        _PW("[ROUTING] No route to 0x%02X", dst);
        return ROUTING_ERR_NO_ROUTE;
    }

    txPDU.src = self;
    txPDU.dst = dst;
    txPDU.ttl = ROUTING_MAX_TTL;
    txPDU.dataLength = length;
    memcpy(txPDU.data, data, length);

    mac_err_t err = MAC_send(nextHop, (uint8_t*)&txPDU, length+ROUTING_HEADERS_SIZE);

    if(err != MAC_SUCCESS) {
        _PW("[ROUTING] Error sending PDU (state: %d)", err);
        return ROUTING_ERR;
    }

    _PI("[ROUTING] Sent PDU to 0x%02X", dst);
    return ROUTING_SUCCESS;
}

routing_addr_t Routing_receive(routing_data_t* data, size_t* length) {
    // Executat per capa superior després que s'executi el callback configurat

    *length = rxPDU.dataLength;
    memcpy(data, rxPDU.data, rxPDU.dataLength);

    return rxPDU.src;
}

void _onMacReceived(void) {
    // Executat quan MAC obté un frame per nosaltres; cal que processem el paquet
    // i veure si és per nosaltres o cal reenviar-lo
    size_t MAClength = 0;
    // Podem copiar directament sobre PDU, ja es farà el mapeig correcte
    mac_addr_t tx = MAC_receive((mac_data_t*)&rxPDU, &MAClength);

    if(rxPDU.dst == self) {
        _PI("[ROUTING] Received packet from 0x%02X", rxPDU.src);
        _printPacket(&rxPDU);
        _packetReceived();
        // TODO: ACK explícit, no hi haurà encaminament (i per tant ACK implícit)
        return;
    }

    if(rxPDU.ttl == 0) {
        _PW("[ROUTING] TTL expired");
        // TODO: ACK explícit, no hi haurà encaminament (i per tant ACK implícit)
        // Per molt que TTL sigui 0, no fa falta que MAC faci tots els reintents
        return;
    }

    routing_addr_t nextHop = RoutingTable_getRoute(rxPDU.dst);
    if(nextHop == 0x00) {
        _PW("[ROUTING] No route to 0x%02X", rxPDU.dst);
        // TODO: ACK explícit, no hi haurà encaminament (i per tant ACK implícit)
        return;
    }

    // Actualitzar TTL
    rxPDU.ttl--;

    // Reenviem amb MAC_send, i ens despreocupem de si s'acaba enviant o no; MAC ja ho intentarà gestionar tant bé com pugui (reintents, BEB, etc.)
    mac_err_t err = MAC_send(nextHop, (uint8_t*)&rxPDU, MAClength); // MAClength no hauria de canviar si únicament es canvia TTL
    _PI("[ROUTING] Forwarded packet to 0x%02X", rxPDU.dst);
}

void _onMacSend(void) {
    // Probablement no utilitzat
}

void _onMacTxFailed(void) {
    // Probablement no utilitzat; routing només es preocupa d'obtenir següent RX i enviar,
    // MAC és qui gestiona reintents
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


void setup() {
    Serial.begin(115200);
    Routing_init(0x01, false);
    Routing_send(0x02, (uint8_t*)"Hola", 4);
}

void loop() {
    scheduler_run();
}


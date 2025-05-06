/*
    Exemple de comunicació entre dos nodes amb LoRaWAN i routing per gateway.
    Situació:
        - Gateway **real** de LoRaWAN, que utilitza l'adreça 'virtual' NODE_ADDRESS_GATEWAY (0x01)
        - Node amb rol de gateway de la xarxa privada, amb adreça 0x02
        - Node client de la xarxa privada, amb adreça 0x03
    El node client (0x03) vol fer una transmissió a gateway LoRaWAN (0x01), utilitzant node gateway (0x02) com a intermediari.
    El node gateway (0x02) ha de reenviar la trama a LoRaWAN, i el gateway LoRaWAN ha de rebre la trama.

    Es fa una única transmissió des de capa de transport.

    La taula de ruta de client és:
        0x01 -> 0x02
        0x02 -> 0x02
    La taula de ruta de gateway és:
        0x01 -> 0x01
        0x03 -> 0x03
*/

#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"

#define GATEWAY

int count = 0;

void onSend() {
    Serial.println("Segment enviat a LoRaWAN!");
}

void onRcv() {
    Serial.println("Segment rebut");
    transport_data_t data;
    size_t length;
    uint8_t port;
    node_address_t tx = Transport_receive(&port, &data, &length);
    data[length] = '\0'; // Per poder imprimir amb serial
    Serial.printf("\tTX: %d\tData: %s\tLength: %d\tPort: %d\n", tx, data, length, port);
}

void setup() {
    Serial.begin(921600);
    
    Serial.println("=================================");
    Serial.println("  LoRaWAN amb Gateway i routing");
    Serial.println("=================================");


    #ifdef GATEWAY
        node_address_t addr = 0x02;
        bool is_gateway = true;
    #else
        node_address_t addr = 0x03;
        bool is_gateway = false;
    #endif

    if(!Transport_init(addr, is_gateway)) {
        Serial.println("Error inicialitzant transport");
        while(1);
    }

    Transport_onEvent(0x01, onRcv, onSend, nullptr);

    RoutingTable_clear();
    #ifdef GATEWAY
        RoutingTable_addRoute(0x01, 0x01);
        RoutingTable_addRoute(0x03, 0x03);
    #else
        RoutingTable_addRoute(0x01, 0x02);
        RoutingTable_addRoute(0x02, 0x02);
    #endif

    // Si no és gateway, enviem segment amb destí el gateway
    #ifndef GATEWAY
        transport_data_t data = "Hola!";
        transport_err_t state = Transport_send(NODE_ADDRESS_GATEWAY, 0x01, data, 5, true);
        if(state != TRANSPORT_SUCCESS) {
            Serial.printf("Error sending: %d\n", state);
            return;
        }
        Serial.println("Segment scheduled to be sent");
    #endif
}

void loop() {
    scheduler_run();
}

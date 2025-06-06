/*
    Exemple per verificar funcionament d'enviament de LoRaWAN cap a node de la xarxa,
    passant per node gateway intermig.
    Situació:
        Gateway WAN (0x01) -> Node rol gate (0x02) -> Node 2 (0x03)
    
    El node 0x01 (xarxa WAN) programa un downlink cap a 0x02 (node rol gateway) a 
    través de LoRaWAN. Conté un paquet amb capes routing + transport amb destí 0x03. 
    Quan 0x02 fa un uplink, rebrà el downlink amb paquet amb destí 0x03.
    Enviarà paquet cap a 0x03 utilitzant LoRaRAW (mac + routing).

    Paquet exemple: 01030506FFFE04024142 (per enviar a través de TTN)

    Resultat: 
        1. En primer enviament, el node 0x03 rebrà el paquet correctament
        2. Si es repeteix l'enviament a través de TTN, 0x03 ignorarà el paquet, ja que
           capa transport té el mateix identificador de segment
        3. Cal que node 0x02 generi un uplink per tal de poder rebre els downlinks

    El segment que es programa com a downlink ha de tenir el port 0x01, l'utilitzat
    en aquest exemple. Si no, no funcionarà correctament, ja que receptor no tindrà
    cap aplicació en aquest port
*/

#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"

// Si és 1, el node té capacitats LoRaWAN
#define GATEWAY 0

#if GATEWAY
    #define NODE_ADDRESS 0x02
#else
    #define NODE_ADDRESS 0x03
#endif

int count = 0;

void onReceive() {
    transport_port_t port;
    transport_data_t data;
    size_t datalen;
    Transport_receive(&port, &data, &datalen);
    data[datalen] = '\0'; // Null-terminate the string
    Serial.print("Received data on port: ");
    Serial.println(port);
    Serial.printf("Data length: %zu\n", datalen);
    
    if(port == 0x01) {
        Serial.print("Data: ");
        Serial.println((char*)data);
        count++;
        Serial.print("Count: ");
        Serial.println(count);
    }
}

void setup() {
    Serial.begin(921600);
    
    Serial.println("============================");
    Serial.println("Gateway to node routing test");
    Serial.println("============================");

    if(!Transport_init(NODE_ADDRESS, GATEWAY)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    Transport_onEvent(0x01, onReceive, nullptr);

    // Si és el node amb rol de gateway, ha de fer un uplink per poder rebre el 
    // downlink prèviament programat a través de TTN
    #if GATEWAY
        delay(1000);
        Serial.println("Sending data...");
        Transport_send(0x01, 0x01, (uint8_t*)"", 0, false);
    #endif

}

void loop() {
    scheduler_run();
}

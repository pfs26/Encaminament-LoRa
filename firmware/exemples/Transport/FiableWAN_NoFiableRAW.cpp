/*
    TRANSPORT

    Situació:
        0x03            0x02            Gateway (0x01)
        ============    ============    ============
        0x02 -> 0x02    0x03 -> 0x03    (no existeix)
        GW -> 0x02      GW -> GW

    Node 0x03 vol enviar a GW, utilitzant comunicació fiable.
    El node amb capacitats LoRaWAN és 0x02, qui rebrà el segment
    i, en tractar-se com una "extensió" del gateway, el processarà
    tot i no ser-ne el destinatari final.
    És la solució més senzilla per utilitzar classe A a LoRaWAN
    i no dependre d'un altre transmissió per acabar-ne una.

    Després de realitzar la correcta transmissió fiable, 0x03
    enviarà un missatge no fiable a 0x02. També generarà esdeveniamnet
    de fi de transmissió, programant una nova transmissió no fiable.
    Això es repeteix fins a un màxim de 5 transmissions no fiables.
*/


#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"

#if IS_GATEWAY
    #define NODE_ADDRESS 0x02
#else
    #define NODE_ADDRESS 0x03
#endif

void onReceive() {
    transport_port_t port;
    transport_data_t data;
    size_t datalen;
    Transport_receive(&port, &data, &datalen);
    data[datalen] = '\0'; // Null-terminate the string
    Serial.printf("Received data on port: %d\tLenght: %d\tData: %s\n", port, datalen, (char*)data);
}

void onSend() {
    // Executat quan s'ha enviat un missatge i s'ha rebut ACK

    // Si s'envia sense fiabilitat, s'executa quan s'ha pogut enviar al 
    // següent node, que no té perquè ser el final. Únicament
    // propaga notificació de MAC.
    Serial.printf("[%d] Data sent with ACK reception\n", millis());

    // Enviem un missatge a 0x02 sense fiabilitat.
    // Com que també generarà onSend, limitem a 5 enviaments
    static int UDPSent = 0;
    if(UDPSent >= 5) {
        Serial.println("Sent 5 messages, stopping");
        return;
    }

    char* data = "Hello 0x02";
    Transport_send(0x02, 63, (uint8_t*)data, 10, false);
    UDPSent++;
}

void setup() {
    Serial.begin(921600);
    Serial.println("===================");
    Serial.println(" Exemple Transport ");
    Serial.println("===================");
    
    if(!Transport_init(NODE_ADDRESS, IS_GATEWAY)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    // configurem callbacks capa transport per aplicació personalitzada
    Transport_onEvent(63, onReceive, onSend);

    // Configurem taula de rutes de cada node
    RoutingTable_clear();
    #if IS_GATEWAY
        RoutingTable_addRoute(NODE_ADDRESS_GATEWAY, NODE_ADDRESS_GATEWAY); 
        RoutingTable_addRoute(0x03, 0x03); 
    #else
        RoutingTable_addRoute(NODE_ADDRESS_GATEWAY, 0x02); 
        RoutingTable_addRoute(0x02, 0x02); 
    #endif

    // Si no és el gateway, enviem un missatge a gateway FIABLE
    #if !IS_GATEWAY
        char* data = "Hello";
        Transport_send(NODE_ADDRESS_GATEWAY, 63, (uint8_t*)data, 5, true);
    #endif
}

void loop() {
    scheduler_run();
}


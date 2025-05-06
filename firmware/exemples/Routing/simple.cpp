/*
    Exemple per verificar encaminament. Situació:
        1. 2 nodes amb adreces 0x02 i 0x03
        2. El node 0x02 envia un paquet (capa Routing) a 0x04
        3. Taula d'encaminament de 0x02 té ruta cap a 0x04 per 0x03
        4. Taula d'envaminament de 0x03 té ruta cap a 0x04 per 0x04
    
    Resultat esperat:
        1. 0x02 genera paquet cap a 0x04
        2. Obté ruta a 0x04 per 0x03
        3. Envia frame cap a 0x03. Quedarà esperant a rebre ACK, i si no reintentarà fins a 3 vegades.
        4. 0x03 rep frame, consulta paquet interior. 
        5. Obté ruta a 0x04 per 0x04
        6. Envia frame cap a 0x04 (és el destí final, físicament no existeix però i no rebrà mai ACK)
        7. 0x03 no rep mai ACK
        8. 0x03 si rep el mateix frame de 0x02 de nou, només enviarà ACK i no repetirà 5, 6 i 7.

    Important: el dispositiu 0x04 **NO** existeix; únicament és per verificar que 0x03 intenta enviar-ho a aquest
*/

#include "routing.h"
#include "scheduler.h"

// Si descomentat, el node és 0x02, i enviarà el paquet a 0x04
// #define INITIATOR

void onSend() {
    Serial.println("Paquet enviat!");
}

void onRcv() {
    Serial.println("Paquet rebut!");
}

void setup() {
    Serial.begin(921600);
    Serial.println("====================");
    Serial.println("Exemple Encaminament");
    Serial.println("====================");



    #ifdef INITIATOR
    node_address_t addr = 0x02;
    #else
    node_address_t addr = 0x03;
    #endif
    if(!Routing_init(addr, false)) {
        Serial.println("Error inicialitzant Encaminement");
        while(1);
    }

    #ifdef INITIATOR
        RoutingTable_updateRoute(0x04, 0x03);
        Routing_send(0x04, (uint8_t*) "Hola 0x04!", 10);
    #else
        RoutingTable_updateRoute(0x04, 0x04);
    #endif

}

void loop() {
    scheduler_run();
}
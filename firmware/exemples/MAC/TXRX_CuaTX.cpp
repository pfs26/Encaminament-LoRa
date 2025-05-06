/*
    Exemple amb dos nodes: un és transmissor i l'altre receptor.
    El transmissor envia frames cada 5 segons. El retard del tansmissor és bloquejant (fet amb delay())
    El receptor imprimeix les dades rebudes per pantalla, i envia ACK si li toca.

    El transmissor, abans de la primera transmissió, realitza múltiples `MAC_send()`,
    comprovant el funcionament de la cua de transmissió
*/

#include <Arduino.h>
#include "mac.h"
#include "scheduler.h"
#include "utils.h"

// Si descomentat, el dispositiu serà qui realitzarà les transmissions
#define SENDER

void onSend(mac_id_t id) {
    Serial.printf("MAC frame enviat (%d)\n", id);
    delay(5000);
    mac_data_t data = "Hola!";
    mac_id_t id2;
    MAC_send(0x03, data, 5, &id2);
    Serial.printf("Programat frame %d\n", id2);
}

void onErr(mac_id_t id) {
    Serial.printf("Error enviant MAC frame %d\n", id);
    delay(5000);
    mac_data_t data = "Hola!";
    mac_id_t id2;
    MAC_send(0x03, data, 5, &id2);
    Serial.printf("Programat frame %d\n", id2);
}

void onRcv() {
    Serial.println("MAC frame rebut");
    mac_data_t data;
    size_t length;
    node_address_t tx = MAC_receive(&data, &length);
    data[length] = '\0'; // Per poder imprimir amb serial
    Serial.printf("\tTX: %d\n\tData: %s\tLength: %d\n", tx, data, length);
}

void setup() {
    Serial.begin(921600);
    
    Serial.println("========================");
    Serial.println("  TX i RX a capa MAC    ");
    Serial.println("  Test cua transmissió  ");
    Serial.println("========================");


    #ifdef SENDER
        node_address_t addr = 0x02;
    #else
        node_address_t addr = 0x03;
    #endif
    if(!MAC_init(addr)) {
        Serial.println("Error inicialitzant MAC");
        while(1);
    }

    MAC_onSend(onSend);
    MAC_onTxFailed(onErr);
    MAC_onReceive(onRcv);

    // Si és transmissor, programem múltiples enviaments,
    // per comprovar el funcionament de la cua de TX
    #ifdef SENDER
        mac_data_t data = "Hola! (1)";
        mac_id_t id;
        MAC_send(0x03, data, 9, &id);
        Serial.printf("Programat frame %d\n", id);
        mac_data_t data2 = "Hola! (2)";
        MAC_send(0x03, data2, 9, &id);
        Serial.printf("Programat frame %d\n", id);
        mac_data_t data3 = "Hola! (3)";
        MAC_send(0x03, data3, 9, &id);
        Serial.printf("Programat frame %d\n", id);
    #endif
}

void loop() {
    scheduler_run();
}
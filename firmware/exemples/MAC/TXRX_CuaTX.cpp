/*
    TRANSMISSOR I RECEPTOR
    ======================
    El transmissor envia frames cada 5 segons. El retard del tansmissor és bloquejant (fet amb delay())
    El receptor imprimeix les dades rebudes per pantalla, i envia ACK si li toca.

    CUA TX
    ======
    El transmissor, abans de la primera transmissió, realitza múltiples `MAC_send()`,
    comprovant el funcionament de la cua de transmissió.

    Reintents i increment de potència
    =================================
    És interessant observar el comportament si s'atura el node receptor, però no el transmissor.
      - El transmissor enviarà frames, no rebrà ACK, i reintentarà enviar el missatge de nou
      - Per a cada reintent, s'ha d'observar com es modifica la potència de transmissió a través de la traça

    Ajustament de potència en enviar ACK
    ====================================
    De forma manual, també es pot connectar *DURANT* la retransmissió d'un frame el node receptor.
      - El receptor despertarà ràpid, rebrà el frame reintentat, i enviarà ACK.
      - S'hauria de veure com el receptor que envia ACK modifica la potència de transmissió, per 
        adaptar-la a la potència de transmissió que ha utilitzat el transmissor en aquell reintent. 
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
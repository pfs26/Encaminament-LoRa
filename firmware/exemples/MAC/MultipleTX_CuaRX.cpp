/*
    MÚLTIPLES TRANSMISSORS EN UNA XARXA
    ===================================
    - Un node envia frames cada 2,5s (`SENDER1`), l'altre cada 10s.
    - Es reintenta en cas d'error.
    - Els missatges rebuts es mostren per pantalla.

    CUA RECEPCIÓ
    ============
    Es representa també el funcionament de la cua de recepció, on la capa superior (en aquest cas, l'exemple)
    controla quan processar-los. Es processen els missatges després d'haver-ne rebut 5.
*/

#include "mac.h"
#include "scheduler.h"

#define SENDER1

void Send() {
    #ifdef SENDER1
        mac_data_t data = "Hola 0x03!";
        node_address_t rx = 0x03;
    #else
        mac_data_t data = "Hola 0x02!";
        node_address_t rx = 0x02;
    #endif
    // Enviem frame
    while(MAC_send(rx, data, strlen((char*)data)) != mac_err_t::MAC_SUCCESS);
}

void onSend(uint16_t id) {
    Serial.printf("MAC frame enviat (ID: %d)\n", id);
    #ifdef SENDER1
        scheduler_once(Send, 2500);
    #else
        scheduler_once(Send, 10000);
    #endif
}

void onErr(uint16_t id) {
    Serial.printf("Error enviant MAC frame (ID: %d)\n", id);
    #ifdef SENDER1
        scheduler_once(Send, 2500);
    #else
        scheduler_once(Send, 10000);
    #endif
}

void onRcv() {
    // Comptador per guardar el nombre de frames rebuts (i que hi ha a la cua)
    static int received = 0;
    Serial.println("MAC frame rebut");
    received++;
    // Processem únicament quan n'hem rebut 5
    if(received == 5) {
        // Obtenim cada frame de la cua, i mostrem per pantalla
        for(int i = 0; i < 5; i++) {
            mac_data_t data;
            size_t length;
            node_address_t tx = MAC_receive(&data, &length);
            data[length] = '\0';
            Serial.printf("\tTX: %d\n\tData: %s\tLength: %d\n", tx, data, length);
        }
        received = 0;
    }
}



void setup() {
    Serial.begin(921600);
    
    Serial.println("===========================");
    Serial.println("  Multiples TX a capa MAC  ");
    Serial.println("  Test Cua de recepció     ");
    Serial.println("===========================");


    #ifdef SENDER1
        node_address_t addr = 0x02;
    #else
        node_address_t addr = 0x03;
    #endif

    if(!MAC_init(addr)) {
        Serial.println("Error inicialitzant mac");
        while(1);
    }

    MAC_onSend(onSend);
    MAC_onTxFailed(onErr);
    MAC_onReceive(onRcv);

    #ifdef SENDER1
        scheduler_once(Send, 2500);
    #else
        scheduler_once(Send, 10000);
    #endif
}

void loop() {
    scheduler_run();
}
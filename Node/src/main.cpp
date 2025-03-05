#include <Arduino.h>
#include "mac.h"
#include "scheduler.h"
#include "utils.h"

// #define SENDER1

void Send() {
    #ifdef SENDER1
        mac_data_t data = "Hola 0x02!";
        mac_addr_t rx = 0x02;
    #else
        mac_data_t data = "Hola 0x01!";
        mac_addr_t rx = 0x01;
    #endif
    while(MAC_send(rx, data, 10) != MAC_ERR_SUCCESS);
}

void onSend() {
    Serial.println("MAC frame sent");
    #ifdef SENDER1
    scheduler_once(Send, 2500);
    #else
    scheduler_once(Send, 10000);
    #endif
}

void onErr() {
    Serial.println("Error sending mac frame");
    #ifdef SENDER1
    scheduler_once(Send, 2500);
    #else
    scheduler_once(Send, 10000);
    #endif
}

void onRcv() {
    Serial.println("MAC frame received");
    mac_data_t data;
    uint8_t length;
    mac_addr_t tx = MAC_receive(&data, &length);
    Serial.printf("\tTX: %d\n\tData: %s\tLength: %d\n", tx, data, length);
}



void setup() {
    Serial.begin(115200);

    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println(esp_reset_reason());


    #ifdef SENDER1
    mac_addr_t addr = 0x01;
    #else
    mac_addr_t addr = 0x02;
    #endif
    if(!MAC_init(addr, false)) {
        _PL("ERR");
        while(1);
    }

    MAC_onSend(onSend);
    MAC_onTxFailed(onErr);
    MAC_onReceive(onRcv);

    scheduler_once(Send);
}

void loop() {
    scheduler_run();
}


// /*
//     Exemple 
// */


// #include <Arduino.h>
// #include "mac.h"
// #include "scheduler.h"
// #include "utils.h"

// // #define SENDER

// int count = 0;

// void onSend() {
//     Serial.println("MAC frame sent");
//     delay(5000);
//     mac_data_t data = "Hola!";
//     MAC_send(0x02, data, 5);
// }

// void onErr() {
//     Serial.println("Error sending mac frame");
//     delay(5000);
//     mac_data_t data = "Hola!";
//     MAC_send(0x02, data, 5);
// }

// void onRcv() {
//     Serial.println("MAC frame received");
//     mac_data_t data;
//     uint8_t length;
//     mac_addr_t tx = MAC_receive(&data, &length);
//     Serial.printf("\tTX: %d\n\tData: %s\tLength: %d\n", tx, data, length);
// }

// void setup() {
//     Serial.begin(115200);

//     Serial.print(F("[SX1262] Initializing ... "));
//     Serial.print("Model: "); Serial.println(ESP.getChipModel());
//     Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
//     Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
//     Serial.println(esp_reset_reason());


//     #ifdef SENDER
//     mac_addr_t addr = 0x01;
//     #else
//     mac_addr_t addr = 0x02;
//     #endif
//     if(!MAC_init(addr, false)) {
//         _PL("ERR");
//         while(1);
//     }

//     MAC_onSend(onSend);
//     MAC_onTxFailed(onErr);
//     MAC_onReceive(onRcv);

//     #ifdef SENDER
//         mac_data_t data = "Hola!";
//         MAC_send(0x02, data, 5);
//     #endif
// }

// void loop() {
//     scheduler_run();
// }
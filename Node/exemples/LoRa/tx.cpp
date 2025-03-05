#include <Arduino.h>
#include "lora.h"
#include "scheduler.h"
#include "utils.h"

#define SENDER1

void Send() {
    #ifdef SENDER1
        lora_data_t data = "Hola 0x02!";
    #else
        lora_data_t data = "Hola 0x01!";
    #endif
    while(LoRa_send(data, 10) != LORA_SUCCESS);

    #ifdef SENDER1
        scheduler_once(Send, 10000);
    #else
        scheduler_once(Send, 250);
    #endif
}

void onRcv() {
    Serial.println("Data received");
    lora_data_t data;
    uint8_t length;
    LoRa_receive(data, &length);
    data[length] = '\0';
    Serial.printf("\tData: %s\tLength: %d\tSNR: %d\tRSSI: %d\n\n", data, length, LoRa_getLastSNR(), LoRa_getLastRSSI());
}

void setup() {
    Serial.begin(115200);
    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
	Serial.print("\n\nCompiled at " __DATE__ " " __TIME__);

    if(!LoRa_init()) {
        Serial.println("LoRa init failed");
        while(1);
    }

    LoRa_onReceive(onRcv);

    scheduler_once(Send);
}

void loop() {
    scheduler_run();
}
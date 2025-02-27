#include <Arduino.h>
#include "lora.h"
#include "scheduler.h"
#include "utils.h"

// Si node és qui envia primer TX
// #define INITIAL

void Sent(void);
void Rcv(void);

void Sent(void) {
    _PL("Main SENT");
}

void Rcv(void) {
    _PL("MAIN RCV");
    lora_data_t data;
    if(!LoRa_receive(&data)) {
        _PL("MAIN Error rcv ");
    }

    _PP("Received: "); _PP((char*)data.data); _PL();
    #ifdef INITIAL
        strncpy((char*)data.data, "Hola node 1", LORA_MAX_SIZE-1);
    #else
        strncpy((char*)data.data, "Hola node 2", LORA_MAX_SIZE-1);
    #endif
    data.length = 11;
    delay(1000);
    LoRa_send(&data);
    
}

void setup() {
    Serial.begin(115200);

    Serial.print(F("[SX1262] Initializing ... "));
    Serial.print("Model: "); Serial.println(ESP.getChipModel());
    Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
    Serial.println(esp_reset_reason());

    if(!LoRa_init()) {
        _PL("ERR");
        while(1);
    }

    LoRa_onSend(Sent);
    LoRa_onReceive(Rcv);

    #ifdef INITIAL
        lora_data_t data;
        strncpy((char*)data.data, "Hola node 1", LORA_MAX_SIZE-1);
        data.length = 12;
        LoRa_send(&data);
    #endif
}

void loop() {
    scheduler_run();
}



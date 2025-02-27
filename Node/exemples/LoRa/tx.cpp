#include <Arduino.h>
#include "lora.h"
#include "scheduler.h"
#include "utils.h"

// Si node és transmissor
// #define TX

void Sent(void);
void Rcv(void);

int count = 0;

void Sent(void) {
    _PL("Main SENT");
    lora_data_t data;
    sprintf((char*)data.data, "%d", count++);
    data.length = strlen((char*)data.data);
    delay(1000);
    lora_tx_error_t status = LoRa_send(&data);
    while(status==LORA_ERROR_TX_PENDING || status == LORA_ERROR_TX_BUSY)
        status = LoRa_send(&data);
}

void Rcv(void) {
    _PL("MAIN RCV");
    lora_data_t data;
    if(!LoRa_receive(&data)) {
        _PL("MAIN Error rcv ");
    }

    Serial.print(millis()); Serial.print(" Received: "); Serial.println((char*)data.data);
    Serial.print("\tSNR: "); Serial.println(LoRa_getLastSNR());
    Serial.print("\tRSSI: "); Serial.println(LoRa_getLastRSSI());
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

    #ifdef TX
        LoRa_onSend(Sent);
    #else
        LoRa_onReceive(Rcv);
    #endif

    #ifdef TX
        lora_data_t data;
        sprintf((char*)data.data, "%d", count++);
        data.length = strlen((char*)data.data);
        LoRa_send(&data);
    #endif
}

void loop() {
    scheduler_run();
}



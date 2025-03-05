#include <Arduino.h>
#include "lora.h"
#include "scheduler.h"
#include "utils.h"

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

    uint8_t data[LORA_MAX_SIZE] = {0};
    int length = LORA_MAX_SIZE;
    int state = LoRa_send(data, length);
    Serial.printf("1st send. Errors = %d\n", state); // no genera error

    uint8_t data2[LORA_MAX_SIZE+1] = {0};
    uint16_t length2 = LORA_MAX_SIZE+1;
    state = LoRa_send(data2, length2);
    Serial.printf("2nd send. Errors = %d\n", state); // genera error
}

void loop() {
    scheduler_run();
}
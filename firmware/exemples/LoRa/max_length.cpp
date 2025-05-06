/*
    Test LoRaRAW per verificar la mida màxima dels
    missatges, i la generació d'errors
*/

#include "lora.h"
#include "scheduler.h"

void setup() {
    Serial.begin(921600);
    Serial.println("====================");
    Serial.println(" LoRa mida mida max");
    Serial.println("====================");

    if(!LoRa_init() || !LoRaRAW_init()) {
        Serial.println("LoRa init failed");
        while(1);
    }

    uint8_t data[LORA_MAX_SIZE] = {0};
    int length = LORA_MAX_SIZE;
    int state = LoRaRAW_send(data, length);
    Serial.printf("1st send. Errors = %d\n", state); // no genera error

    uint8_t data2[LORA_MAX_SIZE+1] = {0};
    uint16_t length2 = LORA_MAX_SIZE+1;
    state = LoRaRAW_send(data2, length2);
    Serial.printf("2nd send. Errors = %d\n", state); // genera error
}

void loop() {
    scheduler_run();
}
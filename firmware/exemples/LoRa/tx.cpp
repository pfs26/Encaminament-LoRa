/*
    Exemple bàsic LoRa:
        - Dos dispositius, un transmissor i un receptor
        - El transmissor envia un missatge cada 10 segons, 
          programant enviament del següent després d'enviar-ne un
        - El receptor espera missatges i els imprimeix per pantalla, mostrant SNR i RSSI
*/

#include "lora.h"
#include "scheduler.h"

// Si descomentant, el dispositiu és qui enviarà
#define SENDER

void Send() {
    lora_data_t data = "Hola mon!";
    while(LoRaRAW_send(data, 9) != LORA_SUCCESS);

    scheduler_once(Send, 10000);
}

void onRcv() {
    Serial.println("Data received");
    lora_data_t data;
    size_t length;
    LoRaRAW_receive(data, &length);
    data[length] = '\0'; // Acabem amb nul, suposant que les dades enviades són ASCII
    Serial.printf("\tData: %s\tLength: %d\tSNR: %d\tRSSI: %d\n\n", data, length, LoRaRAW_getLastSNR(), LoRaRAW_getLastRSSI());
}

void setup() {
    Serial.begin(115200);
    Serial.println("====================");
    Serial.println(" Exemple TX/RX LoRa");
    Serial.println("====================");

    if(!LoRa_init() || !LoRaRAW_init()) {
        Serial.println("LoRa init failed");
        while(1);
    }

    LoRaRAW_onReceive(onRcv);

    scheduler_once(Send);
}

void loop() {
    scheduler_run();
}
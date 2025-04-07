#include <Arduino.h>
#include "transport.h"
#include "routing_table.h"
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"
#include <stdlib.h> // For rand()

#define GATEWAY 0

#if GATEWAY
    #define NODE_ADDRESS 0x02
#else
    #define NODE_ADDRESS 0x03
#endif

// Únicament per mostrar informació del test
bool RTC_DATA_ATTR firstBoot = true;

// Comptadors de transmissions
int RTC_DATA_ATTR TCPsent = 0; // segments TCP enviats (ACK rebut)
int RTC_DATA_ATTR TCPtries = 0; // segments TCP totals enviats (també els que encara no s'ha rebut ACK)
int RTC_DATA_ATTR UDPsent = 0;  // segments UDP totals enviats


#define ACK_CHANCE 0.25 // Probabilitat d'enviar amb ACK
#define SEND_CHANCE 0.5 // Probabilitat d'enviar un missatge en despertar


void onReceive() {
    transport_port_t port;
    transport_data_t data;
    size_t datalen;
    Transport_receive(&port, &data, &datalen);
    data[datalen] = '\0'; // Null-terminate the string
    Serial.printf("Received data on port: %d\tLenght: %d\tData: %s\n", port, datalen, (char*)data);
}

void onSend() {
    // Executat quan s'ha enviat un missatge i s'ha rebut ACK
    // NO s'executa si s'envia sense esperar ACK
    Serial.println("Data sent with ACK reception");
    TCPsent++;
}

void beforeSleeping() {
    Serial.println("Preparing to sleep...");
    Serial.printf("TCP sent: %d\tTCP tries: %d\tUDP sent: %d\n", TCPsent, TCPtries, UDPsent);
}

void ready() {
    // Randomly decide if the node should transmit
    
    if ((float)esp_random() / UINT32_MAX < SEND_CHANCE) {
        // Define the array of possible nodes to send to
        #if GATEWAY
            node_address_t nodes[] = {0x03, 0x01}; // Example: Gateway sends to node 0x03
        #else
            node_address_t nodes[] = {0x02, 0x01}; // Example: Node sends to gateway 0x02
        #endif

        // Pick a random node from the array
        size_t nodeCount = sizeof(nodes) / sizeof(node_address_t);
        node_address_t targetNode = nodes[esp_random() % nodeCount];

        // Randomly decide if an ACK is expected
        bool ackRequested = ((float)esp_random() / UINT32_MAX < ACK_CHANCE);

        // Send the message
        transport_data_t message = {TCPtries + UDPsent + 1};
        transport_err_t result = Transport_send(targetNode, 63, message, 1, ackRequested);
        if (result == TRANSPORT_SUCCESS) {
            if (ackRequested) {
                TCPtries++;
            } else {
                UDPsent++;
            }
            Serial.printf("Message sent to node 0x%02X with%s ACK\n", targetNode, ackRequested ? "" : "out");
        } else {
            Serial.printf("Failed to send message to node 0x%02X. Error: %d\n", targetNode, result);
        }
    } else {
        Serial.println("No message sent this cycle");
    }
}

void setup() {
    Serial.begin(921600);
    
    if(firstBoot) {
        Serial.println("===========================");
        Serial.print("Model: "); Serial.println(ESP.getChipModel());
        Serial.print("CPU: "); Serial.println(ESP.getCpuFreqMHz());
        Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
        Serial.print("Flash: "); Serial.println(ESP.getFlashChipSize());
        Serial.print("Flash speed: "); Serial.println(ESP.getFlashChipSpeed());
        Serial.print("Flash mode: "); Serial.println(ESP.getFlashChipMode());
        // SHould print the random chances for ACK, TCP and UDP. Also pritn node address and its role
        Serial.println("===========================");
        Serial.println("RANDOM OPERATION SLEEP TEST");
        Serial.println("===========================");
        Serial.printf("Node address: 0x%02X\tGateway: %d\n", NODE_ADDRESS, GATEWAY);
        Serial.printf("ACK chance: %.2f\tSend chance: %.2f\n", ACK_CHANCE, SEND_CHANCE);
        Serial.println("===========================");
        firstBoot = false;
    }
        
    if(!Transport_init(NODE_ADDRESS, GATEWAY)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    // configurem callbacks capa transport per aplicació personalitzada (aleatòria)
    Transport_onEvent(63, onReceive, onSend);
    Sleep_onBeforeSleep(beforeSleeping);
    Sleep_onSync(ready);

    // Configurem nodes a qui notificar missatges de sincronització
    #if GATEWAY
        node_address_t nodes[] = {0x03};
    #else
        node_address_t nodes[] = {};
    #endif
    if(!Sleep_setForwardNodes(nodes, sizeof(nodes)/sizeof(node_address_t))) {
        Serial.println("Sleep set forward nodes failed");
        while(1) delay(1);
    }

    // Després de `sleep_init()` no hi hauria d'haver transmissions, a banda de les que 
    // sleep genera.
    // Només s'haurien d'iniciar transmissions després de rebre SYNC `Sleep_onSync()`
    if(!Sleep_init()) {
        Serial.println("Sleep init failed");
        while(1) delay(1);
    }
}

void loop() {
    scheduler_run();
}

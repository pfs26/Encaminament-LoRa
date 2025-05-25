#include <Arduino.h>
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"

static node_address_t forwardCmdTo = NODE_ADDRESS_NULL; // Nodes a qui reenviar el SYNC

// PDU rebuda
static sleep_pdu_t receivedPDU; 

// Control si està sincronitzat a la xarxa o no
static bool isSync = false;

// Temps entre primer SYNC vàlid, i últim SYNC vàlid (els dos extrem: el 1r si tots els nodes envien a la 1a; 
// el segon si tots fan servir tots els intents, + percentatge de CSMA)
static RTC_DATA_ATTR uint64_t deltaTime = 0;

// Tasca per gestionar timeouts
static Task* timeoutTask = nullptr;

// Callback per obtenir dades
static sleep_callback_t onSync = nullptr;

// Instant en que s'ha rebut el SYNC, en `milisegons`
static unsigned long tempsSync = 0;

typedef enum {
    SLEEP_WAIT_FIRST_SYNC,
    SLEEP_PROPAGATE,
    SLEEP_SLEEP,
    SLEEP_WAIT_SYNC,
} sleep_state_t;

typedef enum {
    SLEEP_INITIATOR_E,
    SLEEP_NON_INITIATOR_E,
    SLEEP_SYNC_E,
    SLEEP_TIMEOUT_E,
    SLEEP_DONE_E,
} sleep_event_t;

static RTC_DATA_ATTR sleep_state_t sleepState = SLEEP_WAIT_FIRST_SYNC;


static void decodePDU();
static void onSyncReceived();
static void goToSleep();
static void syncTimeout();
static void received();
static void sent();
static void sendError();
static long computeDeltaTime();
static void forwardSync();
static void sleep_fsm(sleep_event_t);

bool Sleep_init(void) {
    // REQUEREIX TRANSPORT INICIALITZAT!
    // No s'inicialitza aquí per evitar haver de conèixer @ node.
    // Programa principal és qui hauria d'inicialitzar el protocol

    Transport_onEvent(SLEEP_PORT, received, sent, sendError);

    // Generem esdeveniment a FSM d'inici
    sleep_fsm(SLEEP_IS_INITIATOR ? SLEEP_INITIATOR_E : SLEEP_NON_INITIATOR_E);

    return true;
}

void Sleep_deinit(void) {
    // Deshabilitem callbacks
    onSync = nullptr;
    // Cancel·lem tasques programades
    if(timeoutTask != nullptr) {
        scheduler_stop(timeoutTask);
        timeoutTask = nullptr;
    }
    isSync = false;
    // Deshabilitem transport a port de SLEEP
    Transport_deinit(SLEEP_PORT);
}

bool Sleep_setForwardNode(node_address_t node) {
    forwardCmdTo = node;
    _PI("[SLEEP] Set forward node to %d", node);
    return true; // sempre true, de moment. Si fos més d'un node, canviaria la cosa
}

void Sleep_onSync(sleep_callback_t cb) { onSync = cb; }

static void sleep_fsm(sleep_event_t event) {
    switch(sleepState) {
        case SLEEP_WAIT_FIRST_SYNC:
            switch(event) {
                case SLEEP_INITIATOR_E:
                    sleepState = SLEEP_PROPAGATE;
                    // Si som iniciadors, generem missatge, simulem recepció SYNC per propagar
                    receivedPDU.command = SLEEP_CMD_SYNC;
                    receivedPDU.dataLen = 0;
                    scheduler_once(onSyncReceived);
                    break;
                case SLEEP_NON_INITIATOR_E:
                    // En iniciar, si no som iniciadors no fem res, només calculem el temps de tolerància i quedem a l'espera de SYNC
                    // Es calcula només 1 cop. Serà constant per a tota la vida del node (no hauria de canviar la xarxa, i si canvia
                    // caldria recompilar-ho igualment)
                    computeDeltaTime();
                    _PI("[SLEEP] Non-initiator. Waiting for first sync.");
                    break;
                case SLEEP_SYNC_E:
                    sleepState = SLEEP_PROPAGATE;   
                    // Rebem sync
                    onSyncReceived();
                    break;
            }

            break;

        case SLEEP_PROPAGATE:
            if(event == SLEEP_DONE_E) {
                sleepState = SLEEP_SLEEP;
                goToSleep();
            }
            break;
    
        case SLEEP_SLEEP:
            switch(event) {
                case SLEEP_INITIATOR_E:
                    // Si som iniciadors, propaguem directament
                    sleepState = SLEEP_PROPAGATE;
                    // Si som iniciadors, generem missatge, simulem recepció SYNC per propagar
                    receivedPDU.command = SLEEP_CMD_SYNC;
                    receivedPDU.dataLen = 0;
                    _PI("[SLEEP] Initiator. Making PDU and sending.");
                    scheduler_once(onSyncReceived);
                    break;
                case SLEEP_NON_INITIATOR_E:
                    // Si no som iniciadors, iniciem timer d'espera SYNC
                    sleepState = SLEEP_WAIT_SYNC;   
                    unsigned long timeout_ms = MAX(0, 2*deltaTime + 2*SLEEP_EXTRA_TIME);
                    timeoutTask = scheduler_once(syncTimeout, timeout_ms);
                    _PI("[SLEEP] Non-initiator. Waiting for sync with timeout %lu ms", timeout_ms);
                    break;
            }
            break;

        case SLEEP_WAIT_SYNC:
            switch(event) {
                case SLEEP_SYNC_E:
                    // Si rebem sync, aturem timer i propaguem
                    sleepState = SLEEP_PROPAGATE;
                    if(timeoutTask != nullptr) {
                        scheduler_stop(timeoutTask);
                        timeoutTask = nullptr;
                        _PI("[SLEEP] Cancelled sleep timeout");
                    }
                    onSyncReceived();

                    break;
                case SLEEP_TIMEOUT_E:
                    // Si arriba timeout, fem PDU i enviem a propagar
                    sleepState = SLEEP_PROPAGATE;
                    _PI("[SLEEP] Timeout waiting for sync. Making PDU and sending.");
                    receivedPDU.command = SLEEP_CMD_SYNC;
                    receivedPDU.dataLen = 0;
                    // L'instant de sincronització és 0 si no rebem SYNC
                    tempsSync = 0;
                    forwardSync();
                    break;
            }
            break;
    }
}


static long computeDeltaTime() {
    // N per nodes anteriors, (R+1) pels intents totals de TX
    // ACK_FACTOR per quant de temps de transmissió per rebre ACK es deixa
    // AIRTIME pel temps de transmissió
    // N · (R+1) dona transmissions totals
    // N · (R+1) · TX dona temps TX total; N · (R+1) · ACK · TXACK dona temps espera ACK
    // N · (R+1) · TX + N · (R+1) · ACK · TXACK = N · (R+1) · (TX + ACK · TXACK)
    
    uint64_t max_ack_time_ms = US_TO_MS(LoRaRAW_getTimeOnAir(MAC_PDU_HEADER_SIZE)); // Un ACK de MAC té mida del header
    uint64_t tx_time_ms = US_TO_MS(LoRaRAW_getTimeOnAir(LORA_MAX_SIZE));
    deltaTime = SLEEP_QUANTITAT_DISPOSITIUS * (MAC_MAX_RETRIES + 1) * (tx_time_ms + MAC_ACK_TIMEOUT_FACTOR * max_ack_time_ms);
    deltaTime = deltaTime * (1 + SLEEP_DELTA_EXTRA); // 25% marge per CSMA
    _PI("[SLEEP] Delta time compute: TX time = %llu ms + ACK time = %llu ms (%d ACK factor) for %d nodes up to (%d+1) txs, with %.2f extra", tx_time_ms, max_ack_time_ms, MAC_ACK_TIMEOUT_FACTOR, SLEEP_QUANTITAT_DISPOSITIUS, MAC_MAX_RETRIES, SLEEP_DELTA_EXTRA);
    return deltaTime;
}

// Descodifica una PDU de Sleep i la processa.
// Poca utilitat actualment (únicament ordre SYNC)
// però es manté per si es volen afegir més ordres
static void decodePDU() {
    _PI("[SLEEP] Decoding PDU. Command: %d, data: %s", receivedPDU.command, (char*)receivedPDU.data);
    switch(receivedPDU.command) {
        case SLEEP_CMD_SYNC:
            sleep_fsm(SLEEP_SYNC_E);
            break;
        default:
            _PW("[SLEEP] Unknown command: %d", receivedPDU.command);
            break;
    }
}

// Executat en rebre missatge de sincronització.
// Si tasca de 1r boot establerta, la cancel·la.
// Reenvia missatge de sincronització a nodes de la llista `forwardCmdTo`.
static void onSyncReceived() {
    if(isSync) {
        _PW("[SLEEP] Sync already received");
        return;
    }
    _PI("[SLEEP] Sync received");

    // Si és iniciador, marquem SYNC a 0 perquè no afecti retard inicialització a cicle
    tempsSync = SLEEP_IS_INITIATOR ? 0 : millis();

    // En qualsevol cas el node ja estarà sincronitzat a la xarxa
    isSync = true;

    // Propaguem SYNC
    forwardSync();
}

static void forwardSync() {
    // Verifiquem si hi podem posar les nostres dades a camp dades PDU sense passar-nos de mida màxima
    if (receivedPDU.dataLen + SLEEP_DATASIZE_PER_NODE <= SLEEP_MAX_DATA_SIZE) {
        if (onSync != nullptr) {
            // onSync ha de copiar les dades que li interessi a l'apuntador
            uint8_t* data = receivedPDU.data + receivedPDU.dataLen;
            onSync(data, SLEEP_DATASIZE_PER_NODE);
        }
        else { // Si no s'ha definit callback per obtenir dades, afegim 0x00
            _PE("[SLEEP] No callback set. No data can be added to PDU. 0x00 bytes will be added");
            // Posem 0x00 a les dades de la PDU
            memset(receivedPDU.data + receivedPDU.dataLen, 0x00, SLEEP_DATASIZE_PER_NODE);
        }
        // Incrementem la mida de les dades
        receivedPDU.dataLen += SLEEP_DATASIZE_PER_NODE;
    }
    else { // Si no hi caben, només podem enviar directament // TODO: Seria interessant fragmentació
        _PW("[SLEEP] Data length exceeded. No data will be added to PDU. Forwarding.");
    }

    // Reenviem missatge de sincronització a següent node, amb dades modificades
    if(forwardCmdTo != NODE_ADDRESS_NULL) {
        transport_err_t state = Transport_send(forwardCmdTo, SLEEP_PORT, (uint8_t*)&receivedPDU, receivedPDU.dataLen + SLEEP_HEADER_SIZE, false);
        if(state == TRANSPORT_ERR) {
            _PW("[SLEEP] Error forwarding sync to node %d. Going to sleep.", forwardCmdTo);
            sleep_fsm(SLEEP_DONE_E); // Si hi ha error, no podem fer res més, considerem com a fi
        } else {
            _PI("[SLEEP] Sync forwarded to node %d", forwardCmdTo);
        }
    }
    else { // Si no hi ha adreça definida, dormir en rebre SYNC. No podem fer-hi res.
        _PW("[SLEEP] No forward node set. Sync not forwarded");
        sleep_fsm(SLEEP_DONE_E); 
    }

    // Quan es generi esdeveniment `sent` o `txError`, s'anirà a dormir
}

// Posa el microcontrolador i ràdio en mode de baix consum
// la durada del sleep depen del temps en que s'ha rebut sync, i quan
// el node ha finalitzat la seva tasca
static void goToSleep() {
    uint64_t sleepTime = 0;

    // Posar radio a dormir
    LoRaRAW_sleep();
    Transport_deinit(SLEEP_PORT);

    unsigned long tempsDone = 0;
    // Nomes calculem cicle si no som iniciadors i hem rebut sincronització
    if (!SLEEP_IS_INITIATOR && isSync) {
        // Mida de les dades que espera rebre el node, per calcular temps de transmissió (que ha d'estar despert abans)
        uint8_t expectedRcvSize = SLEEP_QUANTITAT_DISPOSITIUS * SLEEP_DATASIZE_PER_NODE + LORA_MAX_SIZE - SLEEP_MAX_DATA_SIZE;
        long transmitTime = US_TO_MS(LoRaRAW_getTimeOnAir(expectedRcvSize));

        // Obtenim el tempsDone (fi) tant a prop com sigui possible de dormir
        tempsDone = millis();
        // sleep time intenta compensar l'error que el propi micro introdueix amb el clock. No podem predir el clock de l'anterior node: s'ha vist com un dispositiu pot tenir 2000ppm, però un altre en pot tenir 25000ppm
        sleepTime = SLEEP_CYCLE_DURATION - (tempsDone - tempsSync) - transmitTime - deltaTime - SLEEP_EXTRA_TIME + SLEEP_CLOCK_CORRECTION;
        _PI("[SLEEP] Expected reception: %d B, Transmission time: %lu ms", expectedRcvSize, transmitTime);
        _PI("[SLEEP] Sleep breakdown: %llu ms = %llu ms - (%lu ms - %lu ms) - %lu ms - %llu ms - %llu ms + %lld ms", 
            sleepTime, SLEEP_CYCLE_DURATION, tempsDone, tempsSync, transmitTime, deltaTime, SLEEP_EXTRA_TIME, SLEEP_CLOCK_CORRECTION);
    }
    else { // Si som iniciadors o no hem rebut SYNC, dormim el temps restant de cicle
        tempsDone = millis();

        sleepTime = SLEEP_CYCLE_DURATION - tempsDone + SLEEP_CLOCK_CORRECTION;
        _PI("[SLEEP] Sleep breakdown: %llu ms = %llu ms - %lu ms + %llu ms", sleepTime, SLEEP_CYCLE_DURATION, tempsDone, SLEEP_CLOCK_CORRECTION);
    }

    // Assegurem que sleep time no és negatiu (hagi fet overflow en uint)
    sleepTime = MAX((int64_t) sleepTime, 0);

    _PE("[SLEEP-STATS] Sync: %d\tSleep time: %llu ms\tSync time: %lu ms\tDone time: %lu ms\tDelta Time: %llu ms\tClk correction: %llu ms\t",
        isSync, sleepTime, tempsSync, tempsDone, deltaTime, SLEEP_CLOCK_CORRECTION);
    
    // Iniciem deep sleep
    _PW("[SLEEP] Extra %llu ms (%llu)", (millis()-tempsDone), sleepTime-(millis()-tempsDone));
    esp_deep_sleep(MS_TO_US(sleepTime));
}

// Processat en rebre dades d'aplicació SLEEP. Executat com a callback de transport
static void received() {
    // Obtenim missatge de transport
    size_t datalen;
    transport_port_t port;
    Transport_receive(&port, (transport_data_t*)&receivedPDU, &datalen);
    // Assegurem que el port estigui correctament configurat
    if(port != SLEEP_PORT) {
        _PE("[SLEEP] Received data on wrong port: %d. Check transport layer handlers", port);
        return;
    }

    decodePDU();
}

static void syncTimeout() {
    sleep_fsm(SLEEP_TIMEOUT_E);
}

static void sent() {
    _PI("[SLEEP] Forwarded data");
    sleep_fsm(SLEEP_DONE_E);
}

static void sendError() {
    _PI("[SLEEP] Error forwarding data. Going to sleep.");
    sleep_fsm(SLEEP_DONE_E);
}
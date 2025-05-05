#include <Arduino.h>
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"

static void decodePDU();
static void onSyncReceived();
static void goToSleep();
static void syncTimeout();
static void received();
static void sent();
static void sendError();
static long computeDeltaTime();
static void forwardSync();

static node_address_t forwardCmdTo = NODE_ADDRESS_NULL; // Nodes a qui reenviar el SYNC

// PDU rebuda
static sleep_pdu_t receivedPDU; 

// Control si està sincronitzat a la xarxa o no
static RTC_DATA_ATTR bool isFirstSync = true;
static bool isSync = false;

// Temps entre primer SYNC vàlid, i últim SYNC vàlid (els dos extrem: el 1r si tots els nodes envien a la 1a; 
// el segon si tots fan servir tots els intents, + percentatge de CSMA)
static RTC_DATA_ATTR uint64_t deltaTime;

static Task* timeoutTask = nullptr;

static sleep_callback_t onSync = nullptr;
static sleep_callback_t onBeforeSleep = nullptr;

// Instant en que s'ha rebut el SYNC, en `milisegons`
static unsigned long tempsSync = 0;

// Quantitat de nodes que hi ha anteriors a aquest
static RTC_DATA_ATTR uint8_t nodesPrevis = 0;
static RTC_DATA_ATTR uint16_t syncCount = 0;
static RTC_DATA_ATTR uint64_t syncTimeSum = 0;
static RTC_DATA_ATTR uint16_t bootCount = 0;
static RTC_DATA_ATTR uint64_t doneTimeSum = 0;

bool Sleep_init(void) {
    // REQUEREIX TRANSPORT INICIALITZAT!
    // No s'inicialitza aquí per evitar haver de conèixer @ node.
    // Programa principal és qui hauria d'inicialitzar el protocol
    if(!isFirstSync) {
        computeDeltaTime();
        long timeout_ms = 3*deltaTime + SLEEP_CLOCK_CORRECTION + SLEEP_EXTRA_TIME;
        // long timeout_ms = 2*deltaTime + SLEEP_CLOCK_CORRECTION + SLEEP_EXTRA_TIME;
        timeoutTask = scheduler_once(syncTimeout, timeout_ms);

        _PI("[SLEEP] Init. Already synchronized.");
        _PI("        Delta time: %.2f seconds. Nodes before: %d", MS_TO_S(deltaTime), nodesPrevis);
        _PI("        Timeout in %.2f seconds.", MS_TO_S(timeout_ms));
    }
    else {
        _PI("[SLEEP] Init. Not synchronized. Waiting for first sync.");
    }

    Transport_onEvent(SLEEP_PORT, received, sent, sendError);

    // Si som iniciadors, simulem que hem rebut SYNC per enviar i iniciar funcionament normal
    if (SLEEP_IS_INITIATOR) {
        _PI("[SLEEP] Initiator. Making PDU and sending.");
        receivedPDU.command = SLEEP_CMD_SYNC;
        receivedPDU.dataLen = 0;
        onSyncReceived();
    }

    return true;
}

void Sleep_deinit(void) {
    // Deshabilitem callbacks
    onSync = nullptr;
    onBeforeSleep = nullptr;
    // Cancel·lem tasques programades
    if(timeoutTask != nullptr) {
        scheduler_stop(timeoutTask);
        timeoutTask = nullptr;
    }
    isFirstSync = true;
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

static long computeDeltaTime() {
    deltaTime = nodesPrevis * MAC_MAX_RETRIES * MAC_ACK_TIMEOUT_FACTOR * US_TO_MS(LoRaRAW_getTimeOnAir(LORA_MAX_SIZE));
    deltaTime = deltaTime * 1.25; // 25% marge per CSMA
    return deltaTime;
}

// Descodifica una PDU de Sleep i la processa.
// Poca utilitat actualment (únicament ordre SYNC)
// però es manté per si es volen afegir més ordres
static void decodePDU() {
    _PI("[SLEEP] Decoding PDU. Command: %d, data: %s", receivedPDU.command, (char*)receivedPDU.data);
    switch(receivedPDU.command) {
        case SLEEP_CMD_SYNC:
            onSyncReceived();
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

    // Si és primera sincronització, marquem recepció SYNC a instant 0 per tal que no afecti retard inicial al càlcul de cicle de sleep
    // Si és iniciador, també marquem SYNC a 0 perquè no afecti retard inicialització a cicle
    // tempsSync = isFirstSync || SLEEP_IS_INITIATOR ? 0 : millis();
    tempsSync = SLEEP_IS_INITIATOR ? 0 : millis();

    // Aturem tasca de timeout si configurada
    if(timeoutTask != nullptr) {
        scheduler_stop(timeoutTask);
        timeoutTask = nullptr;
        _PI("[SLEEP] Cancelled sleep timeout");
    }

    // Actualizem nombre de nodes previs a aquest
    nodesPrevis = receivedPDU.dataLen / SLEEP_DATASIZE_PER_NODE;
    _PI("[SLEEP] Nodes before self: %d", nodesPrevis);

    computeDeltaTime();

    if (!isFirstSync) {
        syncTimeSum += tempsSync;
        syncCount++;
    }

    // En qualsevol cas el node ja estarà sincronitzat a la xarxa
    isSync = true;
    isFirstSync = false;

    // Enviem SYNC
    forwardSync();
}

static void forwardSync() {
    if (receivedPDU.dataLen + SLEEP_DATASIZE_PER_NODE <= SLEEP_MAX_DATA_SIZE) {
        if (onSync != nullptr) {
            uint8_t* data = receivedPDU.data + receivedPDU.dataLen; // Pointer to the correct position in the buffer
            onSync(data);
            
            // Copiem les dades que afegeix onSync() a les dades de la PDU
            // i incrementem la mida de les dades
            memcpy(receivedPDU.data + receivedPDU.dataLen, data, SLEEP_DATASIZE_PER_NODE);
        }
        else {
            _PE("[SLEEP] No callback set. No data can be added to PDU. 0x00 bytes will be added");
            // Posem 0x00 a les dades de la PDU
            memset(receivedPDU.data + receivedPDU.dataLen, 0x00, SLEEP_DATASIZE_PER_NODE);
        }
        // Incrementem la mida de les dades
        receivedPDU.dataLen += SLEEP_DATASIZE_PER_NODE;
    }
    else {
        _PW("[SLEEP] Data length exceeded. No data will be added to PDU. Forwarding.");
    }

    // Reenviem missatge de sincronització a següent node, amb dades modificades
    if(forwardCmdTo != NODE_ADDRESS_NULL) {
        transport_err_t state = Transport_send(forwardCmdTo, SLEEP_PORT, (uint8_t*)&receivedPDU, receivedPDU.dataLen + SLEEP_HEADER_SIZE, false);
        if(state == TRANSPORT_ERR) {
            _PW("[SLEEP] Error forwarding sync to node %d. Going to sleep.", forwardCmdTo);
            goToSleep();
        } else {
            _PI("[SLEEP] Sync forwarded to node %d", forwardCmdTo);
        }
    }
    else { // Si no hi ha adreça definida, dormir en rebre SYNC. No podem fer-hi res.
        _PW("[SLEEP] No forward node set. Sync not forwarded");
        goToSleep();
    }

    // Quan es generi esdeveniment `sent`, s'anirà a dormir
}

// Posa el microcontrolador i ràdio en mode de baix consum
// la durada del sleep depen del temps en que s'ha rebut sync, i quan
// el node ha finalitzat la seva tasca
static void goToSleep() {
    uint64_t sleepTime = 0;
    unsigned long tempsDone = millis();
    _PI("[SLEEP] Time done: %d ms", tempsDone);
    // Nomes calculem cicle si no som iniciadors i hem rebut sincronització
    if (!SLEEP_IS_INITIATOR && isSync) {
        // el temps d'una transmissió del node ANTERIOR a ell, que és la que aqeust ha de rebre,
        // és les dades que aquest ha transmes (nodes * size) més els headers de totes les capes (mida màxima lora - sleep data,
        // la resta són headers)
        uint8_t expectedRcvSize = nodesPrevis * SLEEP_DATASIZE_PER_NODE + LORA_MAX_SIZE - SLEEP_MAX_DATA_SIZE;
        long transmitTime = US_TO_MS(LoRaRAW_getTimeOnAir(expectedRcvSize));
        sleepTime = SLEEP_CYCLE_DURATION - (tempsDone - tempsSync) - SLEEP_CLOCK_CORRECTION - transmitTime - deltaTime - SLEEP_EXTRA_TIME;
        _PI("[SLEEP] Expected reception: %d B, Transmission time: %lu ms, sleeping for %.2f seconds", expectedRcvSize, transmitTime, MS_TO_S(sleepTime));
        _PI("[SLEEP] Sync time: %lu ms, Done time: %lu ms", tempsSync, tempsDone);
        sleepTime = MAX((int64_t) sleepTime, 0);
    }
    else { // SI som iniciadors o no hem rebut SYNC, dormim el temps restant de cicle
        sleepTime = SLEEP_CYCLE_DURATION - tempsDone;
    }

    sleepTime = MAX((int64_t) sleepTime, 0);

    // Activar wakeup a partir de timer
    esp_sleep_enable_timer_wakeup(MS_TO_US(sleepTime));
    // Posar radio a dormir
    LoRaRAW_sleep();
    _PI("[SLEEP] Going to sleep for %.2f minutes", MS_TO_MIN(sleepTime));
    // Iniciar sleep

    if (syncCount > 0) {
        bootCount++;
        doneTimeSum += tempsDone;
    }

    _PI("[SLEEP-STATS] Sync: %d\tSleep time: %llu ms\tSync time: %lu ms\tDone time: %lu ms\tDelta Time: %llu ms\tSync AVG: %.2f ms\tDone AVG: %.2f ms\tSync count: %d\tBoot count: %d",
        isSync, sleepTime, tempsSync, tempsDone, deltaTime, syncTimeSum / (float) syncCount, doneTimeSum / (float) bootCount, syncCount, bootCount);
    esp_deep_sleep_start();
}

static void syncTimeout() {
    _PW("[SLEEP] Sync timeout. Making new PDU, and sending to next node");
    receivedPDU.command = SLEEP_CMD_SYNC;
    receivedPDU.dataLen = 0;

    // El temps de SYNC és 0, no s'ha sincronitzat
    tempsSync = 0;

    // Simulem recepció de SYNC. Com que receivedPDU està inicialitzada, ja posarà les seves dades allà
    forwardSync();
    // onSyncReceived();
}

// Processat en rebre dades d'aplicació SLEEP
static void received() {
    // sleep_pdu_t data;
    size_t datalen;
    transport_port_t port;
    Transport_receive(&port, (transport_data_t*)&receivedPDU, &datalen);
    if(port != SLEEP_PORT) {
        _PE("[SLEEP] Received data on wrong port: %d. Check transport layer handlers", port);
        return;
    }

    decodePDU();
}

static void sent() {
    _PI("[SLEEP] Forwarded data");
    goToSleep();
}

static void sendError() {
    _PI("[SLEEP] Error forwarding data. Going to sleep.");
    goToSleep();
}
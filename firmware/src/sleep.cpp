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
static RTC_DATA_ATTR uint64_t deltaTime = 0;

static Task* timeoutTask = nullptr;

static sleep_callback_t onSync = nullptr;

// Instant en que s'ha rebut el SYNC, en `milisegons`
static unsigned long tempsSync = 0;

// Quantitat de nodes que hi ha anteriors a aquest
static RTC_DATA_ATTR uint8_t nodesPrevis = 0;
static RTC_DATA_ATTR uint16_t syncCount = 0;
static RTC_DATA_ATTR uint64_t syncTimeSum = 0;
static RTC_DATA_ATTR uint16_t bootCount = 0;
static RTC_DATA_ATTR uint64_t doneTimeSum = 0;

unsigned long timeout_ms = 0;

bool Sleep_init(void) {
    // REQUEREIX TRANSPORT INICIALITZAT!
    // No s'inicialitza aquí per evitar haver de conèixer @ node.
    // Programa principal és qui hauria d'inicialitzar el protocol

    // Si som iniciadors, simulem que hem rebut SYNC per enviar i iniciar funcionament normal
    if (SLEEP_IS_INITIATOR) {
        _PI("[SLEEP] Initiator. Making PDU and sending.");
        receivedPDU.command = SLEEP_CMD_SYNC;
        receivedPDU.dataLen = 0;
        // onSyncReceived();
        scheduler_once(onSyncReceived);
    }
    else if(!isFirstSync) { // Si ja haviem sincronitzat anteriorment, iniciem TOUT
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
        if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
            _PW("[SLEEP] Wakeup reason: Timer");
        else 
            _PW("[SLEEP] Wakeup reason: %d", wakeup_reason);


        // computeDeltaTime();
        // 2*extra per compensar el que hi ha davant, i el que volem deixar
        // De marge deixem 2: un pel que despertem abans, i un i mig per assegurar després
        timeout_ms = MAX(0, 2*deltaTime + 2*SLEEP_EXTRA_TIME);
        // timeout_ms = MAX(0, 2*deltaTime + SLEEP_CLOCK_CORRECTION + 2*SLEEP_EXTRA_TIME);
        // long timeout_ms = 2*deltaTime + SLEEP_CLOCK_CORRECTION + SLEEP_EXTRA_TIME; // AQUEST ÉS EL QUE HI HAVIA QUAN FUNCIONAVA!
        timeoutTask = scheduler_once(syncTimeout, timeout_ms);

        _PW("[SLEEP] Init. Already synchronized.");
        _PW("        Clock correction: %llu ms", SLEEP_CLOCK_CORRECTION); 
        _PW("        Delta time: %llu ms. Nodes before: %d", deltaTime, nodesPrevis);
        _PW("        Timeout in %lu ms", timeout_ms);
    } // Si no haviem sincronitzat mai, quedem a l'espera i en funcionament permanent fins SYNC
    else {
        _PI("[SLEEP] Init. Not synchronized. Waiting for first sync.");
    }

    Transport_onEvent(SLEEP_PORT, received, sent, sendError);

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
    // N per nodes anteriors, (R+1) pels intents totals de TX
    // ACK_FACTOR per quant de temps de transmissió per rebre ACK es deixa
    // AIRTIME pel temps de transmissió
    // N · (R+1) dona transmissions totals
    // N · (R+1) · TX dona temps TX total; N · (R+1) · ACK · TXACK dona temps espera ACK
    // N · (R+1) · TX + N · (R+1) · ACK · TXACK = N · (R+1) · (TX + ACK · TXACK)
    
    uint64_t max_ack_time_ms = US_TO_MS(LoRaRAW_getTimeOnAir(MAC_PDU_HEADER_SIZE)); // Un ACK de MAC té mida del header
    uint64_t tx_time_ms = US_TO_MS(LoRaRAW_getTimeOnAir(LORA_MAX_SIZE));
    deltaTime = nodesPrevis * (MAC_MAX_RETRIES + 1) * (tx_time_ms + MAC_ACK_TIMEOUT_FACTOR * max_ack_time_ms);
    // deltaTime = nodesPrevis * (MAC_MAX_RETRIES+1) * MAC_ACK_TIMEOUT_FACTOR * US_TO_MS(LoRaRAW_getTimeOnAir(LORA_MAX_SIZE));
    deltaTime = deltaTime * (1 + SLEEP_DELTA_EXTRA); // 25% marge per CSMA
    _PW("[SLEEP] Delta time compute: TX time = %llu ms + ACK time = %llu ms (%d ACK factor) for %d nodes up to (%d+1) txs, with %.2f extra", tx_time_ms, max_ack_time_ms, MAC_ACK_TIMEOUT_FACTOR, nodesPrevis, MAC_MAX_RETRIES, SLEEP_DELTA_EXTRA);
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
    // Verifiquem si hi podem posar les nostres dades a camp dades PDU sense passar-nos de mida màxima
    if (receivedPDU.dataLen + SLEEP_DATASIZE_PER_NODE <= SLEEP_MAX_DATA_SIZE) {
        if (onSync != nullptr) {
            // onSync ha de copiar les dades que li interessi a l'apuntador
            uint8_t* data = receivedPDU.data + receivedPDU.dataLen;
            onSync(data, SLEEP_DATASIZE_PER_NODE);
            
            // Copiem les dades que afegeix onSync() a les dades de la PDU
            // i incrementem la mida de les dades
            // memcpy(receivedPDU.data + receivedPDU.dataLen, data, SLEEP_DATASIZE_PER_NODE);
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
            goToSleep();
        } else {
            _PI("[SLEEP] Sync forwarded to node %d", forwardCmdTo);
        }
    }
    else { // Si no hi ha adreça definida, dormir en rebre SYNC. No podem fer-hi res.
        _PW("[SLEEP] No forward node set. Sync not forwarded");
        goToSleep();
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
        // el temps d'una transmissió del node ANTERIOR a ell, que és la que aqeust ha de rebre,
        // és les dades que aquest ha transmes (nodes * size) més els headers de totes les capes (mida màxima lora - sleep data,
        // la resta són headers)
        uint8_t expectedRcvSize = nodesPrevis * SLEEP_DATASIZE_PER_NODE + LORA_MAX_SIZE - SLEEP_MAX_DATA_SIZE;
        long transmitTime = US_TO_MS(LoRaRAW_getTimeOnAir(expectedRcvSize));

        // AQUEST ÉS EL QUE HI HAVIA QUAN FUNCIONAVA!
        // sleepTime = SLEEP_CYCLE_DURATION - (tempsDone - tempsSync) - SLEEP_CLOCK_CORRECTION - transmitTime - deltaTime - SLEEP_EXTRA_TIME;
        
        // Posar millis aqui i no fora de l'if -> càlcul de transmitTime és car i pot endarrerir en varis segons
        tempsDone = millis();
        // sleep time intenta compensar l'error que el propi micro introdueix amb el clock. No podem predir el clock de l'anterior node: s'ha vist com un dispositiu pot tenir 2000ppm, però un altre en pot tenir 25000ppm
        sleepTime = SLEEP_CYCLE_DURATION - (tempsDone - tempsSync) - transmitTime - deltaTime - SLEEP_EXTRA_TIME + SLEEP_CLOCK_CORRECTION;
        _PW("[SLEEP] Expected reception: %d B, Transmission time: %lu ms", expectedRcvSize, transmitTime);
        _PW("[SLEEP] Sleep breakdown: %llu ms = %llu ms - (%lu ms - %lu ms) - %lu ms - %llu ms - %llu ms + %lld ms", 
            sleepTime, SLEEP_CYCLE_DURATION, tempsDone, tempsSync, transmitTime, deltaTime, SLEEP_EXTRA_TIME, SLEEP_CLOCK_CORRECTION);
    }
    else { // SI som iniciadors o no hem rebut SYNC, dormim el temps restant de cicle
        // aplicant clock correction també: 
        /*Si PPM < 0, clock és més lent:
             Per cada segon REAL, el microcontrolador dormirà 1 segon i una mica més
             Per evitar-ho, cal dormir MENYS temps (1seg - D), fent que dormi realment 1 segon
          Si PPM > 0, clock és més ràpid:
             Per cada segon REAL, el microcontrolador dormirà 1 segon i una mica menys
             Per evitar-ho, cal dormir MÉS temps (1seg + D), fent que dormi realment 1 segon
        */
        tempsDone = millis();

        // sleepTime = SLEEP_CYCLE_DURATION - tempsDone; // AQUEST ÉS EL QUE HI HAVIA QUAN FUNCIONAVA!
        // sleepTime = SLEEP_CYCLE_DURATION - tempsDone - SLEEP_CLOCK_CORRECTION; // AQUEST HI HAVIA ABANS DE CANVIS EN CLOCK CORRECTION
        sleepTime = SLEEP_CYCLE_DURATION - tempsDone + SLEEP_CLOCK_CORRECTION;
        _PW("[SLEEP] Sleep breakdown: %llu ms = %llu ms - %lu ms + %llu ms", 
            sleepTime, SLEEP_CYCLE_DURATION, tempsDone, SLEEP_CLOCK_CORRECTION);
    }

    // Assegurem que sleep time no és negatiu (hagi fet overflow en uint)
    sleepTime = MAX((int64_t) sleepTime, 0);

    _PE("[SLEEP-STATS] Sync: %d\tSleep time: %llu ms\tSync time: %lu ms\tDone time: %lu ms\tDelta Time: %llu ms\tClk correction: %llu ms\t"
                      "Sync AVG: %.2f ms\tDone AVG: %.2f ms\tSync count: %d\tBoot count: %d\tTimeout:  %lu ms",
        isSync, sleepTime, tempsSync, tempsDone, deltaTime, SLEEP_CLOCK_CORRECTION , syncTimeSum / (float) syncCount, doneTimeSum / (float) bootCount, syncCount, bootCount, timeout_ms);
    
    // Iniciem deep sleep
    _PE("[SLEEP] Extra %llu ms (%llu)", (millis()-tempsDone), sleepTime-(millis()-tempsDone));
    esp_deep_sleep(MS_TO_US(sleepTime));
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
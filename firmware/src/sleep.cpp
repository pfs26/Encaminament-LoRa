// /*
//     Capa aplicació per gestió de baix consum.

//     Funcionament:
//         1. En iniciar estan en mode de recepció. No envien res. Només esperen el missatge "SYNC" de forma INDEFINIDA.
//            És el pas de sincronització inicial.
//         2. En rebre missatge "SYNC", el reenvien a uns nodes determinats i coneguts (Llista `forwardCmdTo`)
//         3. Després de reenviar i deixar un temps de marge Gt (Guard Time), està en mode de funcionament normal.
//         4. Manté mode de funcionament normal durant Wt (Work Time), espera Gt, i després es torna a posar en mode sleep.
//         5. Manté mode sleep durant St (Sleep Time), i després es torna a despertar. Es desperta a T0.
//         6. Espera a rebre el missatge "SYNC" durant SyT (Sync Time).
//             6.1. Si no rep missatge "SYNC" manté funcionament normal durant Wt i es torna a adormir.
//                  Redueix St en un 1% -> Despertarà abans, intentant rebre llavors "SYNC". Sempre dins d'un límit mínim.

//                  Si en qualsevol moment de funcionament normal rep el missatge de Sync (a temps T1) **i no havia sincronitzat**
//                  és que s'ha despertat massa d'hora. Augmentar St tal que `St = St + (T1-T0) / 2`
//             6.2. Si rep el "SYNC" a T1, augmentarà St tal que `St = St + (T1-T0) / 2`.
//                  Això farà que es desperti més tard, però sempre abans que quan rebi "SYNC".
//                  Intenta augmentar temps sleep per reduir consum
//         7. Repeteix passos 2-6 indefinidament.
// */

#include <Arduino.h>
#include "scheduler.h"
#include "utils.h"
#include "sleep.h"

void onSyncReceived();
void goToSleep();
void forwardCMD(sleep_command_t cmd);
void onWakeup();
void received();

static node_address_t forwardCmdTo[SLEEP_MAX_FORWARD_NODES]; // Nodes a qui reenviar el SYNC

// Temps de sleep en `milisegons`. A memòria RTC per mantenir-lo en deep sleep. S'actualitza a partir de "controlador P"
static RTC_DATA_ATTR uint64_t sleepTime = SLEEP_SLEEP_TIME;
// Temps de treball en `milisegons`. A memòria RTC per mantenir-lo en deep sleep. S'actualitza a partir de "controlador P"
// Augmenta a mesura que es redueix sleepTime; en rebre SYNC, es reinicia a `SLEEP_WORK_TIME`
static RTC_DATA_ATTR uint64_t workTime = SLEEP_WORK_TIME;
// Control si està sincronitzat a la xarxa o no
static RTC_DATA_ATTR bool isSync = false;

static Task* workToutTask = nullptr;

static sleep_callback_t onSync = nullptr;
static sleep_callback_t onBeforeSleep = nullptr;

// Instant en que s'ha rebut el SYNC, en `milisegons`
// Permet ajustar temps de sleep amb control
static long tempsRecepcioSync = -1;

bool Sleep_init(void) {
    // REQUEREIX TRANSPORT INICIALITZAT!
    // No s'inicialitza aquí per evitar haver de conèixer @ node.
    // Programa principal és qui hauria d'inicialitzar el protocol

    // Executar mètode inicial per configurar tasques de sleep inicials
    // `onWakeup`, ja que s'executa després de cada inicialització o wakeup
    onWakeup();
    _PI("[SLEEP] Init: %.2f minutes sleep (default: %.2f), %.2f minutes work (default: %.2f)", MS_TO_MIN(sleepTime), MS_TO_MIN(SLEEP_SLEEP_TIME), MS_TO_MIN(workTime), MS_TO_MIN(SLEEP_WORK_TIME));

    Transport_onEvent(SLEEP_PORT, received, nullptr);

    // Si som iniciadors, simulem que hem rebut SYNC per enviar i iniciar funcionament normal
    if (SLEEP_IS_INITIATOR) {
        onSyncReceived();
    }

    return true;
}

void Sleep_deinit(void) {
    // Deshabilitem callbacks
    onSync = nullptr;
    onBeforeSleep = nullptr;
    // Cancel·lem tasques programades
    if(workToutTask != nullptr) {
        scheduler_stop(workToutTask);
        workToutTask = nullptr;
    }
    // Temps per defecte
    sleepTime = SLEEP_SLEEP_TIME;
    workTime = SLEEP_WORK_TIME;
    isSync = false;
    // Deshabilitem transport a port de SLEEP
    Transport_deinit(SLEEP_PORT);
}

bool Sleep_setForwardNodes(node_address_t* nodes, size_t numNodes) {
    if(numNodes > SLEEP_MAX_FORWARD_NODES) {
        _PW("[SLEEP] Too many nodes to forward to. Max: %d", SLEEP_MAX_FORWARD_NODES);
        return false;
    }
    for(int i = 0; i < SLEEP_MAX_FORWARD_NODES; ++i) {
        forwardCmdTo[i] = i < numNodes ? nodes[i] : NODE_ADDRESS_NULL;
    }
    _PI("[SLEEP] Modified `forwardCmdTo` nodes");
    return true;
}


void Sleep_onSync(sleep_callback_t cb) { onSync = cb; }

void Sleep_onBeforeSleep(sleep_callback_t cb) { onBeforeSleep = cb; }

void _onBeforeSleep() {
    if(onBeforeSleep != nullptr) {
        onBeforeSleep();
    }
}

void _onSync() {
    if(onSync != nullptr) {
        onSync();
    }
}

// Descodifica una PDU de Sleep i la processa.
// Poca utilitat actualment (únicament ordre SYNC)
// però es manté per si es volen afegir més ordres
void decodePDU(sleep_pdu_t* pdu) {
    _PI("[SLEEP] Decoding PDU. Command: %d, data: %s", pdu->command, (char*)pdu->data);
    switch(pdu->command) {
        case SLEEP_CMD_SYNC:
            onSyncReceived();
            break;
        default:
            _PW("[SLEEP] Unknown command: %d", pdu->command);
            break;
    }
}

// Reenvia una ordre `CMD` als nodes de la llista `forwardCmdTo`
// De moment poc útil, ja que només es fa servir per reenviar SYNC
// que no té dades al camp de dades.
// Caldria afegir un paràmetre de dades per altres ordres si es volgués
void forwardCMD(sleep_command_t cmd) {
    sleep_pdu_t data;
    data.command = cmd;
    memcpy(data.data, "", 1);
    for(int i = 0; i < SLEEP_MAX_FORWARD_NODES; i++) {
        if(forwardCmdTo[i] == NODE_ADDRESS_NULL) break;
        transport_err_t state = Transport_send(forwardCmdTo[i], SLEEP_PORT, (uint8_t*)&data, strlen((char*)data.data)+1 , false);
        if(state == TRANSPORT_ERR) {
            _PW("[SLEEP] Error forwarding command %d to node %d", cmd, forwardCmdTo[i]);
        } else {
            _PI("[SLEEP] Command %d forwarded to node %d", cmd, forwardCmdTo[i]);
        }
    }
}

// Executat en rebre missatge de sincronització.
// Si tasca de 1r boot establerta, la cancel·la.
// Reenvia missatge de sincronització a nodes de la llista `forwardCmdTo`.
void onSyncReceived() {
    if(tempsRecepcioSync != -1) {
        _PI("[SLEEP] Sync already received");
        return;
    }
    _PI("[SLEEP] Sync received");

    // Si és primera sincronització, marquem recepció SYNC a instant 0
    // per tal que no afecti retard inicial al càlcul de cicle de sleep
    tempsRecepcioSync = isSync ? millis() : 0;

    // En qualsevol cas el node ja estarà sincronitzat a la xarxa
    isSync = true;

    // Si hi havia una tasca programada (tasca de treball inicial per si no es rep SYNC)
    // la parem i n'iniciem una de nova després de rebre SYNC
    if(workToutTask != nullptr) {
        scheduler_stop(workToutTask);
        workToutTask = nullptr;
        _PI("[SLEEP] Cancelled sleep timeout");
    }

    // Programar sleep ABANS de reenviar SYNC, per tal que això no afecti a sincronització amb altres nodes
    // Si un node ho ha de reenviar a molts, potser triga molt temps, dorm més tard del compte i NO rep següent sync
    // WORK TIME inclou temps de reenviament de SYNC
    workTime = SLEEP_WORK_TIME;
    workToutTask = scheduler_once(goToSleep, workTime);

    // Reenviar SYNC a nodes de la llista
    forwardCMD(SLEEP_CMD_SYNC);

    // Programem notificació de recepció de sync. S'executarà
    // després de fer transmissions a forwardToNodes
    scheduler_once(_onSync);
    _PI("[SLEEP] Scheduling sleep in %.2f minutes", MS_TO_MIN(workTime));
}

// Posa el microcontrolador i ràdio en mode de baix consum
// Determina el temps de sleep a partir de l'instant de recepció de SYNC,
// augmentant el temps de sleep si s'ha rebut després d'estona d'haver despertat,
// o disminuint-lo si no s'ha rebut, augmentant així la finestra de recepció.
void goToSleep() {
    // Si som inicialitzadors, el cicle de sleep no s'ha de modificar
    if(!SLEEP_IS_INITIATOR) {
        // Si encara no s'ha sincronitzat inicialment a la xarxa, no modificar temps de sleep
        // (criteri marcat per evitar reduir encara més temps de sleep, en teoria temps despert s
        // si no sincronitzat hauria de ser major que sincronitzat)
        if(!isSync) {
            _PI("[SLEEP] Not yet synchronized with network. Sleeping won't be modified");
        }
        // Si no s'ha rebut SYNC en el temps de treball, reduir el temps de sleep, despertant abans per si es rep SYNC
        else if(tempsRecepcioSync == -1) {
            uint64_t reduccio = sleepTime*SLEEP_SLEEP_TIME_FACTOR_NSYNC;
            sleepTime -= reduccio;
            if(sleepTime < SLEEP_MIN_SLEEP_TIME) {
                sleepTime = SLEEP_MIN_SLEEP_TIME;
            } else {
                workTime += reduccio; // Augmentar temps de treball de següent cicle -> estarà més temps despert INICIALMENT
                _PI("[SLEEP] Sync not received. Sleep time reduced by %.2f%% (%llu ms)", (SLEEP_SLEEP_TIME_FACTOR_NSYNC*100), reduccio);
            }
        }
        else { // S'ha rebut sync. Intentem augmentar temps de sleep si s'ha rebut després d'estona
            uint64_t increment = tempsRecepcioSync  * SLEEP_SLEEP_TIME_FACTOR_SYNC; // ms
            uint64_t diferencia = tempsRecepcioSync - increment; // ms
            // Limitem increment per tal que minim hi hagi `SLEEP_MIN_TIME_BEFORE_SYNC` entre despertar i recepció esperada SYNC
            if (increment < SLEEP_MIN_TIME_BEFORE_SYNC) {
                increment = increment - SLEEP_MIN_TIME_BEFORE_SYNC + diferencia;
                _PI("[SLEEP] Sync received. Sleep time limited");
            }
            else {
                _PI("[SLEEP] Sync received. Sleep time increased by %llu ms", increment);
            }
            sleepTime += increment;
        }
    }

    // Notifiquem que s'anirà a dormir, per si es volen realitzar accions
    // Haurien de ser accions ràpides, per no afectar temps de dormir
    _onBeforeSleep();

    // Activar wakeup a partir de timer
    esp_sleep_enable_timer_wakeup(MS_TO_US(sleepTime));
    // Posar radio a dormir
    LoRaRAW_sleep();
    _PI("[SLEEP] Going to sleep for %.2f minutes", MS_TO_MIN(sleepTime));
    // Iniciar sleep
    esp_deep_sleep_start();
}

// Processat en rebre dades d'aplicació SLEEP
void received() {
    sleep_pdu_t data;
    size_t datalen;
    transport_port_t port;
    Transport_receive(&port, (transport_data_t*)&data, &datalen);
    if(port != SLEEP_PORT) {
        _PE("[SLEEP] Received data on wrong port: %d. Check transport layer handlers", port);
        return;
    }

    decodePDU(&data);
}

// Mètode executat en despertar. En despertar de deepsleep `millis() == 0`.
// Programa sleep de nou al cap de `workTime` segons, que depèn de `SLEEP_WORK_TIME`
// i de temps de recepcions de SYNC anteriors
void onWakeup() {
    _PI("[SLEEP] Wake up");
    if(isSync) {
        workToutTask = scheduler_once(goToSleep, workTime);
        _PI("[SLEEP] Scheduling sleep in %.2f minutes", MS_TO_MIN(workTime));
    }
    else {
        workToutTask = scheduler_once(goToSleep, SLEEP_FIRST_BOOT_TOUT);
        _PI("[SLEEP] Scheduling init timeout in %.2f minutes", MS_TO_MIN(SLEEP_FIRST_BOOT_TOUT));
    }
    // Programa sleep de nou al cap de `workTime` segons per si no es rep SYNC.
    // Si es rep SYNC, s'iniciarà de nou i `workTime` passarà a ser `SLEEP_WORK_TIME`
    // per mantenir el cicle de funcionament normal.
}
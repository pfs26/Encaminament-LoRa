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
#include "transport.h"
#include "scheduler.h"
#include "utils.h"

// Macros utils per conversió
#define MS_TO_US(x) (x * 1000ULL)           // Milisegons a microsegons
#define MS_TO_S(x) (x / 1000)               // Milisegons a segons
#define MS_TO_MIN(x) (x / 60000)            // Milisegons a minuts
#define S_TO_US(x) (x * 1000000ULL)         // Segons a microsegons
#define S_TO_MS(x) (x * 1000ULL)            // Segons a milisegons
#define S_TO_MIN(x) (x / 60)                // Segons a minuts
#define MIN_TO_US(x) (S_TO_US(x * 60))      // Minuts a microsegons
#define MIN_TO_MS(x) (x * 60 * 1000ULL)     // Minuts a milisegons
#define MIN_TO_S(x) (x * 60)                // Minuts a segons
#define H_TO_MS(x) (x * 60 * 60 * 1000ULL)  // Hores a milisegons

#define SLEEP_PORT 0x01
#define SLEEP_MAX_FORWARD_NODES 10

// Temps de sleep mínim que controlador P no superarà, en el cas que no rebi SYNC, en `ms`. 
#define SLEEP_MIN_SLEEP_TIME S_TO_MS(15)

// Defineix si és un node "iniciador" de xarxa.
// Enviarà SYNC sense esperar a rebre'l 
#define SLEEP_IS_INITIATOR false

// Durada d'un cicle de funcionament, en `ms`
#define SLEEP_CYCLE_DURATION S_TO_MS(30)
// #define SLEEP_CYCLE_DURATION (24*60)
// Temps de funcionament normal, en `ms`
#define SLEEP_WORK_TIME S_TO_MS(10)
// Temps de sleep **per defecte**; controlador funcionarà a partir d'aquest. En `ms`
#define SLEEP_SLEEP_TIME (SLEEP_CYCLE_DURATION-SLEEP_WORK_TIME)

// Factor de reducció del temps de sleep en cada cicle si no rep SYNC
#define SLEEP_SLEEP_TIME_FACTOR_NSYNC 0.1
// Factor de "controlador P" en rebre SYNC. Si és 0.5, el temps de sleep augmentarà la meitat del temps de recepció de SYNC. 
#define SLEEP_SLEEP_TIME_FACTOR_SYNC  0.5  

// Temps mínim que volem que estigui despert abans de rebre SYNC, en `milisegons`
// Evita que controlador P augmenti massa el temps de sleep, i deixa un marge
// de mínim aquest temps entre despertar i rebre SYNC (basant-se en temps SYNC anterior)
// **IMPORTANT** que deixi suficient marge per tal que la recepció de SYNC no es perdi
// Valors d'algun segon semblen ser adequats
#define SLEEP_MIN_TIME_BEFORE_SYNC S_TO_MS(2)

// Temps màxim d'espera per primera sincronització a la xarxa, en `ms`
// Com a últim recurs per evitar deixar ràdio + ESP actius permanentment
// Dormirà durant `SLEEP_SLEEP_TIME`
#define SLEEP_FIRST_BOOT_TOUT MIN_TO_MS(2)

enum sleep_command_t : uint8_t {
    SLEEP_CMD_NONE,
    SLEEP_CMD_SYNC,             // Sincronització i inici de cicle
    SLEEP_CMD_WAKEUP,           // Mantenir despert
    SLEEP_CMD_SLEEP,            // Iniciar sleep
    SLEEP_CMD_SET_WORK_TIME,    // Establir temps treball
};

typedef struct {
    sleep_command_t command;
    uint8_t data[TRANSPORT_MAX_DATA_SIZE-sizeof(sleep_command_t)]; // serà en format STRING, acabat amb \0
} sleep_pdu_t;

void onSyncReceived();
void onFirstBoot();
void goToSleep();
void forwardCMD(sleep_command_t cmd);

static node_address_t forwardCmdTo[SLEEP_MAX_FORWARD_NODES] = {0x06}; // Nodes a qui reenviar el SYNC
static node_address_t self = 0x03;

// Temps de sleep en `milisegons`. A memòria RTC per mantenir-lo en deep sleep. S'actualitza a partir de "controlador P"
static RTC_DATA_ATTR uint64_t sleepTime = SLEEP_SLEEP_TIME; 
// Temps de treball en `milisegons`. A memòria RTC per mantenir-lo en deep sleep. S'actualitza a partir de "controlador P"
static RTC_DATA_ATTR uint64_t workTime = SLEEP_WORK_TIME; 
// Control si és el primer cop que s'executa el programa
static RTC_DATA_ATTR bool firstBoot = true;
static RTC_DATA_ATTR bool isSync = false;

static Task* workToutTask = nullptr;

// Instant en que s'ha rebut el SYNC, en `milisegons`
static long tempsRecepcioSync = -1;

void decodePDU(sleep_pdu_t* pdu) {
    Serial.print("Command: ");
    Serial.println(pdu->command);
    Serial.print("Data: ");
    Serial.println((char*)pdu->data);
    switch(pdu->command) {
        case SLEEP_CMD_SYNC:
            onSyncReceived();
            break;
        default:
            Serial.println("Unknown command");
            break;
    }
}

void forwardCMD(sleep_command_t cmd) {
    sleep_pdu_t data;
    data.command = cmd;
    memcpy(data.data, "SYNC", 5);
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

/*  Executat en rebre missatge de sincronització.
    Si tasca de 1r boot establerta, la cancel·la.
    Reenvia missatge de sincronització a nodes de la llista `forwardCmdTo`.
*/
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
    _PI("[SLEEP] Scheduling sleep in %llu s", MS_TO_S(workTime));
}


void goToSleep() {
    // Si no s'ha rebut SYNC en el temps de treball, reduir el temps de sleep, despertant abans per si es rep SYNC
    if(tempsRecepcioSync == -1) {
        uint64_t reduccio = sleepTime*SLEEP_SLEEP_TIME_FACTOR_NSYNC;
        sleepTime -= reduccio;
        if(sleepTime < SLEEP_MIN_SLEEP_TIME) {
            sleepTime = SLEEP_MIN_SLEEP_TIME;
        } else {
            workTime += reduccio; // Augmentar temps de treball de següent cicle -> estarà més temps despert INICIALMENT
            _PI("[SLEEP] Sync not received. Sleep time reduced by %d%% (%llu ms)", (SLEEP_SLEEP_TIME_FACTOR_NSYNC*100), reduccio);
        }
    }
    else {
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

    _PI("[SLEEP] Going to sleep for %llu seconds", MS_TO_S(sleepTime));
    esp_sleep_enable_timer_wakeup(MS_TO_US(sleepTime)); // Set the ESP32 to wake up after sleepTime
    LoRaRAW_sleep();
    _PI("[SLEEP] Going to sleep");
    esp_deep_sleep_start(); // Enter deep sleep mode
}


void received() {
    sleep_pdu_t data;
    size_t datalen;
    transport_port_t port;
    Transport_receive(&port, (transport_data_t*)&data, &datalen);
    if(port != SLEEP_PORT) {
        Serial.println("Port no vàlid");
        return;
    }

    decodePDU(&data);
}


void sent() {

}

// Mètode executat en despertar. En despertar de deepsleep `millis() == 0`.
// Programa sleep de nou al cap de `workTime` segons, que depèn de `SLEEP_WORK_TIME`
// i de temps de recepcions de SYNC anteriors
void onWakeup() {
    _PI("[SLEEP] Wake up");
    if(isSync) {
        workToutTask = scheduler_once(goToSleep, workTime); 
        _PI("[SLEEP] Scheduling sleep in %llu seconds", MS_TO_S(workTime));
    }
    else {
        workToutTask = scheduler_once(goToSleep, SLEEP_FIRST_BOOT_TOUT); 
        _PI("[SLEEP] Scheduling init timeout in %llu seconds", MS_TO_S(SLEEP_FIRST_BOOT_TOUT));
    }
    // Programa sleep de nou al cap de `workTime` segons per si no es rep SYNC. 
    // Si es rep SYNC, s'iniciarà de nou i `workTime` passarà a ser `SLEEP_WORK_TIME`
    // per mantenir el cicle de funcionament normal. 
}

void setup() {
    Serial.begin(921600);

    // Executat únicament en 1r boot
    if(firstBoot) {
        Serial.println("===============================");
        Serial.println(" Reduced power consumption APP");
        Serial.println("===============================");
    }
    firstBoot = false;

    onWakeup();
    
    _PI("[SLEEP] Init: %llu minutes sleep (default: %llu), %llu seconds work (default: %llu)", MS_TO_MIN(sleepTime), MS_TO_MIN(SLEEP_SLEEP_TIME), MS_TO_S(workTime), MS_TO_S(SLEEP_WORK_TIME));

    if(!Transport_init(self, false)) {
        Serial.println("Transport init failed");
        while(1) delay(1);
    }

    // Si som iniciadors, simulem que hem rebut SYNC per enviar i iniciar funcionament normal
    if (SLEEP_IS_INITIATOR) {
        onSyncReceived();
    }

    // transport_data_t data = "Hello!";
    // transport_err_t state = Transport_send(0x01, 0x01, (uint8_t*)&data, 6, false);
    // if(state == TRANSPORT_ERR) {
    //     Serial.println("Send failed");
    // } else {
    //     Serial.println("Send OK");
    // }
    Transport_onEvent(SLEEP_PORT, received, sent);
}

// Loop únicament hauria de tenir "scheduler_run()"
void loop() {
    scheduler_run();
}



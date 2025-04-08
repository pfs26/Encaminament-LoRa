#ifndef SLEEP_H
#define SLEEP_h
#include "transport.h"
#include "config.h"

// Macros utils per conversió
#define MS_TO_US(x) (x * 1000ULL)           // Milisegons a microsegons
#define MS_TO_S(x) (x / 1000.0)               // Milisegons a segons
#define MS_TO_MIN(x) (x / 60000.0)            // Milisegons a minuts
#define S_TO_US(x) (x * 1000000ULL)         // Segons a microsegons
#define S_TO_MS(x) (x * 1000ULL)            // Segons a milisegons
#define S_TO_MIN(x) (x / 60.0)                // Segons a minuts
#define MIN_TO_US(x) (S_TO_US(x * 60))      // Minuts a microsegons
#define MIN_TO_MS(x) (x * 60 * 1000ULL)     // Minuts a milisegons
#define MIN_TO_S(x) (x * 60)                // Minuts a segons
#define H_TO_MS(x) (x * 60 * 60 * 1000ULL)  // Hores a milisegons

// Port que utilitza l'aplicació de SLEEP
#define SLEEP_PORT 0x01
// Nombre màxim de nodes a qui es reenviaran les ordres de SLEEP. 
// Si és molt gran, overhead per enviar missatges de SYNC pot serà alt
#define SLEEP_MAX_FORWARD_NODES 10


// Temps de sleep **per defecte**; controlador funcionarà a partir d'aquest. En `ms`
#define SLEEP_SLEEP_TIME (SLEEP_CYCLE_DURATION - SLEEP_WORK_TIME)



// Enum per definir ordres possibles. Definició per ús futur
enum sleep_command_t : uint8_t {
    SLEEP_CMD_NONE,
    SLEEP_CMD_SYNC,  // Sincronització i inici de cicle
    // SLEEP_CMD_WAKEUP,           // Mantenir despert
    // SLEEP_CMD_SLEEP,            // Iniciar sleep
    // SLEEP_CMD_SET_WORK_TIME,    // Establir temps treball
};

typedef struct {
    sleep_command_t command;
    uint8_t
        data[TRANSPORT_MAX_DATA_SIZE -
             sizeof(sleep_command_t)];  // serà en format STRING, acabat amb \0
} sleep_pdu_t;

typedef void (*sleep_callback_t)(void);

// Inicialitza l'aplicació de SLEEP.
// Espera rebre un missatge de sincronització, el reenvia i deixa en funcionament normal
// durant `SLEEP_WORK_TIME`. Seguidament es posa en baix consum.
// El temps de baix consum s'adapta al temps de recepció de SYNC per intentar etar-hi el màxim temps
bool Sleep_init(void);
// Deixa l'aplicació de sleep en la configuració determinada, deshabilitant totes les seves tasques
void Sleep_deinit(void);
// Estableix els nodes a qui s'han de reenviar les ordres. Retorna `true` si s'han pogut establir,
// `false` si no s'han pogut establir (per exemple, si el nombre de nodes és superior al màxim permès)
bool Sleep_setForwardNodes(node_address_t* nodes, size_t numNodes);
// Registrar un callback a executar quan s'ha rebut el missatge de sincronització.
// Un cop rebut, es disposa de `SLEEP_WORK_TIME` de funcionament normal.
// Si no s'executa el callback (no hi ha recepció de SYNC), no es pot garantir un
// correcte funcionament
void Sleep_onSync(sleep_callback_t cb);
// Registrar un callback a executar abans d'anar a dormir.
// Haurien de ser accions ràpides per no afectar al temps de dormir, que ja s'ha determinat
// en funció de temps de recepció (o no-recepció) de SYNC
void Sleep_onBeforeSleep(sleep_callback_t cb);

#endif

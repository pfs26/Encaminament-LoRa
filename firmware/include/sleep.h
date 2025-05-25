#ifndef SLEEP_H
#define SLEEP_h
#include "transport.h"
#include "config.h"

// Macros utils per conversió
#define US_TO_MS(x) (x / 1000.0)            // Microsegons a milisegons
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

// Defineix si és un node "iniciador" de xarxa. Enviarà SYNC sense esperar a rebre'l. 
#define SLEEP_IS_INITIATOR !IS_GATEWAY

// Durada d'un cicle de funcionament, en `ms`
#define SLEEP_CYCLE_DURATION (uint64_t) MIN_TO_MS(2.5)

// Error del clock, en `ppm`
#define SLEEP_CLOCK_ERROR (0)
// Temps de correcció d'error del clock, en `ms`
#define SLEEP_CLOCK_CORRECTION (int64_t)((SLEEP_CYCLE_DURATION * SLEEP_CLOCK_ERROR / 1000000.0))

// Temps extra d'espera de recepció de SYNC, i previ a primera recepció esperada, en `ms`
// No hauria de ser molt gran; temps tolerància ja considera el temps màxim teòric
#define SLEEP_EXTRA_TIME (uint64_t) S_TO_MS(1) 

// Percentatge addicional que s'aplicarà a temps delta, per considerar CSMA, etc.
#define SLEEP_DELTA_EXTRA 0.25

// Bytes de dades que cada node pot afegir
#define SLEEP_DATASIZE_PER_NODE 1 

// Quantitat de dispositius de la xarxa. Ha de ser igual a major als dispositius reals de la xarxa.
// Es recomana que sigui major per facilitar la tasca d'afegir-ne de nous, i evitar haver de programar-los tots de nou
#define SLEEP_QUANTITAT_DISPOSITIUS 5

// Enum per definir ordres possibles. Definició per ús futur
enum sleep_command_t : uint8_t {
    SLEEP_CMD_NONE,
    SLEEP_CMD_SYNC,                 // Sincronització i inici de cicle
    // SLEEP_CMD_SET_SLEEP_TIME,       // Establir temps sleep
    // SLEEP_CMD_WAKEUP,               // Mantenir despert
};

#define SLEEP_HEADER_SIZE (1 + 1) // 1 de ordre + 1 de mida
#define SLEEP_MAX_DATA_SIZE (TRANSPORT_MAX_DATA_SIZE - SLEEP_HEADER_SIZE) // Màxim de dades que es poden enviar

typedef struct {
    sleep_command_t command;
    uint8_t dataLen = 0;
    uint8_t data[SLEEP_MAX_DATA_SIZE];
} sleep_pdu_t;

typedef void (*sleep_callback_t)(uint8_t* data, size_t dataSize);

/// @brief Inicialitza l'aplicació de SLEEP, inicialitzant capes inferiors.
/// 
/// Si no s'havia sincronitzat mai, espera en recepció indefinidament; 
/// en cas contrari, espera durant 2*`D`, on `D` és el temps màxim de rebre un SYNC si tot falla (reintents màxims en tots els nodes).
/// En sincronitzar-se, executa callback configurat per `Sleep_onSync`. 
/// @return `true` si ha pogut inicialitzar-se, `false` si no.
bool Sleep_init(void);

/// @brief Deixa l'aplicació de sleep en la configuració determinada, deshabilitant totes les seves tasques
void Sleep_deinit(void);

/// @brief  Estableix el node a qui s'han de reenviar les ordres. Retorna 
/// @param node nodes a afegir
/// @return `true` si s'ha pogut establir, `false` si no s'ha pogut establir 
bool Sleep_setForwardNode(node_address_t node);

/// @brief Registrar un callback a executar quan s'ha rebut el missatge de sincronització.
/// 
/// La funció *HA* d'escriure les seves dades, de mida `SLEEP_DATASIZE_PER_NODE`, a
/// l'apuntador de dades passat com a paràmetre.
/// En cas contrari, el funcionament pot no ser el correcte, així com la informació afegida.
/// @param cb Funció a executar, amb la signatura `uint8_t* callback()`
void Sleep_onSync(sleep_callback_t cb);

#endif

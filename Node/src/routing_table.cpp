#include "routing_table.h"
#include "Preferences.h"
#include "utils.h"

static Preferences preferences;

static routing_entry_t* RoutingTable; 
static int RoutingTableSize = 0;

bool RoutingTable_init() {
    // Obtenir rutes de NVS (no volàtil) i copiar-ho a routing table
    // A NVS es guarda com una seqüència de bytes, on el primer byte indica dst i el segon nextHop
    // té una única entrada a la key "routingTable" (max 15 chars). 
    // La mida màxima d'una entrada a NVS en format BLOB 
    // (Binary Large OBject) és "508000 bytes or (97.6% of the partition size - 4000) bytes whichever is lower."
    // així que si cada entrada són 2 byets (dst+nextHop), el màxim de rutes és virtualment "infinit" (125000)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html

    preferences.begin("routingTable");

    // Mida en bytes de la taula de rutes
    int sizeInBytes = preferences.getBytesLength("routingTable");
    // Quantitat d'entrades a la taula de rutes (en funció de mida d'entrada de routing_entry)
    RoutingTableSize = sizeInBytes / sizeof(routing_entry_t);
    // Assignar la memòria necessaria per guardar totes les rutes
    RoutingTable = (routing_entry_t*)malloc(sizeInBytes);
    if (!RoutingTable) {
        _PE("[RTABLE] Error allocating memory for routing table");
        return false;
    }

    // Guardar taules de rutes. El mapeig serà el correcte (és un apuntador amb memòria contigua i mida correcta per malloc)
    int bytesRead = preferences.getBytes("routingTable", RoutingTable, sizeInBytes);
    if (bytesRead != sizeInBytes) {
        _PE("[RTABLE] Error reading routing table from NVS");
        return false;
    }
    _PI("[RTABLE] Routing table initialized. (%d B, %d routes) Heap: %d", sizeInBytes, RoutingTableSize, ESP.getFreeHeap());
    return true;
}

void RoutingTable_deinit() {
    if (RoutingTable) {
        free(RoutingTable);   // Alliberar memory
        RoutingTable = nullptr;  
    }
    RoutingTableSize = 0;  
    preferences.end();
    _PI("[RTABLE] Routing table de-initialized. Heap: %d", ESP.getFreeHeap());
}

void RoutingTable_print() {
    Serial.println("=== RTABLE ===");
    for (int i = 0; i < RoutingTableSize; ++i) {
        Serial.printf(" 0x%02X -> 0x%02X\n", RoutingTable[i].dst, RoutingTable[i].nextHop);
    }
    Serial.println("==============");
}

routing_addr_t RoutingTable_findRoute(routing_addr_t dst) {
    // Iterar totes les entrades fins trobar destí, o retornar 0x00 si no trobada
    for (int i = 0; i < RoutingTableSize; ++i) {
        if(RoutingTable[i].dst == dst) {
            return RoutingTable[i].nextHop;
        }
    }
    return 0x00;
}

bool RoutingTable_addRoute(routing_addr_t dst, routing_addr_t nextHop) {
    if(RoutingTable_findRoute(dst) != 0x00) {
        _PW("[RTABLE] Route for %d already exists", dst);
        return false;
    }

    // Augmentar mida de la taula de rutes
    RoutingTableSize++;
    // Reassignar memòria per la nova entrada
    RoutingTable = (routing_entry_t*)realloc(RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
    if (!RoutingTable) {
        _PE("[RTABLE] Error reallocating memory for routing table");
        return false;
    }

    // Afegir nova ruta
    RoutingTable[RoutingTableSize-1].dst = dst;
    RoutingTable[RoutingTableSize-1].nextHop = nextHop;

    // Guardar taula de rutes a NVS. Fer-ho ara i no en deinit per evitar perdre rutes en cas de crash
    int state = preferences.putBytes("routingTable", (uint8_t*)RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
    _PI("[RTABLE] Route added: dst=%d, nextHop=%d (%d)", dst, nextHop, state);
    return true;
}

bool RoutingTable_removeRoute(routing_addr_t dst) {
    int indexToRemove = -1;
    for (int i = 0; i < RoutingTableSize; ++i) {
        if (RoutingTable[i].dst == dst) {
            indexToRemove = i;
            break;
        }
    }

    if (indexToRemove == -1) {
        _PW("[RTABLE] Route for %d not found", dst);
        return false;
    }

    // Moure les entrades posteriors una posició cap endavant
    for (int i = indexToRemove; i < RoutingTableSize - 1; ++i) {
        RoutingTable[i] = RoutingTable[i + 1];
    }

    // Reduir la mida de la taula de rutes
    RoutingTableSize--;

    // Reassignar memòria
    RoutingTable = (routing_entry_t*)realloc(RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
    if (RoutingTableSize > 0 && !RoutingTable) {
        _PE("[RTABLE] Error reallocating memory for routing table");
        return false;
    }

    // Guardar taula de rutes a NVS
    preferences.putBytes("routingTable", (uint8_t*)RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
    _PI("[RTABLE] Route for %d removed", dst);
    return true;
}

bool RoutingTable_clear() {
    // Alliberar memòria
    free(RoutingTable);
    RoutingTable = nullptr;
    RoutingTableSize = 0;

    // Esborrar taula de rutes de NVS
    preferences.remove("routingTable");
    _PI("[RTABLE] Routing table cleared. Heap: %d", ESP.getFreeHeap());
    return true;
}

bool RoutingTable_Update(routing_addr_t dst, routing_addr_t nextHop) {
    // Actualitza la ruta per `dst`.
    // Si no existeix, la crea. Si existeix, modifica nextHop
    if (!RoutingTable_findRoute(dst)) {
        return RoutingTable_addRoute(dst, nextHop);
    }
    // Existeix -> actualitzar
    for (int i = 0; i < RoutingTableSize; ++i) {
        if (RoutingTable[i].dst == dst) {
            RoutingTable[i].nextHop = nextHop;
            preferences.putBytes("routingTable", (uint8_t*)RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
            _PI("[RTABLE] Route for %d updated. New nextHop: %d", dst, nextHop);
            return true;
        }
    }
}

void setup() {
    Serial.begin(115200);
    preferences.begin("routingTable", false);
    RoutingTable_init();
    RoutingTable_print();
    RoutingTable_addRoute(0x01, 0x02);
    RoutingTable_print();
    RoutingTable_addRoute(0x03, 0x02);
    RoutingTable_print();
    RoutingTable_addRoute(0x04, 0x03);
    RoutingTable_print();
    RoutingTable_removeRoute(0x03);
    RoutingTable_print();
    RoutingTable_deinit();
    RoutingTable_print();
    routing_addr_t nextHop = RoutingTable_findRoute(0x04);
    Serial.printf("Next hop for 0x04: 0x%02X\n", nextHop);
}

void loop() {

}
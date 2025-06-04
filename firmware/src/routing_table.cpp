#include "routing_table.h"
#include "Preferences.h"
#include "utils.h"

static Preferences preferences;

static routing_entry_t* RoutingTable; 
static int RoutingTableSize = 0;

static int _getIndexInRoutingTable(node_address_t dst);
static bool _saveTableToNVS();


bool RoutingTable_init() {
    // Obtenir rutes de NVS (no volàtil) i copiar-ho a routing table
    // A NVS es guarda com una seqüència de bytes, on el primer byte indica dst i el segon nextHop
    // té una única entrada a la key "routingTable" (max 15 chars). 
    // La mida màxima d'una entrada a NVS en format BLOB 
    // (Binary Large OBject) és "508000 bytes or (97.6% of the partition size - 4000) bytes whichever is lower."
    // així que si cada entrada són 2 byets (dst+nextHop), el màxim de rutes és virtualment "infinit" (125000)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html

    if(!preferences.begin("routingTable")){
        _PE("[RTABLE] Error initializing NVS");
        return false;
    }

    // Mida en bytes de la taula de rutes
    int sizeInBytes = preferences.getBytesLength("routingTable");
    // Quantitat d'entrades a la taula de rutes (en funció de mida d'entrada de routing_entry)
    RoutingTableSize = sizeInBytes / sizeof(routing_entry_t);
    if(RoutingTableSize == 0) {
        _PI("[RTABLE] No routing table found in NVS; initializing empty table");
        return true;
    }
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

node_address_t RoutingTable_getRoute(node_address_t dst) {
    int index = _getIndexInRoutingTable(dst);
    if(index != -1) {
        _PI("[RTABLE] Queried route for 0x%02X, through 0x%02X", dst, RoutingTable[index].nextHop);
        return RoutingTable[index].nextHop;
    }
    _PW("[RTABLE] Route for %d not found", dst);
    return NODE_ADDRESS_NULL;
}

bool RoutingTable_addRoute(node_address_t dst, node_address_t nextHop) {
    // Filtrem si ja existeix
    if(_getIndexInRoutingTable(dst) != -1) {
        _PW("[RTABLE] Route for 0x%02X already exists", dst);
        return false;
    }

    // Si no existeix: incrementar mida taula, reassignar memòria de routingtable, guardar registre i guardar a NVS
    RoutingTableSize++;
    RoutingTable = (routing_entry_t*)realloc(RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
    if (!RoutingTable) {
        _PE("[RTABLE] Error reallocating memory for routing table");
        return false;
    }

    // Afegir nova ruta
    RoutingTable[RoutingTableSize-1].dst = dst;
    RoutingTable[RoutingTableSize-1].nextHop = nextHop;

    // Guardar taula de rutes a NVS. Fer-ho ara i no en deinit per evitar perdre rutes en cas de crash
    if(_saveTableToNVS()) {
        _PI("[RTABLE] Route added: 0x%02X -> 0x%02X", dst, nextHop);
        return true;
    }
    return false;
}

bool RoutingTable_removeRoute(node_address_t dst) {
    int indexToRemove = _getIndexInRoutingTable(dst);

    if (indexToRemove == -1) {
        _PW("[RTABLE] Route for 0x%02X not found", dst);
        return false;
    }

    // Moure les entrades posteriors una posició cap endavant
    for (int i = indexToRemove; i < RoutingTableSize - 1; ++i) {
        RoutingTable[i] = RoutingTable[i + 1];
    }

    // Reduir la mida de la taula de rutes, i reassignar memòria sense la nova ruta
    RoutingTableSize--;
    RoutingTable = (routing_entry_t*)realloc(RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
    if (RoutingTableSize > 0 && !RoutingTable) {
        _PE("[RTABLE] Error reallocating memory for routing table");
        return false;
    }

    // Guardar taula de rutes a NVS; fer-ho aquí i no després a deinit per evitar perdre dades si crash
    if(_saveTableToNVS()) {
        _PI("[RTABLE] Route for 0x%02X removed", dst);
        return true;
    }
    return false;
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

bool RoutingTable_updateRoute(node_address_t dst, node_address_t nextHop) {
    // Si no existeix, creem ruta
    int index = _getIndexInRoutingTable(dst);
    if (index == -1) {
        return RoutingTable_addRoute(dst, nextHop);
    }
    // Si existeix, actualitzar nextHop i guardar a NVS
    if(RoutingTable[index].nextHop == nextHop) {
        _PI("[RTABLE] No update for 0x%02X required", dst, nextHop);
        return true;
    }
    RoutingTable[index].nextHop = nextHop;
    if(_saveTableToNVS()) {
        _PI("[RTABLE] Route for 0x%02X updated. New nextHop: 0x%02X", dst, nextHop);
        return true;
    }
    return false;
}

static int _getIndexInRoutingTable(node_address_t dst) {
    for (int i = 0; i < RoutingTableSize; ++i) {
        if (RoutingTable[i].dst == dst) {
            return i;
        }
    }
    return -1;
}

static bool _saveTableToNVS() {
    // Guardar taula de rutes a NVS. Verifica que l'escriptura sigui la correcta
    int bWritten = preferences.putBytes("routingTable", (uint8_t*)RoutingTable, RoutingTableSize * sizeof(routing_entry_t));
    if (bWritten != RoutingTableSize * sizeof(routing_entry_t)) {
        _PE("[RTABLE] Error writing routing table to NVS (Bytes written: %d, expected %d)", bWritten, RoutingTableSize * sizeof(routing_entry_t));
        return false;
    }
    _PI("[RTABLE] Routing table saved to NVS (Bytes written: %d)", bWritten);
    return true;
}

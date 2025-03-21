#include "config_manager.h"

Preferences preferences;

void config_init() {
    preferences.begin("LoRa");
}

void config_deinit() {
    preferences.end();
}

void config_setInt(uint8_t val, const char* key) {
    preferences.putUChar(key, val);
}

uint8_t config_getInt(const char* key) {
    return preferences.getUChar(key, 0);
}
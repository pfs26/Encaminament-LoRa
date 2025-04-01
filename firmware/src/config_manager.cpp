#include <Preferences.h>
#include "config_manager.h"
#include "config.h"

static Preferences preferences;

// Mètode per assegurar que la inicialització de Preferences es fa només una vegada
static void assegurarInit() {
    static bool isInitialized = false;
    if (!isInitialized) {
        preferences.begin("config");
        isInitialized = true;
    }
}

// Node role (client or gateway)
void config_setGateway(bool isGateway) {
    assegurarInit();
    preferences.putBool("is_gateway", isGateway);
}

bool config_getGateway() {
    assegurarInit();
    return preferences.getBool("is_gateway", IS_GATEWAY);
}

// Node self address
void config_setSelfAddress(uint8_t address) {
    assegurarInit();
    preferences.putUChar("self_address", address);
}

uint8_t config_getSelfAddress() {
    assegurarInit();
    return preferences.getUChar("self_address", SELF_ADDRESS);
}

// Spreading factor
void config_setSpreadingFactor(uint8_t factor) {
    assegurarInit();
    preferences.putUChar("spreading_factor", factor);
}

uint8_t config_getSpreadingFactor() {
    assegurarInit();
    return preferences.getUChar("spreading_factor", LORA_SF);
}

// BW
void config_setBandwidth(float bw) {
    assegurarInit();
    preferences.putUChar("bandwidth", bw);
}

float config_getBandwidth() {
    assegurarInit();
    return preferences.getUChar("bandwidth", LORA_BW);
}


// Transmission power
void config_setTransmissionPower(uint8_t power) {
    assegurarInit();
    preferences.putUChar("tx_power", power);
}

uint8_t config_getTransmissionPower() {
    assegurarInit();
    return preferences.getUChar("tx_power", LORA_TX_POW);
}

// Frequency
void config_setFrequency(float frequency) {
    assegurarInit();
    preferences.putUInt("frequency", frequency);
}

float config_getFrequency() {
    assegurarInit();
    return preferences.getUInt("frequency", LORA_FREQ); // Default 868.1 MHz
}

// Coderate
void config_setCoderate(uint8_t coderate) {
    assegurarInit();
    preferences.putUChar("coderate", coderate);
}

uint8_t config_getCoderate() {
    assegurarInit();
    return preferences.getUChar("coderate", LORA_CODERATE);
}

// Coderate
void config_setSyncword(uint8_t syncWord) {
    assegurarInit();
    preferences.putUChar("sync_word", syncWord);
}

uint8_t config_getSyncword() {
    assegurarInit();
    return preferences.getUChar("sync_word", LORA_SYNC_WORD); // Default 0x12
}

// Retry amount
void config_setMacRetryAmount(uint8_t retries) {
    assegurarInit();
    preferences.putUChar("mac_max_retry", retries);
}

uint8_t config_getMacRetryAmount() {
    assegurarInit();
    return preferences.getUChar("mac_max_retry", MAC_MAX_RETRIES); // Default 3 retries
}

// Maximum TTL
void config_setMaxTTL(uint8_t ttl) {
    assegurarInit();
    preferences.putUChar("max_ttl", ttl);
}

uint8_t config_getMaxTTL() {
    assegurarInit();
    return preferences.getUChar("max_ttl", 64); // Default TTL 64
}

// WiFi SSID
void config_setWifiSSID(const char* ssid) {
    assegurarInit();
    preferences.putString("wifi_ssid", ssid);
}

void config_getWifiSSID(char* ssid, size_t len) {
    assegurarInit();
    String storedSSID = preferences.getString("wifi_ssid", "");
    strncpy(ssid, storedSSID.c_str(), len);
}

// WiFi Password
void config_setWifiPassword(const char* password) {
    assegurarInit();
    preferences.putString("wifi_password", password);
}

void config_getWifiPassword(char* password, size_t len) {
    assegurarInit();
    String storedPassword = preferences.getString("wifi_password", "");
    strncpy(password, storedPassword.c_str(), len);
}

// Routing table (cadena de bytes)
void config_setRoutingTable(const uint8_t* table, size_t length) {
    assegurarInit();
    preferences.putBytes("routing_table", table, length);
}

size_t config_getRoutingTable(uint8_t* table, size_t maxLength) {
    assegurarInit();
    return preferences.getBytes("routing_table", table, maxLength);
}
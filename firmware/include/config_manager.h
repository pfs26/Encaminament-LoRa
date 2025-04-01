#ifndef _CONFIG_MANAGER_H
#define _CONFIG_MANAGER_H

#include <stdint.h>

// Node role
void config_setGateway(bool isGateway);
bool config_getGateway();

// Node self address
void config_setSelfAddress(uint8_t address);
uint8_t config_getSelfAddress();

// Spreading factor
void config_setSpreadingFactor(uint8_t factor);
uint8_t config_getSpreadingFactor();

// Bandwidth
void config_setBandwidth(float bw);
float config_getBandwidth();

// Transmission power
void config_setTransmissionPower(uint8_t power);
uint8_t config_getTransmissionPower();

// Frequency
void config_setFrequency(float frequency);
float config_getFrequency();

// Coderate
void config_setCoderate(uint8_t coderate);
uint8_t config_getCoderate();

// Coderate
void config_setSyncword(uint8_t syncWord);
uint8_t config_getSyncword();

// Retry amount
void config_setMacRetryAmount(uint8_t retries);
uint8_t config_getMacRetryAmount();

// Maximum TTL
void config_setMaxTTL(uint8_t ttl);
uint8_t config_getMaxTTL();

// WiFi SSID
void config_setWifiSSID(const char* ssid);
void config_getWifiSSID(char* ssid, size_t len);

// WiFi Password
void config_setWifiPassword(const char* password);
void config_getWifiPassword(char* password, size_t len);

// Routing table
void config_setRoutingTable(const uint8_t* table, size_t length);
size_t config_getRoutingTable(uint8_t* table, size_t maxLength);

#endif
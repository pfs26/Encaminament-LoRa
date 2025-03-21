#ifndef _CONFIG_MANAGER_H
#define _CONFIG_MANAGER_H

#include <Preferences.h>

void config_init();
void config_deinit();
void config_setInt(uint8_t val, const char* key);
uint8_t config_getInt(const char* key);

#endif
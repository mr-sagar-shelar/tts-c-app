#ifndef CONFIG_H
#define CONFIG_H

#include "cJSON.h"

void save_setting(const char *key, const char *value);
char* get_setting(const char *key);
void init_config();
void cleanup_config();

#endif

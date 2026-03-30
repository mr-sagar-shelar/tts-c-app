#ifndef CONFIG_H
#define CONFIG_H

#include "cJSON.h"

/**
 * Initializes the configuration system by loading userConfig.json.
 */
void init_config();

/**
 * Cleans up configuration memory.
 */
void cleanup_config();

/**
 * Saves the current configuration object to userConfig.json.
 */
void save_config();

/**
 * Gets a string setting from the configuration.
 * @return A newly allocated string (must be freed) or NULL.
 */
char* get_setting(const char *key);

/**
 * Saves a string setting to the configuration and persists to disk.
 */
void save_setting(const char *key, const char *value);

/**
 * Returns the root cJSON object for complex data structures (contacts, alarms).
 * Modules should use this to read/write their sub-objects and then call save_config().
 */
cJSON* get_config_root();

#endif /* CONFIG_H */

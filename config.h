#ifndef CONFIG_H
#define CONFIG_H

#include "cJSON.h"

/**
 * Initializes the configuration system.
 * If userConfig.json is missing, attempts to sync from server.
 * If sync fails, initializes with default values.
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
 * Syncs the configuration with the server.
 * Uploads local changes and updates last_sync timestamp.
 */
void sync_config();

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
 */
cJSON* get_config_root();

#endif /* CONFIG_H */

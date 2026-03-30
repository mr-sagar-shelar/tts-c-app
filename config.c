#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "cJSON.h"

#define CONFIG_FILE "userConfig.json"

static cJSON *config_json = NULL;

void init_config() {
    if (config_json) return;

    FILE *f = fopen(CONFIG_FILE, "rb");
    if (!f) {
        config_json = cJSON_CreateObject();
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    if (data) {
        fread(data, 1, len, f);
        data[len] = '\0';
        config_json = cJSON_Parse(data);
        free(data);
    }
    fclose(f);

    if (!config_json) {
        config_json = cJSON_CreateObject();
    }
}

void save_config() {
    if (!config_json) return;
    char *out = cJSON_Print(config_json);
    if (out) {
        FILE *f = fopen(CONFIG_FILE, "w");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
        free(out);
    }
}

void save_setting(const char *key, const char *value) {
    if (!config_json) init_config();

    cJSON *item = cJSON_GetObjectItemCaseSensitive(config_json, key);
    if (item) {
        cJSON_ReplaceItemInObject(config_json, key, cJSON_CreateString(value));
    } else {
        cJSON_AddItemToObject(config_json, key, cJSON_CreateString(value));
    }
    save_config();
}

char* get_setting(const char *key) {
    if (!config_json) init_config();

    cJSON *item = cJSON_GetObjectItemCaseSensitive(config_json, key);
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        return strdup(item->valuestring);
    }
    return NULL;
}

cJSON* get_config_root() {
    if (!config_json) init_config();
    return config_json;
}

void cleanup_config() {
    if (config_json) {
        cJSON_Delete(config_json);
        config_json = NULL;
    }
}

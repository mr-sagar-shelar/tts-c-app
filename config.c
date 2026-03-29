#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "cJSON.h"

#define CONFIG_FILE "userConfig.json"

static cJSON *config_json = NULL;

void init_config() {
    FILE *f = fopen(CONFIG_FILE, "rb");
    if (!f) {
        config_json = cJSON_CreateObject();
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    config_json = cJSON_Parse(data);
    free(data);
    if (!config_json) {
        config_json = cJSON_CreateObject();
    }
}

void save_setting(const char *key, const char *value) {
    if (!config_json) init_config();

    if (cJSON_GetObjectItemCaseSensitive(config_json, key)) {
        cJSON_ReplaceItemInObject(config_json, key, cJSON_CreateString(value));
    } else {
        cJSON_AddItemToObject(config_json, key, cJSON_CreateString(value));
    }

    char *out = cJSON_Print(config_json);
    FILE *f = fopen(CONFIG_FILE, "w");
    if (f) {
        fputs(out, f);
        fclose(f);
    }
    free(out);
}

char* get_setting(const char *key) {
    if (!config_json) init_config();

    cJSON *item = cJSON_GetObjectItemCaseSensitive(config_json, key);
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        return strdup(item->valuestring);
    }
    return NULL;
}

void cleanup_config() {
    if (config_json) {
        cJSON_Delete(config_json);
        config_json = NULL;
    }
}

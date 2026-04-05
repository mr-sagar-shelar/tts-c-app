#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "cJSON.h"
#include "download_ui.h"
#include "menu.h"

#define CONFIG_FILE "userSettings.json"
#define LEGACY_CONFIG_FILE "userConfig.json"
#define SERVER_URL "https://api.example.com/sync/userConfig" // Placeholder URL

static cJSON *config_json = NULL;

static FILE *open_config_file_for_read(void) {
    FILE *file = fopen(CONFIG_FILE, "rb");
    if (file) {
        return file;
    }
    return fopen(LEGACY_CONFIG_FILE, "rb");
}

static void ensure_default_string(const char *key, const char *value) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(config_json, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        if (item) {
            cJSON_ReplaceItemInObject(config_json, key, cJSON_CreateString(value));
        } else {
            cJSON_AddStringToObject(config_json, key, value);
        }
    }
}

static void create_default_config() {
    if (config_json) cJSON_Delete(config_json);
    config_json = cJSON_CreateObject();
    cJSON_AddStringToObject(config_json, "language", "en");
    cJSON_AddStringToObject(config_json, "city", "Pune");
    cJSON_AddStringToObject(config_json, "timezone", "Asia/Kolkata");
    cJSON_AddStringToObject(config_json, "time_format", "12h");
    cJSON_AddStringToObject(config_json, "tts_voice", "slt");
    cJSON_AddStringToObject(config_json, "tts_volume", "medium");
    cJSON_AddStringToObject(config_json, "tts_speed", "1.0");
    cJSON_AddStringToObject(config_json, "speech_mode", "on");
    cJSON_AddStringToObject(config_json, "audio_playback", "off");
    cJSON_AddStringToObject(config_json, "audio_output", "hdmi");
    cJSON_AddItemToObject(config_json, "contacts", cJSON_CreateArray());
    cJSON_AddItemToObject(config_json, "alarms", cJSON_CreateArray());
    cJSON_AddItemToObject(config_json, "database_tables", cJSON_CreateArray());
    cJSON_AddStringToObject(config_json, "last_sync", "Never");
}

static int download_from_server() {
    char error[256] = {0};
    char *data = fetch_text_with_progress_ui("Config Sync", SERVER_URL, "server config", error, sizeof(error));
    if (data) {
        cJSON *parsed = cJSON_Parse(data);
        if (parsed) {
            if (config_json) cJSON_Delete(config_json);
            config_json = parsed;
            free(data);
            return 1;
        }
        free(data);
    }
    return 0;
}

static void upload_to_server() {
    if (!config_json) return;
    
    // Update last_sync timestamp
    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strlen(ts) - 1] = '\0'; // Remove newline
    cJSON_ReplaceItemInObject(config_json, "last_sync", cJSON_CreateString(ts));
    save_config();

    char *data = cJSON_PrintUnformatted(config_json);
    if (data) {
        printf("\nUploading config to server...\n");
        // Mocking upload with curl. In real world, use -X POST or -X PUT
        // and -d @filename or -d "json_data"
        FILE *f = fopen("/tmp/uploadConfig.json", "w");
        if (f) {
            char error[256] = {0};
            fputs(data, f);
            fclose(f);
            if (upload_file_with_progress_ui("Config Sync", SERVER_URL, "/tmp/uploadConfig.json", "configuration", error, sizeof(error))) {
                printf("Sync successful!\n");
            } else {
                printf("%s\n", menu_translate("ui_server_sync_failed_upload", "Server sync failed (Upload). Local changes saved"));
            }
        }
        free(data);
    }
}

void init_config() {
    if (config_json) return;

    FILE *f = open_config_file_for_read();
    if (!f) {
        // File doesn't exist, try to sync from server
        if (!download_from_server()) {
            printf("Server sync failed or no data. Initializing with defaults.\n");
            create_default_config();
        }
        save_config();
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
        create_default_config();
        save_config();
        return;
    }

    ensure_default_string("language", "en");
    ensure_default_string("city", "Pune");
    ensure_default_string("timezone", "Asia/Kolkata");
    ensure_default_string("time_format", "12h");
    ensure_default_string("tts_voice", "slt");
    ensure_default_string("tts_volume", "medium");
    ensure_default_string("tts_speed", "1.0");
    ensure_default_string("speech_mode", "on");
    ensure_default_string("audio_playback", "off");
    ensure_default_string("audio_output", "hdmi");
    ensure_default_string("last_sync", "Never");
    if (!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(config_json, "database_tables"))) {
        cJSON_ReplaceItemInObject(config_json, "database_tables", cJSON_CreateArray());
    }
    save_config();
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

void sync_config() {
    upload_to_server();
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

#include "keys_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "menu.h"
#include "utils.h"

#define KEYS_FILE "keys.json"
#define API_LEAGUE_KEY_NAME "api_league_key"

static cJSON *keys_json = NULL;

static void keys_manager_create_default(void) {
    if (keys_json) {
        cJSON_Delete(keys_json);
    }
    keys_json = cJSON_CreateObject();
    cJSON_AddStringToObject(keys_json, API_LEAGUE_KEY_NAME, "");
}

static void keys_manager_save(void) {
    char *output;
    FILE *file;

    if (!keys_json) {
        keys_manager_create_default();
    }

    output = cJSON_Print(keys_json);
    if (!output) {
        return;
    }

    file = fopen(KEYS_FILE, "w");
    if (file) {
        fputs(output, file);
        fclose(file);
    }
    free(output);
}

void init_keys_manager(void) {
    FILE *file;
    long length;
    char *data;

    if (keys_json) {
        return;
    }

    file = fopen(KEYS_FILE, "rb");
    if (!file) {
        keys_manager_create_default();
        keys_manager_save();
        return;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    data = (char *)malloc((size_t)length + 1);
    if (!data) {
        fclose(file);
        keys_manager_create_default();
        keys_manager_save();
        return;
    }

    fread(data, 1, (size_t)length, file);
    data[length] = '\0';
    fclose(file);

    keys_json = cJSON_Parse(data);
    free(data);

    if (!keys_json || !cJSON_IsObject(keys_json)) {
        keys_manager_create_default();
        keys_manager_save();
        return;
    }

    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(keys_json, API_LEAGUE_KEY_NAME))) {
        cJSON_ReplaceItemInObject(keys_json, API_LEAGUE_KEY_NAME, cJSON_CreateString(""));
        keys_manager_save();
    }
}

void cleanup_keys_manager(void) {
    if (keys_json) {
        cJSON_Delete(keys_json);
        keys_json = NULL;
    }
}

char *keys_manager_get_api_league_key(void) {
    cJSON *item;

    if (!keys_json) {
        init_keys_manager();
    }

    item = cJSON_GetObjectItemCaseSensitive(keys_json, API_LEAGUE_KEY_NAME);
    if (cJSON_IsString(item) && item->valuestring) {
        return strdup(item->valuestring);
    }

    return NULL;
}

void keys_manager_show_menu(void) {
    char *current_key;

    if (!keys_json) {
        init_keys_manager();
    }

    current_key = keys_manager_get_api_league_key();

    while (1) {
        printf("\033[H\033[J--- %s ---\n",
               menu_translate("keys_manager", "Keys Manager"));
        printf("%s: %s\n\n",
               menu_translate("api_league_key_label", "API League Key"),
               (current_key && current_key[0])
                   ? menu_translate("ui_key_saved", "Saved")
                   : menu_translate("ui_not_set", "Not set"));
        printf("%s\n\n",
               menu_translate("keys_manager_help",
                              "Press Enter to set or update the API League key. Press Esc to go back."));
        printf("%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_ESC) {
                break;
            }
            if (key == KEY_ENTER) {
                char input[512];

                get_user_input(input, sizeof(input), "Enter API League Key");
                if (input[0] != '\0') {
                    cJSON_ReplaceItemInObject(keys_json, API_LEAGUE_KEY_NAME, cJSON_CreateString(input));
                    keys_manager_save();
                    free(current_key);
                    current_key = strdup(input);

                    printf("\n%s\n%s",
                           menu_translate("ui_value_saved", "Value saved!"),
                           menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
                    fflush(stdout);
                    read_key();
                }
            }
        }
    }

    free(current_key);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "alarm.h"
#include "utils.h"
#include "cJSON.h"
#include "config.h"

typedef struct {
    int hour;
    int minute;
    int enabled;
    char label[64];
} Alarm;

static cJSON *alarms_json = NULL;

static void load_alarms_from_config() {
    cJSON *root = get_config_root();
    alarms_json = cJSON_GetObjectItemCaseSensitive(root, "alarms");
    if (!alarms_json) {
        alarms_json = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "alarms", alarms_json);
    }
}

static void save_alarms() {
    save_config();
}

static int get_alarm_count() {
    if (!alarms_json) load_alarms_from_config();
    return cJSON_GetArraySize(alarms_json);
}

static void get_alarm_at(int index, Alarm *a) {
    cJSON *item = cJSON_GetArrayItem(alarms_json, index);
    if (item) {
        a->hour = cJSON_GetObjectItemCaseSensitive(item, "hour")->valueint;
        a->minute = cJSON_GetObjectItemCaseSensitive(item, "minute")->valueint;
        a->enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled")->valueint;
        cJSON *lbl = cJSON_GetObjectItemCaseSensitive(item, "label");
        if (lbl && cJSON_IsString(lbl)) {
            strncpy(a->label, lbl->valuestring, 63);
        } else {
            strcpy(a->label, "Alarm");
        }
    }
}

static void add_alarm(Alarm *a) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "hour", a->hour);
    cJSON_AddNumberToObject(item, "minute", a->minute);
    cJSON_AddNumberToObject(item, "enabled", a->enabled);
    cJSON_AddStringToObject(item, "label", a->label);
    cJSON_AddItemToArray(alarms_json, item);
    save_alarms();
}

static void update_alarm(int index, Alarm *a) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "hour", a->hour);
    cJSON_AddNumberToObject(item, "minute", a->minute);
    cJSON_AddNumberToObject(item, "enabled", a->enabled);
    cJSON_AddStringToObject(item, "label", a->label);
    cJSON_ReplaceItemInArray(alarms_json, index, item);
    save_alarms();
}

static void remove_alarm(int index) {
    cJSON_DeleteItemFromArray(alarms_json, index);
    save_alarms();
}

static void alarm_form(Alarm *a, const char *title) {
    int sel = 0;
    const char *options[] = {"Label", "Hour", "Minute", "Enabled", "Save and Back"};
    int num_options = 5;

    while (1) {
        printf("\033[H\033[J--- %s ---\n", title);
        printf("Current: [%s] %02d:%02d (%s)\n", a->label, a->hour, a->minute, a->enabled ? "ON" : "OFF");
        printf("---------------------------\n");
        
        for (int i = 0; i < num_options; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        
        printf("\n[Arrows: Navigate | Enter: Select | Esc: Cancel]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP && sel > 0) {
            sel--;
        } else if (key == KEY_DOWN && sel < num_options - 1) {
            sel++;
        } else if (key == KEY_ENTER) {
            if (sel == 0) { // Label
                get_user_input(a->label, sizeof(a->label), "Enter alarm label");
            } else if (sel == 1) { // Hour
                int val = handle_value_picker("Select Hour", 0, 23, a->hour);
                if (val != -1) a->hour = val;
            } else if (sel == 2) { // Minute
                int val = handle_value_picker("Select Minute", 0, 59, a->minute);
                if (val != -1) a->minute = val;
            } else if (sel == 3) { // Enabled
                a->enabled = !a->enabled;
            } else if (sel == 4) { // Save
                return;
            }
        } else if (key == KEY_ESC) {
            a->hour = -1; // Flag for cancel
            return;
        }
    }
}

void handle_alarm() {
    if (!alarms_json) {
        load_alarms_from_config();
    }

    int sel = 0;
    const char *options[] = {"Add Alarm", "Update Alarm", "Remove Alarm", "Go Back"};
    int num_options = 4;

    while (1) {
        int alarm_count = get_alarm_count();
        printf("\033[H\033[J--- Alarms ---\n");
        if (alarm_count == 0) {
            printf("  (No alarms set)\n");
        } else {
            for (int i = 0; i < alarm_count; i++) {
                Alarm a;
                get_alarm_at(i, &a);
                printf("%d. [%s] %02d:%02d (%s)\n", i + 1, a.label, a.hour, a.minute, a.enabled ? "ON" : "OFF");
            }
        }
        printf("---------------------------\n");
        
        for (int i = 0; i < num_options; i++) {
            if (i == sel) printf("> %s\n", options[i]);
            else printf("  %s\n", options[i]);
        }
        
        printf("\n[Arrows: Navigate | Enter: Select | Shortcuts: a, u, r | Esc: Back]\n");
        fflush(stdout);

        int key = read_key();
        int trigger_sel = -1;

        if (key == KEY_UP && sel > 0) {
            sel--;
        } else if (key == KEY_DOWN && sel < num_options - 1) {
            sel++;
        } else if (key == KEY_ENTER) {
            trigger_sel = sel;
        } else if (key == KEY_ESC) {
            break;
        } else if (tolower(key) == 'a') {
            trigger_sel = 0;
        } else if (tolower(key) == 'u') {
            trigger_sel = 1;
        } else if (tolower(key) == 'r') {
            trigger_sel = 2;
        }

        if (trigger_sel == 0) { // Add
            Alarm a = {0, 0, 1, "Alarm"};
            alarm_form(&a, "Add New Alarm");
            if (a.hour != -1) {
                add_alarm(&a);
                printf("\nAlarm added! Press any key...");
                fflush(stdout); read_key();
            }
        } else if (trigger_sel == 1) { // Update
            if (alarm_count == 0) {
                printf("\nNo alarms to update! Press any key...");
                fflush(stdout); read_key();
                continue;
            }
            char input[16];
            get_user_input(input, sizeof(input), "Enter alarm number to update");
            int idx = atoi(input) - 1;
            if (idx >= 0 && idx < alarm_count) {
                Alarm a;
                get_alarm_at(idx, &a);
                alarm_form(&a, "Update Alarm");
                if (a.hour != -1) {
                    update_alarm(idx, &a);
                    printf("\nAlarm updated! Press any key...");
                    fflush(stdout); read_key();
                }
            } else {
                printf("\nInvalid number! Press any key...");
                fflush(stdout); read_key();
            }
        } else if (trigger_sel == 2) { // Remove
            if (alarm_count == 0) {
                printf("\nNo alarms to remove! Press any key...");
                fflush(stdout); read_key();
                continue;
            }
            char input[16];
            get_user_input(input, sizeof(input), "Enter alarm number to remove");
            int idx = atoi(input) - 1;
            if (idx >= 0 && idx < alarm_count) {
                remove_alarm(idx);
                printf("\nAlarm removed! Press any key...");
                fflush(stdout); read_key();
            } else {
                printf("\nInvalid number! Press any key...");
                fflush(stdout); read_key();
            }
        } else if (trigger_sel == 3) { // Back
            break;
        }
    }
}

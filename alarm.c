#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alarm.h"
#include "utils.h"
#include "cJSON.h"

#define ALARMS_FILE "alarms.json"

typedef struct {
    int hour;
    int minute;
    int enabled;
    char label[64];
} Alarm;

static Alarm *alarms = NULL;
static int alarm_count = 0;

static void load_alarms() {
    FILE *f = fopen(ALARMS_FILE, "rb");
    if (!f) {
        alarm_count = 0;
        alarms = NULL;
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json) {
        alarm_count = 0;
        alarms = NULL;
        return;
    }

    alarm_count = cJSON_GetArraySize(json);
    if (alarm_count > 0) {
        alarms = (Alarm*)malloc(sizeof(Alarm) * alarm_count);
        for (int i = 0; i < alarm_count; i++) {
            cJSON *item = cJSON_GetArrayItem(json, i);
            alarms[i].hour = cJSON_GetObjectItemCaseSensitive(item, "hour")->valueint;
            alarms[i].minute = cJSON_GetObjectItemCaseSensitive(item, "minute")->valueint;
            alarms[i].enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled")->valueint;
            cJSON *lbl = cJSON_GetObjectItemCaseSensitive(item, "label");
            if (lbl && cJSON_IsString(lbl)) {
                strncpy(alarms[i].label, lbl->valuestring, 63);
            } else {
                strcpy(alarms[i].label, "Alarm");
            }
        }
    }
    cJSON_Delete(json);
}

static void save_alarms() {
    cJSON *json = cJSON_CreateArray();
    for (int i = 0; i < alarm_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "hour", alarms[i].hour);
        cJSON_AddNumberToObject(item, "minute", alarms[i].minute);
        cJSON_AddNumberToObject(item, "enabled", alarms[i].enabled);
        cJSON_AddStringToObject(item, "label", alarms[i].label);
        cJSON_AddItemToArray(json, item);
    }
    char *out = cJSON_Print(json);
    FILE *f = fopen(ALARMS_FILE, "w");
    if (f) {
        fputs(out, f);
        fclose(f);
    }
    free(out);
    cJSON_Delete(json);
}

static void add_alarm(Alarm *a) {
    alarm_count++;
    alarms = (Alarm*)realloc(alarms, sizeof(Alarm) * alarm_count);
    alarms[alarm_count - 1] = *a;
    save_alarms();
}

static void update_alarm(int index, Alarm *a) {
    if (index >= 0 && index < alarm_count) {
        alarms[index] = *a;
        save_alarms();
    }
}

static void remove_alarm(int index) {
    if (index >= 0 && index < alarm_count) {
        for (int i = index; i < alarm_count - 1; i++) {
            alarms[i] = alarms[i + 1];
        }
        alarm_count--;
        if (alarm_count > 0) {
            alarms = (Alarm*)realloc(alarms, sizeof(Alarm) * alarm_count);
        } else {
            free(alarms);
            alarms = NULL;
        }
        save_alarms();
    }
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
    if (alarms == NULL && alarm_count == 0) {
        load_alarms();
    }

    int sel = 0;
    const char *options[] = {"Add Alarm", "Update Alarm", "Remove Alarm", "Go Back"};
    int num_options = 4;

    while (1) {
        printf("\033[H\033[J--- Alarms ---\n");
        if (alarm_count == 0) {
            printf("  (No alarms set)\n");
        } else {
            for (int i = 0; i < alarm_count; i++) {
                printf("%d. [%s] %02d:%02d (%s)\n", i + 1, alarms[i].label, alarms[i].hour, alarms[i].minute, alarms[i].enabled ? "ON" : "OFF");
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
                Alarm a = alarms[idx];
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

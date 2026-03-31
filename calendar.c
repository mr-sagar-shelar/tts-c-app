#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "calendar.h"
#include "cJSON.h"
#include "menu.h"
#include "utils.h"
#include "config.h"

static cJSON *calendar_json = NULL;

void init_calendar() {
    cJSON *root = get_config_root();
    calendar_json = cJSON_GetObjectItemCaseSensitive(root, "calendar");
    if (!calendar_json) {
        calendar_json = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "calendar", calendar_json);
    }
}

void cleanup_calendar() {
    // calendar_json is part of config_root, cleaned up by cleanup_config()
    calendar_json = NULL;
}

void save_calendar() {
    save_config();
}

void add_event(CalendarEvent *e) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "date", e->date);
    cJSON_AddStringToObject(item, "time", e->time);
    cJSON_AddStringToObject(item, "title", e->title);
    cJSON_AddStringToObject(item, "description", e->description);
    cJSON_AddBoolToObject(item, "reminder", e->reminder);
    cJSON_AddItemToArray(calendar_json, item);
    save_calendar();
}

void edit_event(int index, CalendarEvent *e) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "date", e->date);
    cJSON_AddStringToObject(item, "time", e->time);
    cJSON_AddStringToObject(item, "title", e->title);
    cJSON_AddStringToObject(item, "description", e->description);
    cJSON_AddBoolToObject(item, "reminder", e->reminder);
    cJSON_ReplaceItemInArray(calendar_json, index, item);
    save_calendar();
}

void delete_event(int index) {
    cJSON_DeleteItemFromArray(calendar_json, index);
    save_calendar();
}

int get_event_count() {
    return calendar_json ? cJSON_GetArraySize(calendar_json) : 0;
}

CalendarEvent* get_event(int index) {
    cJSON *item = cJSON_GetArrayItem(calendar_json, index);
    if (!item) return NULL;

    static CalendarEvent e;
    memset(&e, 0, sizeof(CalendarEvent));
    
    cJSON *dt = cJSON_GetObjectItemCaseSensitive(item, "date");
    cJSON *tm = cJSON_GetObjectItemCaseSensitive(item, "time");
    cJSON *tl = cJSON_GetObjectItemCaseSensitive(item, "title");
    cJSON *ds = cJSON_GetObjectItemCaseSensitive(item, "description");
    cJSON *rm = cJSON_GetObjectItemCaseSensitive(item, "reminder");

    if (dt && cJSON_IsString(dt)) strncpy(e.date, dt->valuestring, 15);
    if (tm && cJSON_IsString(tm)) strncpy(e.time, tm->valuestring, 15);
    if (tl && cJSON_IsString(tl)) strncpy(e.title, tl->valuestring, 127);
    if (ds && cJSON_IsString(ds)) strncpy(e.description, ds->valuestring, 255);
    if (rm) e.reminder = cJSON_IsTrue(rm);

    return &e;
}

void event_form(CalendarEvent *e, int is_edit) {
    int field = 0;
    const char *labels[] = {"Date (YYYY-MM-DD)", "Time (HH:MM)", "Title", "Description", "Reminder (0/1)"};
    char reminder_str[2] = {0};
    reminder_str[0] = e->reminder ? '1' : '0';
    
    char *buffers[] = {e->date, e->time, e->title, e->description, reminder_str};
    int sizes[] = {16, 16, 128, 256, 2};

    while (1) {
        printf("\033[H\033[J");
        printf("--- %s Event (Tab to navigate, Enter to Save, Esc to Cancel) ---\n", is_edit ? "Edit" : "Add");
        for (int i = 0; i < 5; i++) {
            if (i == field) printf("> %s: %s\n", labels[i], buffers[i]);
            else printf("  %s: %s\n", labels[i], buffers[i]);
        }
        fflush(stdout);

        int key = read_key();
        if (key == KEY_TAB || key == KEY_DOWN) {
            field = (field + 1) % 5;
        } else if (key == KEY_UP) {
            field = (field + 4) % 5;
        } else if (key == KEY_ENTER) {
            e->reminder = (reminder_str[0] == '1');
            return;
        } else if (key == KEY_ESC) {
            memset(e, 0, sizeof(CalendarEvent));
            return;
        } else if (key == KEY_BACKSPACE) {
            int len = (int)strlen(buffers[field]);
            if (len > 0) buffers[field][len-1] = '\0';
        } else if (key > 0 && key < 1000 && isprint(key)) {
            int len = (int)strlen(buffers[field]);
            if (len < sizes[field] - 1) {
                if (field == 4) { // Reminder field only accepts 0 or 1
                    if (key == '0' || key == '1') {
                        buffers[field][0] = (char)key;
                        buffers[field][1] = '\0';
                    }
                } else {
                    buffers[field][len] = (char)key;
                    buffers[field][len+1] = '\0';
                }
            }
        }
    }
}

void handle_calendar(MenuNode *node) {
    if (strcmp(node->key, "calendar_list") == 0) {
        int count = get_event_count();
        if (count == 0) {
            printf("\033[H\033[J--- Calendar ---\n%s", menu_translate("ui_no_events_found_press_any_key", "No events found. Press any key..."));
            fflush(stdout); read_key();
            return;
        }
        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Calendar Events List ---\n");
            for (int i = 0; i < count; i++) {
                CalendarEvent *e = get_event(i);
                if (i == sel) printf("> %s %s: %s\n", e->date, e->time, e->title);
                else printf("  %s %s: %s\n", e->date, e->time, e->title);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < count - 1) sel++;
            else if (key == KEY_ENTER) {
                CalendarEvent *e = get_event(sel);
                printf("\033[H\033[J--- Event Details ---\n");
                printf("Date: %s\nTime: %s\nTitle: %s\nDescription: %s\nReminder: %s\n\nPress any key...", 
                    e->date, e->time, e->title, e->description, e->reminder ? "Yes" : "No");
                fflush(stdout); read_key();
            } else if (key == KEY_ESC) break;
        }
    } else if (strcmp(node->key, "calendar_search") == 0) {
        char query[256];
        get_user_input(query, sizeof(query), menu_translate("ui_enter_search_term", "Enter search term"));
        if (strlen(query) == 0) return;

        int total_count = get_event_count();
        int matches[256];
        int match_count = 0;

        char lower_query[256] = {0};
        for(int i=0; query[i] && i < 255; i++) lower_query[i] = (char)tolower(query[i]);

        for (int i = 0; i < total_count && match_count < 256; i++) {
            CalendarEvent *e = get_event(i);
            char full_data[1024];
            snprintf(full_data, sizeof(full_data), "%s %s %s %s", 
                e->date, e->time, e->title, e->description);
            
            for(int j=0; full_data[j]; j++) full_data[j] = (char)tolower(full_data[j]);

            if (strstr(full_data, lower_query) != NULL) {
                matches[match_count++] = i;
            }
        }

        if (match_count == 0) {
            printf("\n%s", menu_translate("ui_no_events_found_press_any_key", "No events found. Press any key...")); fflush(stdout); read_key();
            return;
        }

        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Search Results: %s ---\n", query);
            for (int i = 0; i < match_count; i++) {
                CalendarEvent *e = get_event(matches[i]);
                if (i == sel) printf("> %s %s: %s\n", e->date, e->time, e->title);
                else printf("  %s %s: %s\n", e->date, e->time, e->title);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < match_count - 1) sel++;
            else if (key == KEY_ENTER) {
                CalendarEvent *e = get_event(matches[sel]);
                printf("\033[H\033[J--- Event Details ---\n");
                printf("Date: %s\nTime: %s\nTitle: %s\nDescription: %s\nReminder: %s\n\nPress any key...", 
                    e->date, e->time, e->title, e->description, e->reminder ? "Yes" : "No");
                fflush(stdout); read_key();
            } else if (key == KEY_ESC) break;
        }
    } else if (strcmp(node->key, "calendar_add") == 0) {
        CalendarEvent e; memset(&e, 0, sizeof(CalendarEvent));
        event_form(&e, 0);
        if (strlen(e.date) > 0 && strlen(e.title) > 0) {
            add_event(&e);
            printf("\nEvent added! Press any key..."); fflush(stdout); read_key();
        }
    } else if (strcmp(node->key, "calendar_edit") == 0) {
        int count = get_event_count();
        if (count == 0) return;
        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Select Event to Edit ---\n");
            for (int i = 0; i < count; i++) {
                CalendarEvent *e = get_event(i);
                if (i == sel) printf("> %s - %s\n", e->date, e->title);
                else printf("  %s - %s\n", e->date, e->title);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < count - 1) sel++;
            else if (key == KEY_ENTER) {
                CalendarEvent *e = get_event(sel);
                CalendarEvent updated = *e;
                event_form(&updated, 1);
                if (strlen(updated.date) > 0 && strlen(updated.title) > 0) {
                    edit_event(sel, &updated);
                    printf("\nEvent updated! Press any key..."); fflush(stdout); read_key();
                }
                break;
            } else if (key == KEY_ESC) break;
        }
    } else if (strcmp(node->key, "calendar_delete") == 0) {
        int count = get_event_count();
        if (count == 0) return;
        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Select Event to Delete ---\n");
            for (int i = 0; i < count; i++) {
                CalendarEvent *e = get_event(i);
                if (i == sel) printf("> %s - %s\n", e->date, e->title);
                else printf("  %s - %s\n", e->date, e->title);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < count - 1) sel++;
            else if (key == KEY_ENTER) {
                printf("\nDelete this event? (y/n)"); fflush(stdout);
                int k = read_key();
                if (k == 'y' || k == 'Y') {
                    delete_event(sel);
                    printf("\nDeleted. Press any key..."); fflush(stdout); read_key();
                }
                break;
            } else if (key == KEY_ESC) break;
        }
    }
}

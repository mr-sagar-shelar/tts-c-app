#include "todo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "cJSON.h"
#include "entertainment.h"
#include "menu.h"
#include "menu_audio.h"
#include "utils.h"

#define TODO_FILE "todo.json"
#define TODO_MAX_ITEMS 512

static cJSON *todo_root = NULL;

static int todo_contains_text_ci(const char *haystack, const char *needle) {
    size_t hay_len;
    size_t needle_len;
    size_t i;

    if (!needle || needle[0] == '\0') {
        return 1;
    }
    if (!haystack) {
        return 0;
    }

    hay_len = strlen(haystack);
    needle_len = strlen(needle);
    if (needle_len > hay_len) {
        return 0;
    }

    for (i = 0; i + needle_len <= hay_len; i++) {
        size_t j;
        for (j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }

    return 0;
}

static void todo_create_default(void) {
    if (todo_root) {
        cJSON_Delete(todo_root);
    }
    todo_root = cJSON_CreateObject();
    cJSON_AddItemToObject(todo_root, "items", cJSON_CreateArray());
}

static cJSON *todo_items(void) {
    cJSON *items;

    if (!todo_root) {
        todo_create_default();
    }

    items = cJSON_GetObjectItemCaseSensitive(todo_root, "items");
    if (!cJSON_IsArray(items)) {
        items = cJSON_CreateArray();
        cJSON_ReplaceItemInObject(todo_root, "items", items);
    }
    return items;
}

static void todo_save(void) {
    char *output;
    FILE *file;

    if (!todo_root) {
        todo_create_default();
    }

    output = cJSON_Print(todo_root);
    if (!output) {
        return;
    }

    file = fopen(TODO_FILE, "w");
    if (file) {
        fputs(output, file);
        fclose(file);
    }
    free(output);
}

void init_todo(void) {
    FILE *file;
    long length;
    char *data;

    if (todo_root) {
        return;
    }

    file = fopen(TODO_FILE, "rb");
    if (!file) {
        todo_create_default();
        todo_save();
        return;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    data = (char *)malloc((size_t)length + 1);
    if (!data) {
        fclose(file);
        todo_create_default();
        todo_save();
        return;
    }

    fread(data, 1, (size_t)length, file);
    data[length] = '\0';
    fclose(file);

    todo_root = cJSON_Parse(data);
    free(data);

    if (!todo_root || !cJSON_IsObject(todo_root)) {
        todo_create_default();
        todo_save();
        return;
    }

    todo_items();
    todo_save();
}

void cleanup_todo(void) {
    if (todo_root) {
        cJSON_Delete(todo_root);
        todo_root = NULL;
    }
}

static void todo_show_message(const char *title, const char *message) {
    printf("\033[H\033[J--- %s ---\n\n%s\n\n%s",
           title,
           message,
           menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);
    read_key();
}

static const char *todo_item_title(cJSON *item) {
    cJSON *title = cJSON_GetObjectItemCaseSensitive(item, "title");
    return cJSON_IsString(title) && title->valuestring ? title->valuestring : "Untitled";
}

static const char *todo_item_details(cJSON *item) {
    cJSON *details = cJSON_GetObjectItemCaseSensitive(item, "details");
    return cJSON_IsString(details) && details->valuestring ? details->valuestring : "";
}

static const char *todo_item_created_at(cJSON *item) {
    cJSON *created_at = cJSON_GetObjectItemCaseSensitive(item, "created_at");
    return cJSON_IsString(created_at) && created_at->valuestring ? created_at->valuestring : "";
}

static int todo_item_completed(cJSON *item) {
    cJSON *completed = cJSON_GetObjectItemCaseSensitive(item, "completed");
    return cJSON_IsBool(completed) ? cJSON_IsTrue(completed) : 0;
}

static void todo_build_summary(cJSON *item, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "[%c] %s",
             todo_item_completed(item) ? 'x' : ' ',
             todo_item_title(item));
}

static void todo_build_detail_text(cJSON *item, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size,
             "Title: %s\nStatus: %s\nCreated: %s\n\nDetails:\n%s",
             todo_item_title(item),
             todo_item_completed(item) ? "Completed" : "Pending",
             todo_item_created_at(item)[0] ? todo_item_created_at(item) : "Not available",
             todo_item_details(item)[0] ? todo_item_details(item) : "No details");
}

static int todo_pick_from_indexes(const char *title, int *indexes, int count) {
    int selected = 0;
    int last_spoken = -1;

    if (count <= 0) {
        todo_show_message(title, "No todo items found.");
        return -1;
    }

    while (1) {
        int i;

        printf("\033[H\033[J--- %s ---\n", title);
        for (i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(todo_items(), indexes[i]);
            char summary[512];

            todo_build_summary(item, summary, sizeof(summary));
            printf("%s%s\n", i == selected ? "> " : "  ", summary);
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        if (selected != last_spoken) {
            cJSON *item = cJSON_GetArrayItem(todo_items(), indexes[selected]);
            char summary[512];
            todo_build_summary(item, summary, sizeof(summary));
            menu_audio_speak(summary);
            last_spoken = selected;
        }

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, count);
            } else if (key == KEY_ENTER) {
                menu_audio_stop();
                return indexes[selected];
            } else if (key == KEY_ESC) {
                menu_audio_stop();
                return -1;
            }
        }
    }
}

static int todo_collect_indexes(const char *query, int *indexes, int max_indexes) {
    int count = cJSON_GetArraySize(todo_items());
    int found = 0;
    int i;

    for (i = 0; i < count && found < max_indexes; i++) {
        cJSON *item = cJSON_GetArrayItem(todo_items(), i);
        const char *title = todo_item_title(item);
        const char *details = todo_item_details(item);

        if (!query || query[0] == '\0' ||
            todo_contains_text_ci(title, query) ||
            todo_contains_text_ci(details, query)) {
            indexes[found++] = i;
        }
    }

    return found;
}

static void todo_show_item(cJSON *item) {
    char detail[2048];
    todo_build_detail_text(item, detail, sizeof(detail));
    content_ui_show_spoken_text(todo_item_title(item), todo_item_title(item), detail);
}

static void todo_list_items(void) {
    int indexes[TODO_MAX_ITEMS];
    int count = todo_collect_indexes(NULL, indexes, TODO_MAX_ITEMS);
    int index = todo_pick_from_indexes("Todo Items", indexes, count);

    if (index >= 0) {
        todo_show_item(cJSON_GetArrayItem(todo_items(), index));
    }
}

static void todo_search_items(void) {
    char query[256];
    int indexes[TODO_MAX_ITEMS];
    int count;
    int index;

    get_user_input(query, sizeof(query), menu_translate("ui_enter_search_term", "Enter search term"));
    if (query[0] == '\0') {
        return;
    }

    count = todo_collect_indexes(query, indexes, TODO_MAX_ITEMS);
    index = todo_pick_from_indexes("Search Todo", indexes, count);
    if (index >= 0) {
        todo_show_item(cJSON_GetArrayItem(todo_items(), index));
    }
}

static void todo_add_item(void) {
    char title[256];
    char details[1024];
    char timestamp[64];
    time_t now;
    struct tm tm_value;
    cJSON *item;

    get_user_input(title, sizeof(title), "Enter todo title");
    if (title[0] == '\0') {
        return;
    }

    get_user_input(details, sizeof(details), "Enter todo details");

    now = time(NULL);
    localtime_r(&now, &tm_value);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_value);

    item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "title", title);
    cJSON_AddStringToObject(item, "details", details);
    cJSON_AddBoolToObject(item, "completed", 0);
    cJSON_AddStringToObject(item, "created_at", timestamp);
    cJSON_AddItemToArray(todo_items(), item);
    todo_save();

    todo_show_message("Todo App", "Todo item added.");
}

static int todo_pick_any_item(const char *title) {
    int indexes[TODO_MAX_ITEMS];
    int count = todo_collect_indexes(NULL, indexes, TODO_MAX_ITEMS);
    return todo_pick_from_indexes(title, indexes, count);
}

static void todo_edit_item(void) {
    int index = todo_pick_any_item("Edit Todo");
    cJSON *item;

    if (index < 0) {
        return;
    }

    item = cJSON_GetArrayItem(todo_items(), index);
    if (item) {
        char title[256];
        char details[1024];

        snprintf(title, sizeof(title), "%s", todo_item_title(item));
        snprintf(details, sizeof(details), "%s", todo_item_details(item));

        get_user_input(title, sizeof(title), "Edit todo title");
        if (title[0] == '\0') {
            return;
        }
        get_user_input(details, sizeof(details), "Edit todo details");

        cJSON_ReplaceItemInObject(item, "title", cJSON_CreateString(title));
        cJSON_ReplaceItemInObject(item, "details", cJSON_CreateString(details));
        todo_save();
        todo_show_message("Todo App", "Todo item updated.");
    }
}

static void todo_toggle_complete(int complete) {
    int index = todo_pick_any_item(complete ? "Mark Complete" : "Mark Pending");
    cJSON *item;

    if (index < 0) {
        return;
    }

    item = cJSON_GetArrayItem(todo_items(), index);
    if (!item) {
        return;
    }

    cJSON_ReplaceItemInObject(item, "completed", cJSON_CreateBool(complete));
    todo_save();
    todo_show_message("Todo App", complete ? "Todo marked complete." : "Todo marked pending.");
}

static void todo_delete_item(void) {
    int index = todo_pick_any_item("Delete Todo");

    if (index < 0) {
        return;
    }

    printf("\033[H\033[J--- Delete Todo ---\n\nDelete \"%s\"? (y/n)",
           todo_item_title(cJSON_GetArrayItem(todo_items(), index)));
    fflush(stdout);

    {
        int key = read_key();
        if (key == 'y' || key == 'Y') {
            cJSON_DeleteItemFromArray(todo_items(), index);
            todo_save();
            todo_show_message("Todo App", "Todo item deleted.");
        }
    }
}

void handle_todo_app(MenuNode *node) {
    if (strcmp(node->key, "todo_list") == 0) {
        todo_list_items();
    } else if (strcmp(node->key, "todo_search") == 0) {
        todo_search_items();
    } else if (strcmp(node->key, "todo_add") == 0) {
        todo_add_item();
    } else if (strcmp(node->key, "todo_edit") == 0) {
        todo_edit_item();
    } else if (strcmp(node->key, "todo_mark_complete") == 0) {
        todo_toggle_complete(1);
    } else if (strcmp(node->key, "todo_mark_pending") == 0) {
        todo_toggle_complete(0);
    } else if (strcmp(node->key, "todo_delete") == 0) {
        todo_delete_item();
    } else if (strcmp(node->key, "todo_open") == 0) {
        int index = todo_pick_any_item("Open Todo");
        if (index >= 0) {
            todo_show_item(cJSON_GetArrayItem(todo_items(), index));
        }
    }
}

#include "menu.h"
#include "config.h"
#include "platform_ops.h"
#include "speech_settings.h"
#include "utils.h"

static char *current_lang_code = NULL;
static cJSON *current_lang_json = NULL;

static char *menu_selected_value_label(MenuNode *node) {
    char *value;

    if (!node) {
        return NULL;
    }

    value = speech_settings_get_selected_label(node->key);
    if (value) {
        return value;
    }

    if (strcmp(node->key, "language_switch") == 0) {
        char *language = get_setting("language");
        if (!language) {
            return strdup(menu_translate("ui_language_english", "English"));
        }
        if (strcmp(language, "hi") == 0) {
            free(language);
            return strdup(menu_translate("ui_language_hindi", "Hindi"));
        }
        free(language);
        return strdup(menu_translate("ui_language_english", "English"));
    }

    if (strcmp(node->key, "set_volume") == 0) {
        int percent = 0;
        char message[128];
        char label[32];

        if (platform_ops_get_system_volume_percent(&percent, message, sizeof(message))) {
            snprintf(label, sizeof(label), "%d%%", percent);
            return strdup(label);
        }
    }

    return NULL;
}

int is_menu_visible(MenuNode *node, const char *current_lang) {
    if (!node) return 0;
    if (node->num_languages == 0) return 1;
    if (!current_lang) return 1;

    for (int i = 0; i < node->num_languages; i++) {
        if (strcmp(node->languages[i], current_lang) == 0) {
            return 1;
        }
    }
    return 0;
}

static MenuNode* parse_json_to_menu(cJSON *json, MenuNode *parent) {
    if (!json) return NULL;

    MenuNode *node = (MenuNode*)malloc(sizeof(MenuNode));
    cJSON *title_key = cJSON_GetObjectItemCaseSensitive(json, "title_key");
    cJSON *key = cJSON_GetObjectItemCaseSensitive(json, "key");
    cJSON *description_key = cJSON_GetObjectItemCaseSensitive(json, "description_key");
    cJSON *shortcut = cJSON_GetObjectItemCaseSensitive(json, "shortcut");
    cJSON *languages = cJSON_GetObjectItemCaseSensitive(json, "languages");
    
    node->title_key = title_key ? strdup(title_key->valuestring) : strdup("Untitled");
    node->key = key ? strdup(key->valuestring) : strdup("no_key");
    node->description_key = description_key ? strdup(description_key->valuestring) : NULL;
    node->shortcut = (shortcut && shortcut->valuestring && strlen(shortcut->valuestring) > 0) ? shortcut->valuestring[0] : 0;
    node->title = strdup(node->title_key); // Default to key until translated
    node->description = node->description_key ? strdup(node->description_key) : NULL;
    node->parent = parent;
    node->items = NULL;
    node->num_items = 0;
    node->languages = NULL;
    node->num_languages = 0;

    if (cJSON_IsArray(languages)) {
        node->num_languages = cJSON_GetArraySize(languages);
        if (node->num_languages > 0) {
            node->languages = (char**)malloc(node->num_languages * sizeof(char*));
            for (int i = 0; i < node->num_languages; i++) {
                cJSON *lang = cJSON_GetArrayItem(languages, i);
                node->languages[i] = strdup(lang->valuestring);
            }
        }
    }

    cJSON *items = cJSON_GetObjectItemCaseSensitive(json, "items");
    if (cJSON_IsArray(items)) {
        node->num_items = cJSON_GetArraySize(items);
        node->items = (MenuNode**)malloc(node->num_items * sizeof(MenuNode*));
        for (int i = 0; i < node->num_items; i++) {
            node->items[i] = parse_json_to_menu(cJSON_GetArrayItem(items, i), node);
        }
    }

    return node;
}

MenuNode* load_menu_from_json(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json) return NULL;

    MenuNode *root = parse_json_to_menu(json, NULL);
    cJSON_Delete(json);

    return root;
}

void free_menu(MenuNode *node) {
    if (!node) return;

    for (int i = 0; i < node->num_items; i++) {
        free_menu(node->items[i]);
    }
    if (node->items) free(node->items);
    
    for (int i = 0; i < node->num_languages; i++) {
        free(node->languages[i]);
    }
    if (node->languages) free(node->languages);

    free(node->title);
    free(node->title_key);
    if (node->description) free(node->description);
    if (node->description_key) free(node->description_key);
    free(node->key);
    free(node);
}

void print_menu(MenuNode *node, int selected_index) {
    char *selected_value = menu_selected_value_label(node);
    int lines_used = 0;

    // Clear screen (ANSI escape code)
    printf("\033[H\033[J");
    lines_used += print_memory_widget_line();
    printf("--- %s ---\n", node->title);
    lines_used++;
    if (selected_value) {
        printf("%s: %s\n\n", menu_translate("ui_selected_value", "Selected Value"), selected_value);
        free(selected_value);
        lines_used += 2;
    } else {
        printf("\n");
        lines_used++;
    }
    
    int visible_count = 0;
    for (int i = 0; i < node->num_items; i++) {
        if (is_menu_visible(node->items[i], current_lang_code)) {
            visible_count++;
        }
    }

    if (visible_count == 0) {
        printf("  (%s)\n", menu_translate("ui_no_items_available_in_language", "No items available in this language"));
        lines_used++;
    } else {
        int current_visible_idx = 0;
        for (int i = 0; i < node->num_items; i++) {
            if (!is_menu_visible(node->items[i], current_lang_code)) continue;

            char shortcut_str[8] = "";
            if (node->items[i]->shortcut) {
                snprintf(shortcut_str, sizeof(shortcut_str), "(%c) ", node->items[i]->shortcut);
            }
            if (current_visible_idx == selected_index) {
                printf("> %s%s\n", shortcut_str, node->items[i]->title);
            } else {
                printf("  %s%s\n", shortcut_str, node->items[i]->title);
            }
            lines_used++;
            current_visible_idx++;
        }
    }
    pad_screen_to_footer(lines_used, 2);
    printf("\n%s\n", menu_translate("ui_footer_menu_info", "[Arrows: Navigate | Enter: Select | Esc: Back/Exit | Ctrl+I: Info]"));
}

void print_description(MenuNode *node) {
    if (!node) return;
    printf("\033[H\033[J");
    printf("--- Information: %s ---\n\n", node->title);
    if (node->description) {
        printf("%s\n", node->description);
    } else {
        printf("%s\n", menu_translate("ui_no_description_available", "No description available."));
    }
    printf("\n\n%s", menu_translate("ui_press_esc_to_return", "Press Esc to return to menu..."));
    fflush(stdout);
    while (read_key() != KEY_ESC);
}

const char *menu_translate(const char *key, const char *fallback) {
    cJSON *translated;

    if (!key || !current_lang_json) {
        return fallback;
    }

    translated = cJSON_GetObjectItemCaseSensitive(current_lang_json, key);
    if (translated && cJSON_IsString(translated) && translated->valuestring) {
        return translated->valuestring;
    }

    return fallback;
}

static void update_titles_recursive(MenuNode *node, cJSON *lang_json) {
    if (!node) return;
    
    cJSON *translated = cJSON_GetObjectItemCaseSensitive(lang_json, node->title_key);
    if (translated && cJSON_IsString(translated)) {
        free(node->title);
        node->title = strdup(translated->valuestring);
    }

    if (node->description_key) {
        cJSON *desc_translated = cJSON_GetObjectItemCaseSensitive(lang_json, node->description_key);
        if (desc_translated && cJSON_IsString(desc_translated)) {
            if (node->description) free(node->description);
            node->description = strdup(desc_translated->valuestring);
        }
    }
    
    for (int i = 0; i < node->num_items; i++) {
        update_titles_recursive(node->items[i], lang_json);
    }
}

void set_language(MenuNode *root, const char *lang_code) {
    if (current_lang_code) free(current_lang_code);
    current_lang_code = strdup(lang_code);

    char filename[16];
    snprintf(filename, sizeof(filename), "%s.json", lang_code);
    
    FILE *f = fopen(filename, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *lang_json = cJSON_Parse(data);
    free(data);
    if (!lang_json) return;

    if (current_lang_json) {
        cJSON_Delete(current_lang_json);
    }
    current_lang_json = lang_json;
    update_titles_recursive(root, lang_json);
}

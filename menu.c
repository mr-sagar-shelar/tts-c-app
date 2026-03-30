#include "menu.h"

static char *current_lang_code = NULL;

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
    cJSON *shortcut = cJSON_GetObjectItemCaseSensitive(json, "shortcut");
    cJSON *languages = cJSON_GetObjectItemCaseSensitive(json, "languages");
    
    node->title_key = title_key ? strdup(title_key->valuestring) : strdup("Untitled");
    node->key = key ? strdup(key->valuestring) : strdup("no_key");
    node->shortcut = (shortcut && shortcut->valuestring && strlen(shortcut->valuestring) > 0) ? shortcut->valuestring[0] : 0;
    node->title = strdup(node->title_key); // Default to key until translated
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
    free(node->key);
    free(node);
}

void print_menu(MenuNode *node, int selected_index) {
    // Clear screen (ANSI escape code)
    printf("\033[H\033[J");
    printf("--- %s ---\n", node->title);
    
    int visible_count = 0;
    for (int i = 0; i < node->num_items; i++) {
        if (is_menu_visible(node->items[i], current_lang_code)) {
            visible_count++;
        }
    }

    if (visible_count == 0) {
        printf("  (No items available in this language)\n");
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
            current_visible_idx++;
        }
    }
    printf("\n[Arrows: Navigate | Enter: Select | Esc: Back/Exit | Shortcut Keys]\n");
}

static void update_titles_recursive(MenuNode *node, cJSON *lang_json) {
    if (!node) return;
    
    cJSON *translated = cJSON_GetObjectItemCaseSensitive(lang_json, node->title_key);
    if (translated && cJSON_IsString(translated)) {
        free(node->title);
        node->title = strdup(translated->valuestring);
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

    update_titles_recursive(root, lang_json);
    cJSON_Delete(lang_json);
}

#include "menu.h"

static MenuNode* parse_json_to_menu(cJSON *json, MenuNode *parent) {
    if (!json) return NULL;

    MenuNode *node = (MenuNode*)malloc(sizeof(MenuNode));
    cJSON *title_key = cJSON_GetObjectItemCaseSensitive(json, "title_key");
    cJSON *key = cJSON_GetObjectItemCaseSensitive(json, "key");
    cJSON *shortcut = cJSON_GetObjectItemCaseSensitive(json, "shortcut");
    
    node->title_key = title_key ? strdup(title_key->valuestring) : strdup("Untitled");
    node->key = key ? strdup(key->valuestring) : strdup("no_key");
    node->shortcut = (shortcut && shortcut->valuestring && strlen(shortcut->valuestring) > 0) ? shortcut->valuestring[0] : 0;
    node->title = strdup(node->title_key); // Default to key until translated
    node->parent = parent;
    node->items = NULL;
    node->num_items = 0;

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
    free(node->items);
    free(node->title);
    free(node->title_key);
    free(node->key);
    free(node);
}

void print_menu(MenuNode *node, int selected_index) {
    // Clear screen (ANSI escape code)
    printf("\033[H\033[J");
    printf("--- %s ---\n", node->title);
    if (node->num_items == 0) {
        printf("  (No items)\n");
    } else {
        for (int i = 0; i < node->num_items; i++) {
            char shortcut_str[8] = "";
            if (node->items[i]->shortcut) {
                snprintf(shortcut_str, sizeof(shortcut_str), "(%c) ", node->items[i]->shortcut);
            }
            if (i == selected_index) {
                printf("> %s%s\n", shortcut_str, node->items[i]->title);
            } else {
                printf("  %s%s\n", shortcut_str, node->items[i]->title);
            }
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

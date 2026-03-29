#include "menu.h"

static MenuNode* parse_json_to_menu(cJSON *json, MenuNode *parent) {
    if (!json) return NULL;

    MenuNode *node = (MenuNode*)malloc(sizeof(MenuNode));
    cJSON *title = cJSON_GetObjectItemCaseSensitive(json, "title");
    cJSON *key = cJSON_GetObjectItemCaseSensitive(json, "key");
    node->title = title ? strdup(title->valuestring) : strdup("Untitled");
    node->key = key ? strdup(key->valuestring) : strdup("no_key");
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
            if (i == selected_index) {
                printf("> %s\n", node->items[i]->title);
            } else {
                printf("  %s\n", node->items[i]->title);
            }
        }
    }
    printf("\n[Arrows: Navigate | Enter: Select | Esc: Back/Exit]\n");
}

#ifndef MENU_H
#define MENU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

typedef struct MenuNode {
    char *title;
    char *title_key;
    char *description;
    char *description_key;
    char *key;
    char shortcut;
    struct MenuNode **items;
    int num_items;
    struct MenuNode *parent;
    char **languages;
    int num_languages;
} MenuNode;

MenuNode* load_menu_from_json(const char *filename);
void free_menu(MenuNode *node);
void print_menu(MenuNode *node, int selected_index);
void set_language(MenuNode *root, const char *lang_code);
int is_menu_visible(MenuNode *node, const char *current_lang);
void print_description(MenuNode *node);

#endif

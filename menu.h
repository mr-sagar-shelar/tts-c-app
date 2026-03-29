#ifndef MENU_H
#define MENU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

typedef struct MenuNode {
    char *title;
    char *key;
    struct MenuNode **items;
    int num_items;
    struct MenuNode *parent;
} MenuNode;

MenuNode* load_menu_from_json(const char *filename);
void free_menu(MenuNode *node);
void print_menu(MenuNode *node, int selected_index);

#endif

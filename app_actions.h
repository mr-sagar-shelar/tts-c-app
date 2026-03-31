#ifndef APP_ACTIONS_H
#define APP_ACTIONS_H

#include "menu.h"

int app_collect_visible_menu_items(MenuNode *node, const char *language, MenuNode **items, int max_items);
void app_handle_settings_menu(MenuNode *node, MenuNode *root);
void app_dispatch_leaf_action(MenuNode *selected_node, MenuNode *root);

#endif /* APP_ACTIONS_H */

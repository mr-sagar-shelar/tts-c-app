#ifndef VOIP_H
#define VOIP_H

#include "menu.h"

void voip_handle_menu(MenuNode *node);
char *voip_get_selected_label(const char *menu_key);

#endif /* VOIP_H */

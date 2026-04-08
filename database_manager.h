#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include "menu.h"

void init_database_manager(void);
void cleanup_database_manager(void);
void handle_database_manager(MenuNode *node);

#endif /* DATABASE_MANAGER_H */

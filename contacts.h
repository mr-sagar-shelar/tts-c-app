#ifndef CONTACTS_H
#define CONTACTS_H

#include "cJSON.h"
#include "menu.h"

typedef struct {
    char first_name[128];
    char last_name[128];
    char phone[64];
    char email[128];
    char address[256];
    char postal_code[32];
} Contact;

/**
 * Initializes the contact list from JSON.
 */
void init_contacts();

/**
 * Cleans up memory used by contacts.
 */
void cleanup_contacts();

/**
 * CRUD operations for contacts.
 */
void add_contact(Contact *c);
void edit_contact(int index, Contact *c);
void delete_contact(int index);
int get_contact_count();
Contact* get_contact(int index);
void save_contacts();

/**
 * UI handler for adding/editing a contact.
 */
void contact_form(Contact *c, int is_edit);

/**
 * Main dispatcher for address manager menu items.
 */
void handle_address_manager(MenuNode *node);

#endif /* CONTACTS_H */

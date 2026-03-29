#ifndef CONTACTS_H
#define CONTACTS_H

#include "cJSON.h"

typedef struct {
    char first_name[128];
    char last_name[128];
    char phone[64];
    char email[128];
    char address[256];
    char postal_code[32];
} Contact;

void init_contacts();
void cleanup_contacts();
void add_contact(Contact *c);
void edit_contact(int index, Contact *c);
void delete_contact(int index);
int get_contact_count();
Contact* get_contact(int index);
void save_contacts();

#endif

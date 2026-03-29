#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contacts.h"
#include "cJSON.h"

#define CONTACTS_FILE "contacts.json"

static cJSON *contacts_json = NULL;

void init_contacts() {
    FILE *f = fopen(CONTACTS_FILE, "rb");
    if (!f) {
        contacts_json = cJSON_CreateArray();
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    contacts_json = cJSON_Parse(data);
    free(data);
    if (!contacts_json) {
        contacts_json = cJSON_CreateArray();
    }
}

void cleanup_contacts() {
    if (contacts_json) {
        cJSON_Delete(contacts_json);
        contacts_json = NULL;
    }
}

void save_contacts() {
    if (!contacts_json) return;
    char *out = cJSON_Print(contacts_json);
    FILE *f = fopen(CONTACTS_FILE, "w");
    if (f) {
        fputs(out, f);
        fclose(f);
    }
    free(out);
}

void add_contact(Contact *c) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "first_name", c->first_name);
    cJSON_AddStringToObject(item, "last_name", c->last_name);
    cJSON_AddStringToObject(item, "phone", c->phone);
    cJSON_AddStringToObject(item, "email", c->email);
    cJSON_AddStringToObject(item, "address", c->address);
    cJSON_AddStringToObject(item, "postal_code", c->postal_code);
    cJSON_AddItemToArray(contacts_json, item);
    save_contacts();
}

void edit_contact(int index, Contact *c) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "first_name", c->first_name);
    cJSON_AddStringToObject(item, "last_name", c->last_name);
    cJSON_AddStringToObject(item, "phone", c->phone);
    cJSON_AddStringToObject(item, "email", c->email);
    cJSON_AddStringToObject(item, "address", c->address);
    cJSON_AddStringToObject(item, "postal_code", c->postal_code);
    cJSON_ReplaceItemInArray(contacts_json, index, item);
    save_contacts();
}

void delete_contact(int index) {
    cJSON_DeleteItemFromArray(contacts_json, index);
    save_contacts();
}

int get_contact_count() {
    return contacts_json ? cJSON_GetArraySize(contacts_json) : 0;
}

Contact* get_contact(int index) {
    cJSON *item = cJSON_GetArrayItem(contacts_json, index);
    if (!item) return NULL;

    static Contact c;
    memset(&c, 0, sizeof(Contact));
    
    cJSON *fn = cJSON_GetObjectItemCaseSensitive(item, "first_name");
    cJSON *ln = cJSON_GetObjectItemCaseSensitive(item, "last_name");
    cJSON *ph = cJSON_GetObjectItemCaseSensitive(item, "phone");
    cJSON *em = cJSON_GetObjectItemCaseSensitive(item, "email");
    cJSON *ad = cJSON_GetObjectItemCaseSensitive(item, "address");
    cJSON *pc = cJSON_GetObjectItemCaseSensitive(item, "postal_code");

    if (fn) strncpy(c.first_name, fn->valuestring, 127);
    if (ln) strncpy(c.last_name, ln->valuestring, 127);
    if (ph) strncpy(c.phone, ph->valuestring, 63);
    if (em) strncpy(c.email, em->valuestring, 127);
    if (ad) strncpy(c.address, ad->valuestring, 255);
    if (pc) strncpy(c.postal_code, pc->valuestring, 31);

    return &c;
}

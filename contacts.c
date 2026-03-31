#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "contacts.h"
#include "cJSON.h"
#include "menu.h"
#include "utils.h"
#include "config.h"

static cJSON *contacts_json = NULL;

void init_contacts() {
    cJSON *root = get_config_root();
    contacts_json = cJSON_GetObjectItemCaseSensitive(root, "contacts");
    if (!contacts_json) {
        contacts_json = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "contacts", contacts_json);
    }
}

void cleanup_contacts() {
    // contacts_json is part of config_root, cleaned up by cleanup_config()
    contacts_json = NULL;
}

void save_contacts() {
    save_config();
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

    if (fn && cJSON_IsString(fn)) strncpy(c.first_name, fn->valuestring, 127);
    if (ln && cJSON_IsString(ln)) strncpy(c.last_name, ln->valuestring, 127);
    if (ph && cJSON_IsString(ph)) strncpy(c.phone, ph->valuestring, 63);
    if (em && cJSON_IsString(em)) strncpy(c.email, em->valuestring, 127);
    if (ad && cJSON_IsString(ad)) strncpy(c.address, ad->valuestring, 255);
    if (pc && cJSON_IsString(pc)) strncpy(c.postal_code, pc->valuestring, 31);

    return &c;
}

void contact_form(Contact *c, int is_edit) {
    int field = 0;
    const char *labels[] = {"First Name", "Last Name", "Phone", "Email", "Address", "Postal Code"};
    char *buffers[] = {c->first_name, c->last_name, c->phone, c->email, c->address, c->postal_code};
    int sizes[] = {128, 128, 64, 128, 256, 32};

    while (1) {
        printf("\033[H\033[J");
        printf("--- %s Contact (Tab to navigate, Enter to Save, Esc to Cancel) ---\n", is_edit ? "Edit" : "Add");
        for (int i = 0; i < 6; i++) {
            if (i == field) printf("> %s: %s\n", labels[i], buffers[i]);
            else printf("  %s: %s\n", labels[i], buffers[i]);
        }
        fflush(stdout);

        int key = read_key();
        if (key == KEY_TAB || key == KEY_DOWN) {
            field = (field + 1) % 6;
        } else if (key == KEY_UP) {
            field = (field + 5) % 6;
        } else if (key == KEY_ENTER) {
            return;
        } else if (key == KEY_ESC) {
            memset(c, 0, sizeof(Contact));
            return;
        } else if (key == KEY_BACKSPACE) {
            int len = (int)strlen(buffers[field]);
            if (len > 0) buffers[field][len-1] = '\0';
        } else if (key > 0 && key < 1000 && isprint(key)) {
            int len = (int)strlen(buffers[field]);
            if (len < sizes[field] - 1) {
                buffers[field][len] = (char)key;
                buffers[field][len+1] = '\0';
            }
        }
    }
}

void handle_address_manager(MenuNode *node) {
    if (strcmp(node->key, "contacts_list") == 0) {
        int count = get_contact_count();
        if (count == 0) {
            printf("\033[H\033[J--- Contacts ---\nNo contacts found. Press any key...");
            fflush(stdout); read_key();
            return;
        }
        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Contacts List ---\n");
            for (int i = 0; i < count; i++) {
                Contact *c = get_contact(i);
                if (i == sel) printf("> %s %s (%s)\n", c->first_name, c->last_name, c->phone);
                else printf("  %s %s (%s)\n", c->first_name, c->last_name, c->phone);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < count - 1) sel++;
            else if (key == KEY_ENTER) {
                Contact *c = get_contact(sel);
                printf("\033[H\033[J--- Contact Details ---\n");
                printf("Name: %s %s\nPhone: %s\nEmail: %s\nAddress: %s\nPostal: %s\n\nPress any key...", 
                    c->first_name, c->last_name, c->phone, c->email, c->address, c->postal_code);
                fflush(stdout); read_key();
            } else if (key == KEY_ESC) break;
        }
    } else if (strcmp(node->key, "contacts_search") == 0) {
        char query[256];
        get_user_input(query, sizeof(query), menu_translate("ui_enter_search_term", "Enter search term"));
        if (strlen(query) == 0) return;

        int total_count = get_contact_count();
        int matches[256];
        int match_count = 0;

        char lower_query[256] = {0};
        for(int i=0; query[i] && i < 255; i++) lower_query[i] = (char)tolower(query[i]);

        for (int i = 0; i < total_count && match_count < 256; i++) {
            Contact *c = get_contact(i);
            char full_data[1024];
            snprintf(full_data, sizeof(full_data), "%s %s %s %s %s %s", 
                c->first_name, c->last_name, c->phone, c->email, c->address, c->postal_code);
            
            for(int j=0; full_data[j]; j++) full_data[j] = (char)tolower(full_data[j]);

            if (strstr(full_data, lower_query) != NULL) {
                matches[match_count++] = i;
            }
        }

        if (match_count == 0) {
            printf("\nNo contacts found. Press any key..."); fflush(stdout); read_key();
            return;
        }

        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Search Results: %s ---\n", query);
            for (int i = 0; i < match_count; i++) {
                Contact *c = get_contact(matches[i]);
                if (i == sel) printf("> %s %s (%s)\n", c->first_name, c->last_name, c->phone);
                else printf("  %s %s (%s)\n", c->first_name, c->last_name, c->phone);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < match_count - 1) sel++;
            else if (key == KEY_ENTER) {
                Contact *c = get_contact(matches[sel]);
                printf("\033[H\033[J--- Contact Details ---\n");
                printf("Name: %s %s\nPhone: %s\nEmail: %s\nAddress: %s\nPostal: %s\n\nPress any key...", 
                    c->first_name, c->last_name, c->phone, c->email, c->address, c->postal_code);
                fflush(stdout); read_key();
            } else if (key == KEY_ESC) break;
        }
    } else if (strcmp(node->key, "contacts_add") == 0) {
        Contact c; memset(&c, 0, sizeof(Contact));
        contact_form(&c, 0);
        if (strlen(c.first_name) > 0 || strlen(c.last_name) > 0) {
            add_contact(&c);
            printf("\nContact added! Press any key..."); fflush(stdout); read_key();
        }
    } else if (strcmp(node->key, "contacts_edit") == 0) {
        int count = get_contact_count();
        if (count == 0) return;
        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Select Contact to Edit ---\n");
            for (int i = 0; i < count; i++) {
                Contact *c = get_contact(i);
                if (i == sel) printf("> %s %s\n", c->first_name, c->last_name);
                else printf("  %s %s\n", c->first_name, c->last_name);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < count - 1) sel++;
            else if (key == KEY_ENTER) {
                Contact *c = get_contact(sel);
                Contact updated = *c;
                contact_form(&updated, 1);
                if (strlen(updated.first_name) > 0 || strlen(updated.last_name) > 0) {
                    edit_contact(sel, &updated);
                    printf("\nContact updated! Press any key..."); fflush(stdout); read_key();
                }
                break;
            } else if (key == KEY_ESC) break;
        }
    } else if (strcmp(node->key, "contacts_delete") == 0) {
        int count = get_contact_count();
        if (count == 0) return;
        int sel = 0;
        while (1) {
            printf("\033[H\033[J--- Select Contact to Delete ---\n");
            for (int i = 0; i < count; i++) {
                Contact *c = get_contact(i);
                if (i == sel) printf("> %s %s\n", c->first_name, c->last_name);
                else printf("  %s %s\n", c->first_name, c->last_name);
            }
            fflush(stdout);
            int key = read_key();
            if (key == KEY_UP && sel > 0) sel--;
            else if (key == KEY_DOWN && sel < count - 1) sel++;
            else if (key == KEY_ENTER) {
                printf("\nDelete this contact? (y/n)"); fflush(stdout);
                int k = read_key();
                if (k == 'y' || k == 'Y') {
                    delete_contact(sel);
                    printf("\nDeleted. Press any key..."); fflush(stdout); read_key();
                }
                break;
            } else if (key == KEY_ESC) break;
        }
    }
}

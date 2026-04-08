#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "config.h"
#include "database_manager.h"
#include "menu.h"
#include "utils.h"

#define DBM_MAX_TABLES 128
#define DBM_MAX_FIELDS 12
#define DBM_MAX_FIELD_NAME 64
#define DBM_MAX_VALUE_LEN 256

static cJSON *database_tables_json = NULL;

typedef struct {
    char values[DBM_MAX_FIELDS][DBM_MAX_VALUE_LEN];
} DatabaseRecordForm;

static cJSON *dbm_get_tables_array(void) {
    cJSON *root = get_config_root();

    if (!root) {
        return NULL;
    }

    if (!database_tables_json) {
        database_tables_json = cJSON_GetObjectItemCaseSensitive(root, "database_tables");
        if (!cJSON_IsArray(database_tables_json)) {
            database_tables_json = cJSON_CreateArray();
            cJSON_ReplaceItemInObject(root, "database_tables", database_tables_json);
            save_config();
        }
    }

    return database_tables_json;
}

void init_database_manager(void) {
    dbm_get_tables_array();
}

void cleanup_database_manager(void) {
    database_tables_json = NULL;
}

static int dbm_table_count(void) {
    cJSON *tables = dbm_get_tables_array();
    return tables ? cJSON_GetArraySize(tables) : 0;
}

static cJSON *dbm_get_table(int index) {
    cJSON *tables = dbm_get_tables_array();
    return tables ? cJSON_GetArrayItem(tables, index) : NULL;
}

static const char *dbm_table_name(cJSON *table) {
    cJSON *name = cJSON_GetObjectItemCaseSensitive(table, "name");
    return cJSON_IsString(name) && name->valuestring ? name->valuestring : "Untitled";
}

static cJSON *dbm_table_fields(cJSON *table) {
    cJSON *fields = cJSON_GetObjectItemCaseSensitive(table, "fields");
    return cJSON_IsArray(fields) ? fields : NULL;
}

static cJSON *dbm_table_records(cJSON *table) {
    cJSON *records = cJSON_GetObjectItemCaseSensitive(table, "records");
    return cJSON_IsArray(records) ? records : NULL;
}

static int dbm_field_count(cJSON *table) {
    cJSON *fields = dbm_table_fields(table);
    return fields ? cJSON_GetArraySize(fields) : 0;
}

static int dbm_record_count(cJSON *table) {
    cJSON *records = dbm_table_records(table);
    return records ? cJSON_GetArraySize(records) : 0;
}

static const char *dbm_field_name(cJSON *table, int field_index) {
    cJSON *fields = dbm_table_fields(table);
    cJSON *field = fields ? cJSON_GetArrayItem(fields, field_index) : NULL;

    return cJSON_IsString(field) && field->valuestring ? field->valuestring : "Field";
}

static cJSON *dbm_record_values(cJSON *record) {
    cJSON *values = cJSON_GetObjectItemCaseSensitive(record, "values");
    return cJSON_IsArray(values) ? values : NULL;
}

static const char *dbm_record_value(cJSON *record, int field_index) {
    cJSON *values = dbm_record_values(record);
    cJSON *value = values ? cJSON_GetArrayItem(values, field_index) : NULL;

    return cJSON_IsString(value) && value->valuestring ? value->valuestring : "";
}

static void dbm_save(void) {
    save_config();
}

static void dbm_show_message(const char *title, const char *message) {
    printf("\033[H\033[J--- %s ---\n\n%s\n\n%s",
           title,
           message,
           menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);
    read_key();
}

static int dbm_pick_table(const char *title) {
    int count = dbm_table_count();
    int selected = 0;

    if (count == 0) {
        dbm_show_message(title, "No database tables found.");
        return -1;
    }

    while (1) {
        int i;

        printf("\033[H\033[J--- %s ---\n", title);
        for (i = 0; i < count; i++) {
            cJSON *table = dbm_get_table(i);
            printf("%s%s (%d fields, %d records)\n",
                   i == selected ? "> " : "  ",
                   dbm_table_name(table),
                   dbm_field_count(table),
                   dbm_record_count(table));
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, count);
            } else if (key == KEY_ENTER) {
                return selected;
            } else if (key == KEY_ESC) {
                return -1;
            }
        }
    }
}

static void dbm_build_record_summary(cJSON *table, cJSON *record, char *buffer, size_t buffer_size) {
    int field_count = dbm_field_count(table);
    int i;

    buffer[0] = '\0';
    for (i = 0; i < field_count && i < 3; i++) {
        const char *value = dbm_record_value(record, i);
        char piece[320];

        snprintf(piece, sizeof(piece), "%s%s: %s",
                 i == 0 ? "" : " | ",
                 dbm_field_name(table, i),
                 value[0] ? value : "-");
        strncat(buffer, piece, buffer_size - strlen(buffer) - 1);
    }
}

static int dbm_pick_record(cJSON *table, const char *title, int *record_indexes, int record_count) {
    int selected = 0;

    if (!table || record_count <= 0) {
        dbm_show_message(title, "No records found.");
        return -1;
    }

    while (1) {
        int i;

        printf("\033[H\033[J--- %s ---\n", title);
        for (i = 0; i < record_count; i++) {
            cJSON *record = cJSON_GetArrayItem(dbm_table_records(table), record_indexes ? record_indexes[i] : i);
            char summary[512];

            dbm_build_record_summary(table, record, summary, sizeof(summary));
            printf("%s%s\n", i == selected ? "> " : "  ", summary[0] ? summary : "(empty record)");
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, record_count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, record_count);
            } else if (key == KEY_ENTER) {
                return record_indexes ? record_indexes[selected] : selected;
            } else if (key == KEY_ESC) {
                return -1;
            }
        }
    }
}

static int dbm_table_name_exists(const char *name, int ignore_index) {
    int i;
    int count = dbm_table_count();

    for (i = 0; i < count; i++) {
        if (i == ignore_index) {
            continue;
        }
        if (strcasecmp(dbm_table_name(dbm_get_table(i)), name) == 0) {
            return 1;
        }
    }

    return 0;
}

static void dbm_create_table(void) {
    char name[128] = {0};
    char field_count_text[16] = {0};
    int field_count;
    int i;
    cJSON *table;
    cJSON *fields;

    get_user_input(name, sizeof(name), "Enter table name");
    if (name[0] == '\0') {
        return;
    }
    if (dbm_table_name_exists(name, -1)) {
        dbm_show_message("Database Manager", "A table with this name already exists.");
        return;
    }

    get_user_input(field_count_text, sizeof(field_count_text), "Enter number of fields (1 to 12)");
    field_count = atoi(field_count_text);
    if (field_count < 1 || field_count > DBM_MAX_FIELDS) {
        dbm_show_message("Database Manager", "Invalid field count.");
        return;
    }

    table = cJSON_CreateObject();
    fields = cJSON_CreateArray();

    cJSON_AddStringToObject(table, "name", name);
    cJSON_AddItemToObject(table, "fields", fields);
    cJSON_AddItemToObject(table, "records", cJSON_CreateArray());

    for (i = 0; i < field_count; i++) {
        char field_name[DBM_MAX_FIELD_NAME] = {0};
        char prompt[96];

        snprintf(prompt, sizeof(prompt), "Enter name for field %d", i + 1);
        get_user_input(field_name, sizeof(field_name), prompt);
        if (field_name[0] == '\0') {
            snprintf(field_name, sizeof(field_name), "Field %d", i + 1);
        }
        cJSON_AddItemToArray(fields, cJSON_CreateString(field_name));
    }

    cJSON_AddItemToArray(dbm_get_tables_array(), table);
    dbm_save();
    dbm_show_message("Database Manager", "Table created.");
}

static void dbm_delete_table(void) {
    int table_index = dbm_pick_table("Delete Database Table");

    if (table_index < 0) {
        return;
    }

    printf("\033[H\033[J--- Delete Database Table ---\n\nDelete table \"%s\"? (y/n)",
           dbm_table_name(dbm_get_table(table_index)));
    fflush(stdout);
    {
        int key = read_key();
        if (key == 'y' || key == 'Y') {
            cJSON_DeleteItemFromArray(dbm_get_tables_array(), table_index);
            dbm_save();
            dbm_show_message("Database Manager", "Table deleted.");
        }
    }
}

static void dbm_show_tables(void) {
    int table_index = dbm_pick_table("Database Tables");

    if (table_index < 0) {
        return;
    }

    {
        cJSON *table = dbm_get_table(table_index);
        char message[1024];
        int field_count = dbm_field_count(table);
        int i;

        snprintf(message, sizeof(message), "Table: %s\nFields: ", dbm_table_name(table));
        for (i = 0; i < field_count; i++) {
            strncat(message, dbm_field_name(table, i), sizeof(message) - strlen(message) - 1);
            if (i + 1 < field_count) {
                strncat(message, ", ", sizeof(message) - strlen(message) - 1);
            }
        }
        {
            char counts[128];
            snprintf(counts, sizeof(counts), "\nRecords: %d", dbm_record_count(table));
            strncat(message, counts, sizeof(message) - strlen(message) - 1);
        }
        dbm_show_message("Database Table", message);
    }
}

static void dbm_show_current_table_info(cJSON *table) {
    char message[1024];
    int field_count;
    int i;

    if (!table) {
        return;
    }

    field_count = dbm_field_count(table);
    snprintf(message, sizeof(message), "Table: %s\nFields: ", dbm_table_name(table));
    for (i = 0; i < field_count; i++) {
        strncat(message, dbm_field_name(table, i), sizeof(message) - strlen(message) - 1);
        if (i + 1 < field_count) {
            strncat(message, ", ", sizeof(message) - strlen(message) - 1);
        }
    }
    {
        char counts[128];
        snprintf(counts, sizeof(counts), "\nRecords: %d", dbm_record_count(table));
        strncat(message, counts, sizeof(message) - strlen(message) - 1);
    }
    dbm_show_message("Database Table", message);
}

static int dbm_fill_form_from_record(cJSON *table, cJSON *record, DatabaseRecordForm *form) {
    int field_count = dbm_field_count(table);
    int i;

    if (!form) {
        return 0;
    }
    memset(form, 0, sizeof(*form));

    for (i = 0; i < field_count; i++) {
        snprintf(form->values[i], sizeof(form->values[i]), "%s",
                 record ? dbm_record_value(record, i) : "");
    }

    return 1;
}

static int dbm_record_form(cJSON *table, DatabaseRecordForm *form, int is_edit) {
    int field_count = dbm_field_count(table);
    int field = 0;

    if (!table || !form || field_count <= 0) {
        return 0;
    }

    while (1) {
        int i;

        printf("\033[H\033[J--- %s Record (%s) ---\n",
               is_edit ? "Edit" : "Add",
               dbm_table_name(table));
        for (i = 0; i < field_count; i++) {
            printf("%s%s: %s\n",
                   i == field ? "> " : "  ",
                   dbm_field_name(table, i),
                   form->values[i]);
        }
        printf("\n[Arrows/Tab: Navigate | Type: Edit | Enter: Save | Esc: Cancel]\n");
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_TAB || key == KEY_DOWN) {
                field = (field + 1) % field_count;
            } else if (key == KEY_UP) {
                field = (field + field_count - 1) % field_count;
            } else if (key == KEY_ENTER) {
                return 1;
            } else if (key == KEY_ESC) {
                return 0;
            } else if (key == KEY_BACKSPACE) {
                int len = (int)strlen(form->values[field]);
                if (len > 0) {
                    form->values[field][len - 1] = '\0';
                }
            } else if (key > 0 && key < 1000 && isprint(key)) {
                int len = (int)strlen(form->values[field]);
                if (len < DBM_MAX_VALUE_LEN - 1) {
                    form->values[field][len] = (char)key;
                    form->values[field][len + 1] = '\0';
                }
            }
        }
    }
}

static cJSON *dbm_build_record_json(cJSON *table, DatabaseRecordForm *form) {
    int field_count = dbm_field_count(table);
    int i;
    cJSON *record = cJSON_CreateObject();
    cJSON *values = cJSON_CreateArray();

    cJSON_AddItemToObject(record, "values", values);
    for (i = 0; i < field_count; i++) {
        cJSON_AddItemToArray(values, cJSON_CreateString(form->values[i]));
    }

    return record;
}

static void dbm_add_record(cJSON *table) {
    DatabaseRecordForm form;

    dbm_fill_form_from_record(table, NULL, &form);
    if (!dbm_record_form(table, &form, 0)) {
        return;
    }

    cJSON_AddItemToArray(dbm_table_records(table), dbm_build_record_json(table, &form));
    dbm_save();
    dbm_show_message("Database Record", "Record added.");
}

static void dbm_edit_record(cJSON *table) {
    int record_index = dbm_pick_record(table, "Edit Record", NULL, dbm_record_count(table));

    if (record_index < 0) {
        return;
    }

    {
        DatabaseRecordForm form;
        cJSON *record = cJSON_GetArrayItem(dbm_table_records(table), record_index);

        dbm_fill_form_from_record(table, record, &form);
        if (!dbm_record_form(table, &form, 1)) {
            return;
        }
        cJSON_ReplaceItemInArray(dbm_table_records(table), record_index, dbm_build_record_json(table, &form));
        dbm_save();
        dbm_show_message("Database Record", "Record updated.");
    }
}

static void dbm_delete_record(cJSON *table) {
    int record_index = dbm_pick_record(table, "Delete Record", NULL, dbm_record_count(table));

    if (record_index < 0) {
        return;
    }

    printf("\033[H\033[J--- Delete Record ---\n\nDelete this record? (y/n)");
    fflush(stdout);
    {
        int key = read_key();
        if (key == 'y' || key == 'Y') {
            cJSON_DeleteItemFromArray(dbm_table_records(table), record_index);
            dbm_save();
            dbm_show_message("Database Record", "Record deleted.");
        }
    }
}

static void dbm_view_records(cJSON *table) {
    int record_index = dbm_pick_record(table, "Database Records", NULL, dbm_record_count(table));

    if (record_index < 0) {
        return;
    }

    {
        cJSON *record = cJSON_GetArrayItem(dbm_table_records(table), record_index);
        char message[2048];
        int field_count = dbm_field_count(table);
        int i;

        message[0] = '\0';
        for (i = 0; i < field_count; i++) {
            char line[512];
            snprintf(line, sizeof(line), "%s: %s\n",
                     dbm_field_name(table, i),
                     dbm_record_value(record, i));
            strncat(message, line, sizeof(message) - strlen(message) - 1);
        }
        dbm_show_message("Database Record", message[0] ? message : "(empty record)");
    }
}

static void dbm_search_records(cJSON *table) {
    int total_count = dbm_record_count(table);
    int *matches;
    int match_count = 0;
    char query[256];
    char lower_query[256] = {0};
    int i;

    get_user_input(query, sizeof(query), "Enter search term");
    if (query[0] == '\0') {
        return;
    }

    if (total_count <= 0) {
        dbm_show_message("Search Records", "No records found.");
        return;
    }

    matches = (int *)malloc((size_t)total_count * sizeof(int));
    if (!matches) {
        dbm_show_message("Search Records", "Unable to allocate search results.");
        return;
    }

    for (i = 0; query[i] && i < 255; i++) {
        lower_query[i] = (char)tolower((unsigned char)query[i]);
    }

    for (i = 0; i < total_count; i++) {
        cJSON *record = cJSON_GetArrayItem(dbm_table_records(table), i);
        int field_index;
        char haystack[2048] = {0};

        for (field_index = 0; field_index < dbm_field_count(table); field_index++) {
            strncat(haystack, dbm_record_value(record, field_index), sizeof(haystack) - strlen(haystack) - 1);
            strncat(haystack, " ", sizeof(haystack) - strlen(haystack) - 1);
        }
        for (field_index = 0; haystack[field_index]; field_index++) {
            haystack[field_index] = (char)tolower((unsigned char)haystack[field_index]);
        }

        if (strstr(haystack, lower_query) != NULL) {
            matches[match_count++] = i;
        }
    }

    if (match_count == 0) {
        free(matches);
        dbm_show_message("Search Records", "No matching records found.");
        return;
    }

    {
        int record_index = dbm_pick_record(table, "Search Records", matches, match_count);
        if (record_index >= 0) {
            cJSON *record = cJSON_GetArrayItem(dbm_table_records(table), record_index);
            char message[2048];
            int field_count = dbm_field_count(table);
            int j;

            message[0] = '\0';
            for (j = 0; j < field_count; j++) {
                char line[512];
                snprintf(line, sizeof(line), "%s: %s\n",
                         dbm_field_name(table, j),
                         dbm_record_value(record, j));
                strncat(message, line, sizeof(message) - strlen(message) - 1);
            }
            dbm_show_message("Database Record", message);
        }
        free(matches);
    }
}

static void dbm_open_table(void) {
    int table_index = dbm_pick_table("Open Database Table");

    if (table_index < 0) {
        return;
    }

    {
        cJSON *table = dbm_get_table(table_index);
        int selected = 0;
        const char *options[] = {
            "List Records",
            "Search Records",
            "Add Record",
            "Edit Record",
            "Delete Record",
            "Table Info",
            "Back"
        };
        const int option_count = 7;

        while (1) {
            int i;

            printf("\033[H\033[J--- %s ---\n", dbm_table_name(table));
            for (i = 0; i < option_count; i++) {
                printf("%s%s\n", i == selected ? "> " : "  ", options[i]);
            }
            printf("\nFields: %d | Records: %d\n", dbm_field_count(table), dbm_record_count(table));
            printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
            fflush(stdout);

            {
                int key = read_key();
                if (key == KEY_UP) {
                    selected = menu_next_index(selected, -1, option_count);
                } else if (key == KEY_DOWN) {
                    selected = menu_next_index(selected, 1, option_count);
                } else if (key == KEY_ENTER) {
                    if (selected == 0) {
                        dbm_view_records(table);
                    } else if (selected == 1) {
                        dbm_search_records(table);
                    } else if (selected == 2) {
                        dbm_add_record(table);
                    } else if (selected == 3) {
                        dbm_edit_record(table);
                    } else if (selected == 4) {
                        dbm_delete_record(table);
                    } else if (selected == 5) {
                        dbm_show_current_table_info(table);
                    } else {
                        return;
                    }
                } else if (key == KEY_ESC) {
                    return;
                }
            }
        }
    }
}

void handle_database_manager(MenuNode *node) {
    if (strcmp(node->key, "db_table_list") == 0) {
        dbm_show_tables();
    } else if (strcmp(node->key, "db_table_create") == 0) {
        dbm_create_table();
    } else if (strcmp(node->key, "db_table_open") == 0) {
        dbm_open_table();
    } else if (strcmp(node->key, "db_table_delete") == 0) {
        dbm_delete_table();
    }
}

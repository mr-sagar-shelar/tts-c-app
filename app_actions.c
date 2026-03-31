#include "app_actions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "alarm.h"
#include "calendar.h"
#include "config.h"
#include "contacts.h"
#include "dictionary.h"
#include "entertainment.h"
#include "file_manager.h"
#include "notepad.h"
#include "radio.h"
#include "speech_engine.h"
#include "speech_settings.h"
#include "tools.h"
#include "typing_tutor.h"
#include "utils.h"
#include "voice_library.h"

int app_collect_visible_menu_items(MenuNode *node, const char *language, MenuNode **items, int max_items) {
    int visible_count = 0;
    int i;

    if (!node || !items || max_items <= 0) {
        return 0;
    }

    for (i = 0; i < node->num_items && visible_count < max_items; i++) {
        if (is_menu_visible(node->items[i], language)) {
            items[visible_count++] = node->items[i];
        }
    }

    return visible_count;
}

static int app_is_descendant_of(MenuNode *node, const char *ancestor_key) {
    MenuNode *cursor = node;

    while (cursor) {
        if (strcmp(cursor->key, ancestor_key) == 0) {
            return 1;
        }
        cursor = cursor->parent;
    }

    return 0;
}

void app_handle_settings_menu(MenuNode *node, MenuNode *root) {
    char speech_message[128];
    char speech_error[128];

    if (strcmp(node->key, "language_switch") == 0) {
        char *current_lang = get_setting("language");
        if (!current_lang) {
            current_lang = strdup("en");
        }

        printf("\033[H\033[J");
        printf("--- %s ---\n", node->title);
        printf("Current Language: %s\n", current_lang);
        printf("\n1. English (en)\n");
        printf("2. Hindi (hi)\n");
        printf("\nSelect (1 or 2), or Esc to go back.");
        fflush(stdout);

        while (1) {
            int key = read_key();
            if (key == '1') {
                save_setting("language", "en");
                set_language(root, "en");
                break;
            } else if (key == '2') {
                save_setting("language", "hi");
                set_language(root, "hi");
                break;
            } else if (key == KEY_ESC) {
                break;
            }
        }
        free(current_lang);
        return;
    }

    if (handle_speech_setting_selection(node->key, speech_message, sizeof(speech_message))) {
        speech_engine_reload_settings();
        printf("\033[H\033[J");
        printf("--- %s ---\n", node->title);
        printf("%s\n", speech_message);
        if (speech_engine_startup(speech_error, sizeof(speech_error))) {
            printf("Speech engine updated with the selected voice.\n");
        }
        printf("\nPress any key to continue...");
        fflush(stdout);
        read_key();
        return;
    }

    if (strcmp(node->key, "download_voice") == 0) {
        voice_library_show_menu();
        return;
    }

    {
        char *current_val = get_setting(node->key);
        printf("\033[H\033[J");
        printf("--- %s ---\n", node->title);
        printf("Current Value: %s\n", current_val ? current_val : "Not set");
        free(current_val);

        printf("\nPress Enter to change or Esc to go back.");
        fflush(stdout);

        while (1) {
            int key = read_key();
            if (key == KEY_ENTER) {
                char new_val[256];
                get_user_input(new_val, sizeof(new_val), "Enter new value");
                save_setting(node->key, new_val);
                printf("\nValue saved! Press any key to continue...");
                fflush(stdout);
                read_key();
                break;
            } else if (key == KEY_ESC) {
                break;
            }
        }
    }
}

static void app_open_notepad_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (file) {
        char content[4096] = {0};
        fread(content, 1, sizeof(content) - 1, file);
        fclose(file);
        handle_notepad(content, path);
    } else {
        printf("\nError opening file. Press any key...");
        fflush(stdout);
        read_key();
    }
}

void app_dispatch_leaf_action(MenuNode *selected_node, MenuNode *root) {
    if (strcmp(selected_node->key, "notepad_new") == 0) {
        handle_notepad(NULL, NULL);
    } else if (strcmp(selected_node->key, "notepad_search") == 0) {
        handle_notepad_search();
    } else if (strcmp(selected_node->key, "notepad_open") == 0 || strcmp(selected_node->key, "wp_open") == 0) {
        char *selected_path = strcmp(selected_node->key, "wp_open") == 0
            ? file_navigator_supported(USER_SPACE)
            : file_navigator(USER_SPACE, 0);
        if (selected_path) {
            if (strcmp(selected_node->key, "notepad_open") == 0) {
                app_open_notepad_file(selected_path);
            } else {
                file_manager_open_viewer(selected_path);
            }
            free(selected_path);
        }
    } else if (strcmp(selected_node->key, "sense_dictionary") == 0) {
        handle_dictionary();
    } else if (strcmp(selected_node->key, "english_only_dictionary") == 0) {
        handle_english_only_dictionary();
    } else if (strcmp(selected_node->key, "multi_lang_dictionary") == 0) {
        handle_multi_lang_dictionary();
    } else if (strcmp(selected_node->key, "short_stories") == 0) {
        content_ui_show_short_stories();
    } else if (strcmp(selected_node->key, "joke") == 0) {
        content_ui_show_joke();
    } else if (strcmp(selected_node->key, "calculator") == 0) {
        system_ui_run_calculator();
    } else if (strcmp(selected_node->key, "typing_tutor") == 0) {
        handle_typing_tutor();
    } else if (strcmp(selected_node->key, "weather") == 0) {
        system_ui_show_weather();
    } else if (strcmp(selected_node->key, "current_time_date") == 0) {
        system_ui_show_current_time_date();
    } else if (strcmp(selected_node->key, "news") == 0) {
        system_ui_show_news();
    } else if (strcmp(selected_node->key, "poems") == 0) {
        content_ui_show_poems();
    } else if (strcmp(selected_node->key, "fm_word_viewer") == 0) {
        content_ui_run_word_viewer();
    } else if (strcmp(selected_node->key, "set_city") == 0) {
        system_ui_set_city();
    } else if (strcmp(selected_node->key, "timezone") == 0) {
        system_ui_change_timezone();
    } else if (strcmp(selected_node->key, "time_format") == 0) {
        system_ui_change_time_format();
    } else if (strcmp(selected_node->key, "set_time_manual") == 0) {
        system_ui_set_time_manual();
    } else if (strcmp(selected_node->key, "set_date_manual") == 0) {
        system_ui_set_date_manual();
    } else if (strcmp(selected_node->key, "alarm") == 0) {
        handle_alarm();
    } else if (strcmp(selected_node->key, "internet_radio") == 0) {
        radio_ui_show_menu();
    } else if (app_is_descendant_of(selected_node, "settings")) {
        if (strcmp(selected_node->key, "sync_server") == 0) {
            sync_config();
            printf("\nPress any key to continue...");
            fflush(stdout);
            read_key();
        } else {
            app_handle_settings_menu(selected_node, root);
        }
    } else if (app_is_descendant_of(selected_node, "file_manager")) {
        file_manager_handle_menu(selected_node);
    } else if (app_is_descendant_of(selected_node, "address_manager")) {
        handle_address_manager(selected_node);
    } else if (app_is_descendant_of(selected_node, "calendar")) {
        handle_calendar(selected_node);
    }
}

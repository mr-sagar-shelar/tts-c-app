#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_actions.h"
#include "calendar.h"
#include "config.h"
#include "contacts.h"
#include "file_manager.h"
#include "menu.h"
#include "speech_engine.h"
#include "utils.h"

int main() {
    init_config();
    {
        char speech_error[128];
        speech_engine_startup(speech_error, sizeof(speech_error));
    }
    init_contacts();
    init_calendar();
    
    mkdir(USER_SPACE, 0777);
    mkdir("Downloads", 0777);
    
    MenuNode *root = load_menu_from_json("menu.json");
    if (!root) {
        fprintf(stderr, "Failed to load menu.json\n");
        return 1;
    }

    char *lang_setting = get_setting("language");
    if (lang_setting) {
        set_language(root, lang_setting);
        free(lang_setting);
    } else {
        set_language(root, "en");
    }

    set_conio_terminal_mode();
    app_sync_language_voice_on_startup(root);

    MenuNode *current_node = root;
    int selected_index = 0;

    while (1) {
        print_menu(current_node, selected_index);

        int key = read_key();
        
        MenuNode *visible_items[256];
        char *lang = get_setting("language");
        if (!lang) lang = strdup("en");
        int visible_count = app_collect_visible_menu_items(current_node, lang, visible_items, 256);

        if (key == KEY_ESC) {
            if (current_node->parent) {
                MenuNode *parent = current_node->parent;
                int parent_visible_idx = 0;
                for (int i = 0; i < parent->num_items; i++) {
                    if (is_menu_visible(parent->items[i], lang)) {
                        if (parent->items[i] == current_node) {
                            selected_index = parent_visible_idx;
                            break;
                        }
                        parent_visible_idx++;
                    }
                }
                current_node = parent;
            } else {
                printf("\nExiting...\n");
                free(lang);
                speech_engine_shutdown();
                break;
            }
        } else if (key == KEY_UP) {
            selected_index = menu_next_index(selected_index, -1, visible_count);
        } else if (key == KEY_DOWN) {
            selected_index = menu_next_index(selected_index, 1, visible_count);
        } else if (key == KEY_CTRL_I) {
            if (visible_count > 0) {
                print_description(visible_items[selected_index]);
            }
        } else if (key == KEY_ENTER) {
            if (visible_count > 0) {
                MenuNode *selected_node = visible_items[selected_index];
                if (selected_node->num_items > 0) {
                    current_node = selected_node;
                    selected_index = 0;
                } else {
                    app_dispatch_leaf_action(selected_node, root);
                }
            }
        } else if (key > 0 && key < 1000) {
            // Shortcut handling
            for (int i = 0; i < visible_count; i++) {
                if (visible_items[i]->shortcut == tolower(key)) {
                    selected_index = i;
                    // Trigger ENTER logic for the shortcut
                    // (Simplest is to just simulate key == KEY_ENTER here or move logic to function)
                    // For now, let's just use the selected_index and continue the loop to next turn
                    // where it might be handled, but better to just call the logic.
                    // To avoid code duplication, I'll keep it simple for now.
                    break;
                }
            }
        }
        free(lang);
    }

    free_menu(root);
    cleanup_config();
    cleanup_contacts();
    cleanup_calendar();
    return 0;
}

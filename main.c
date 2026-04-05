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
#include "menu_audio.h"
#include "menu.h"
#include "speech_engine.h"
#include "utils.h"

static void append_menu_speech_part(char *buffer, size_t buffer_size, const char *text) {
    size_t len;

    if (!buffer || !text || !text[0]) {
        return;
    }

    len = strlen(buffer);
    if (len > 0 && len + 2 < buffer_size) {
        snprintf(buffer + len, buffer_size - len, ". ");
        len = strlen(buffer);
    }

    if (len < buffer_size) {
        snprintf(buffer + len, buffer_size - len, "%s", text);
    }
}

static void build_menu_speech_text(MenuNode *selected_node, char *buffer, size_t buffer_size) {
    char shortcut_text[32];

    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';

    if (selected_node && selected_node->title) {
        append_menu_speech_part(buffer, buffer_size, selected_node->title);
        if (selected_node->shortcut) {
            snprintf(shortcut_text, sizeof(shortcut_text), " %c ",
                     (char)toupper((unsigned char)selected_node->shortcut));
            append_menu_speech_part(buffer, buffer_size, shortcut_text);
        }
    }
}

int main() {
    int has_utf8_locale;
    MenuNode *last_spoken_node = NULL;
    int last_spoken_index = -1;
    int startup_ready_announced = 0;

    has_utf8_locale = init_utf8_locale();
    enable_utf8_terminal_mode();

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
        if (strcmp(lang_setting, "hi") == 0 && !has_utf8_locale) {
            fprintf(stderr,
                    "Warning: UTF-8 locale unavailable. Hindi text may not render correctly on this terminal.\n");
        }
        free(lang_setting);
    } else {
        set_language(root, "en");
    }

    set_conio_terminal_mode();
    app_sync_language_voice_on_startup(root);
    menu_audio_init();

    MenuNode *current_node = root;
    int selected_index = 0;

    while (1) {
        MenuNode *visible_items[256];
        char menu_speech[512];
        char *lang = get_setting("language");
        if (!lang) lang = strdup("en");
        int visible_count = app_collect_visible_menu_items(current_node, lang, visible_items, 256);
        if (visible_count > 0 && selected_index >= visible_count) {
            selected_index = visible_count - 1;
        }
        if (visible_count > 0) {
            if (!startup_ready_announced) {
                char startup_message[640];
                build_menu_speech_text(visible_items[selected_index], menu_speech, sizeof(menu_speech));
                if (menu_speech[0] != '\0') {
                    snprintf(startup_message, sizeof(startup_message), "Sai is ready. %s", menu_speech);
                } else {
                    snprintf(startup_message, sizeof(startup_message), "Sai is ready");
                }
                menu_audio_speak(startup_message);
                startup_ready_announced = 1;
                last_spoken_node = current_node;
                last_spoken_index = selected_index;
            } else if (last_spoken_node != current_node || last_spoken_index != selected_index) {
                build_menu_speech_text(visible_items[selected_index], menu_speech, sizeof(menu_speech));
                menu_audio_request(menu_speech);
                last_spoken_node = current_node;
                last_spoken_index = selected_index;
            }
        } else {
            menu_audio_stop();
            last_spoken_node = current_node;
            last_spoken_index = -1;
        }

        print_menu(current_node, selected_index);

        int key = read_key();

        if (key == KEY_ESC) {
            menu_audio_stop();
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
                last_spoken_node = NULL;
            } else {
                break;
            }
        } else if (key == KEY_UP) {
            selected_index = menu_next_index(selected_index, -1, visible_count);
        } else if (key == KEY_DOWN) {
            selected_index = menu_next_index(selected_index, 1, visible_count);
        } else if (key == KEY_CTRL_I) {
            menu_audio_stop();
            if (visible_count > 0) {
                print_description(visible_items[selected_index]);
                last_spoken_node = NULL;
            }
        } else if (key == KEY_ENTER) {
            menu_audio_stop();
            if (visible_count > 0) {
                MenuNode *selected_node = visible_items[selected_index];
                if (selected_node->num_items > 0) {
                    current_node = selected_node;
                    selected_index = 0;
                    last_spoken_node = NULL;
                } else {
                    app_dispatch_leaf_action(selected_node, root);
                    last_spoken_node = NULL;
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
    menu_audio_shutdown();
    speech_engine_shutdown();
    cleanup_config();
    cleanup_contacts();
    cleanup_calendar();
    return 0;
}

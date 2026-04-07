#include "app_actions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "alarm.h"
#include "calendar.h"
#include "config.h"
#include "contacts.h"
#include "database_manager.h"
#include "dictionary.h"
#include "entertainment.h"
#include "file_manager.h"
#include "keys_manager.h"
#include "notepad.h"
#include "radio.h"
#include "speech_engine.h"
#include "speech_settings.h"
#include "tools.h"
#include "trivia.h"
#include "typing_tutor.h"
#include "utils.h"
#include "voice_library.h"
#include "wifi_manager.h"

static int app_confirm_action(const char *title, const char *message) {
    int selected_index = 0;
    const char *options[] = {
        menu_translate("ui_yes", "Yes"),
        menu_translate("ui_no", "No")
    };

    while (1) {
        printf("\033[H\033[J--- %s ---\n", title);
        printf("%s\n\n", message);
        for (int i = 0; i < 2; i++) {
            if (i == selected_index) {
                printf("> %s\n", options[i]);
            } else {
                printf("  %s\n", options[i]);
            }
        }
        printf("\n%s\n", menu_translate("ui_footer_cancel", "[Arrows: Navigate | Enter: Select | Esc: Cancel]"));
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected_index = menu_next_index(selected_index, -1, 2);
            } else if (key == KEY_DOWN) {
                selected_index = menu_next_index(selected_index, 1, 2);
            } else if (key == KEY_ENTER) {
                return selected_index == 0;
            } else if (key == KEY_ESC) {
                return 0;
            }
        }
    }
}

static int app_apply_language_voice(const char *language_code) {
    char hindi_voice_path[256];
    char *current_voice;

    if (!language_code) {
        return 1;
    }

    voice_library_get_voice_path(VOICE_LIBRARY_HINDI_FILENAME, hindi_voice_path, sizeof(hindi_voice_path));
    current_voice = get_setting("tts_voice");

    if (strcmp(language_code, "hi") == 0) {
        char error[256] = {0};

        if (!voice_library_voice_exists(VOICE_LIBRARY_HINDI_FILENAME)) {
            if (!app_confirm_action("Hindi Voice",
                                    "Hindi speech requires the Hindi Flite voice.\nDownload it now?")) {
                free(current_voice);
                return 0;
            }

            if (!voice_library_download_voice(VOICE_LIBRARY_HINDI_FILENAME,
                                              "Hindi Voice Download",
                                              error,
                                              sizeof(error))) {
                printf("\033[H\033[J--- Hindi Voice ---\n%s\n\n%s",
                       error[0] ? error : "Hindi voice download failed.",
                       menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
                fflush(stdout);
                read_key();
                free(current_voice);
                return 0;
            }
        }

        save_setting("tts_voice", hindi_voice_path);
    } else if (!current_voice || strcmp(current_voice, hindi_voice_path) == 0) {
        save_setting("tts_voice", "slt");
    }

    free(current_voice);
    speech_engine_reload_settings();
    return 1;
}

void app_sync_language_voice_on_startup(MenuNode *root) {
    char *language_code = get_setting("language");

    if (!language_code) {
        return;
    }

    if (root) {
        set_language(root, language_code);
    }

    if (!app_apply_language_voice(language_code)) {
        if (strcmp(language_code, "hi") == 0) {
            save_setting("language", "en");
            if (root) {
                set_language(root, "en");
            }
            app_apply_language_voice("en");
        }
    } else {
        char speech_error[128];
        speech_engine_startup(speech_error, sizeof(speech_error));
    }

    free(language_code);
}

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
        int selected_index = 0;
        const char *titles[] = {
            menu_translate("ui_language_english", "English"),
            menu_translate("ui_language_hindi", "Hindi")
        };
        const char *values[] = {"en", "hi"};
        char *current_lang = get_setting("language");
        if (current_lang && strcmp(current_lang, "hi") == 0) {
            selected_index = 1;
        }

        while (1) {
            printf("\033[H\033[J");
            printf("--- %s ---\n", node->title);
            printf("%s: %s\n\n", menu_translate("ui_selected_value", "Selected Value"), titles[selected_index]);
            for (int i = 0; i < 2; i++) {
                if (i == selected_index) {
                    printf("> %s\n", titles[i]);
                } else {
                    printf("  %s\n", titles[i]);
                }
            }
            printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
            fflush(stdout);

            {
                int key = read_key();
                if (key == KEY_UP) {
                    selected_index = menu_next_index(selected_index, -1, 2);
                } else if (key == KEY_DOWN) {
                    selected_index = menu_next_index(selected_index, 1, 2);
                } else if (key == KEY_ENTER) {
                    if (app_apply_language_voice(values[selected_index])) {
                        save_setting("language", values[selected_index]);
                        set_language(root, values[selected_index]);
                        init_utf8_locale();
                        enable_utf8_terminal_mode();
                        break;
                    }
                } else if (key == KEY_ESC) {
                    break;
                }
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
            printf("%s\n", menu_translate("ui_speech_engine_updated", "Speech engine updated with the selected voice."));
        }
        printf("\n%s", menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
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
        printf("%s: %s\n",
               menu_translate("ui_current_value", "Current Value"),
               current_val ? current_val : menu_translate("ui_not_set", "Not set"));
        free(current_val);

        printf("\n%s", menu_translate("ui_press_enter_change_or_esc_back", "Press Enter to change or Esc to go back"));
        fflush(stdout);

        while (1) {
            int key = read_key();
            if (key == KEY_ENTER) {
                char new_val[256];
                get_user_input(new_val, sizeof(new_val), "Enter new value");
                save_setting(node->key, new_val);
                printf("\n%s %s",
                       menu_translate("ui_value_saved", "Value saved!"),
                       menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
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
    } else if (strcmp(selected_node->key, "convert_units") == 0) {
        system_ui_convert_units();
    } else if (strcmp(selected_node->key, "trivia_start") == 0) {
        trivia_run_quiz();
    } else if (strncmp(selected_node->key, "trivia_", 7) == 0) {
        trivia_show_settings_menu(selected_node->key);
    } else if (strcmp(selected_node->key, "english_only_dictionary") == 0) {
        handle_english_only_dictionary();
    } else if (strcmp(selected_node->key, "multi_lang_dictionary") == 0) {
        handle_multi_lang_dictionary();
    } else if (strcmp(selected_node->key, "short_stories") == 0) {
        content_ui_show_short_stories();
    } else if (strcmp(selected_node->key, "joke") == 0) {
        content_ui_show_joke();
    } else if (strcmp(selected_node->key, "random_life_hack") == 0) {
        content_ui_show_random_life_hack();
    } else if (strcmp(selected_node->key, "random_affirmation") == 0) {
        content_ui_show_random_affirmation();
    } else if (strcmp(selected_node->key, "random_trivia") == 0) {
        content_ui_show_random_trivia();
    } else if (strcmp(selected_node->key, "random_riddle") == 0) {
        content_ui_show_random_riddle();
    } else if (strcmp(selected_node->key, "random_quote") == 0) {
        content_ui_show_random_quote();
    } else if (strcmp(selected_node->key, "synonyms") == 0) {
        content_ui_show_synonyms();
    } else if (strcmp(selected_node->key, "singularize") == 0) {
        content_ui_show_singularize();
    } else if (strcmp(selected_node->key, "random_poem_api") == 0) {
        content_ui_show_random_poem_api();
    } else if (strcmp(selected_node->key, "random_joke_api") == 0) {
        content_ui_show_random_joke_api();
    } else if (strcmp(selected_node->key, "calculator") == 0) {
        system_ui_run_calculator();
    } else if (strcmp(selected_node->key, "typing_tutor") == 0 ||
               strcmp(selected_node->key, "learn_typing") == 0) {
        handle_typing_tutor();
    } else if (strcmp(selected_node->key, "weather") == 0) {
        system_ui_show_weather();
    } else if (strcmp(selected_node->key, "google_search") == 0) {
        system_ui_google_search();
    } else if (strcmp(selected_node->key, "wiki_search") == 0) {
        system_ui_wiki_search();
    } else if (strcmp(selected_node->key, "world_clock") == 0) {
        system_ui_show_world_clock();
    } else if (strcmp(selected_node->key, "news") == 0) {
        system_ui_show_news();
    } else if (strcmp(selected_node->key, "poems") == 0) {
        content_ui_show_poems();
    } else if (strcmp(selected_node->key, "riddles") == 0) {
        content_ui_show_local_riddle();
    } else if (strcmp(selected_node->key, "fm_word_viewer") == 0) {
        content_ui_run_word_viewer();
    } else if (strcmp(selected_node->key, "set_city") == 0) {
        system_ui_set_city();
    } else if (strcmp(selected_node->key, "set_volume") == 0) {
        system_ui_set_volume();
    } else if (strcmp(selected_node->key, "audio_output_hdmi") == 0) {
        system_ui_set_audio_output("hdmi");
    } else if (strcmp(selected_node->key, "audio_output_hat") == 0) {
        system_ui_set_audio_output("hat");
    } else if (strcmp(selected_node->key, "timezone") == 0) {
        system_ui_change_timezone();
    } else if (strcmp(selected_node->key, "time_format") == 0) {
        system_ui_change_time_format();
    } else if (strcmp(selected_node->key, "set_time_manual") == 0) {
        system_ui_set_time_manual();
    } else if (strcmp(selected_node->key, "set_date_manual") == 0) {
        system_ui_set_date_manual();
    } else if (strcmp(selected_node->key, "power_off") == 0) {
        system_ui_power_off();
    } else if (strcmp(selected_node->key, "alarm") == 0) {
        handle_alarm();
    } else if (strcmp(selected_node->key, "voice_recorder") == 0) {
        system_ui_voice_recorder();
    } else if (strcmp(selected_node->key, "display_free_memory") == 0) {
        system_ui_display_free_memory();
    } else if (strcmp(selected_node->key, "display_time_date") == 0) {
        system_ui_display_time_date();
    } else if (strcmp(selected_node->key, "display_network_status") == 0) {
        system_ui_display_network_status();
    } else if (strcmp(selected_node->key, "display_power_status") == 0) {
        system_ui_display_power_status();
    } else if (strcmp(selected_node->key, "setup_internet") == 0) {
        wifi_manager_show_menu();
    } else if (strcmp(selected_node->key, "keys_manager") == 0) {
        keys_manager_show_menu();
    } else if (strcmp(selected_node->key, "internet_radio") == 0) {
        radio_ui_show_menu();
    } else if (strcmp(selected_node->key, "mp3_player") == 0 ||
               strcmp(selected_node->key, "media_player") == 0) {
        system_ui_mp3_player();
    } else if (strcmp(selected_node->key, "user_guide") == 0) {
        system_ui_show_user_guide();
    } else if (strcmp(selected_node->key, "about_sai") == 0) {
        system_ui_show_about_sai();
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
    } else if (app_is_descendant_of(selected_node, "database_manager")) {
        handle_database_manager(selected_node);
    }
}

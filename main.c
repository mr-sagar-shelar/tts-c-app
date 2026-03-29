#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "menu.h"
#include "config.h"

struct termios original_termios;

void reset_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(reset_terminal_mode);
    new_termios = original_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
}

#define KEY_UP 1001
#define KEY_DOWN 1002
#define KEY_ENTER 1003
#define KEY_ESC 1004

int read_key() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return 0;

    if (c == 27) { // Escape or start of sequence
        struct termios work_termios;
        tcgetattr(STDIN_FILENO, &work_termios);
        struct termios tmp_termios = work_termios;
        tmp_termios.c_cc[VMIN] = 0;
        tmp_termios.c_cc[VTIME] = 1; // 100ms timeout
        tcsetattr(STDIN_FILENO, TCSANOW, &tmp_termios);

        char seq[2];
        int n = read(STDIN_FILENO, &seq[0], 1);
        if (n <= 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &work_termios);
            return KEY_ESC;
        }
        n = read(STDIN_FILENO, &seq[1], 1);
        
        tcsetattr(STDIN_FILENO, TCSANOW, &work_termios);

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
            }
        }
        return KEY_ESC;
    } else if (c == 10 || c == 13) {
        return KEY_ENTER;
    }
    return (unsigned char)c;
}

void get_user_input(char *buffer, int size, const char *prompt) {
    reset_terminal_mode();
    printf("\n%s: ", prompt);
    if (fgets(buffer, size, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
    }
    set_conio_terminal_mode();
}

void handle_settings(MenuNode *node, MenuNode *root) {
    if (strcmp(node->key, "language_switch") == 0) {
        char *current_lang = get_setting("language");
        if (!current_lang) current_lang = strdup("en");
        
        printf("\033[H\033[J");
        printf("--- %s ---\n", node->title);
        printf("Current Language: %s\n", current_lang);
        printf("\n1. English (en)\n");
        printf("2. Hindi (hn)\n");
        printf("\nSelect (1 or 2), or Esc to go back.");
        fflush(stdout);

        while (1) {
            int key = read_key();
            if (key == '1') {
                save_setting("language", "en");
                set_language(root, "en");
                break;
            } else if (key == '2') {
                save_setting("language", "hn");
                set_language(root, "hn");
                break;
            } else if (key == KEY_ESC) {
                break;
            }
        }
        free(current_lang);
    } else {
        char *current_val = get_setting(node->key);
        printf("\033[H\033[J");
        printf("--- %s ---\n", node->title);
        printf("Current Value: %s\n", current_val ? current_val : "Not set");
        if (current_val) free(current_val);
        
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

int main() {
    init_config();
    MenuNode *root = load_menu_from_json("menu.json");
    if (!root) {
        fprintf(stderr, "Failed to load menu.json\n");
        return 1;
    }

    char *lang = get_setting("language");
    if (lang) {
        set_language(root, lang);
        free(lang);
    } else {
        set_language(root, "en");
    }

    set_conio_terminal_mode();

    MenuNode *current_node = root;
    int selected_index = 0;

    while (1) {
        print_menu(current_node, selected_index);

        int key = read_key();
        if (key == KEY_ESC) {
            if (current_node->parent) {
                MenuNode *parent = current_node->parent;
                for (int i = 0; i < parent->num_items; i++) {
                    if (parent->items[i] == current_node) {
                        selected_index = i;
                        break;
                    }
                }
                current_node = parent;
            } else {
                printf("\nExiting...\n");
                break;
            }
        } else if (key == KEY_UP) {
            if (selected_index > 0) selected_index--;
        } else if (key == KEY_DOWN) {
            if (selected_index < current_node->num_items - 1) selected_index++;
        } else if (key == KEY_ENTER) {
            if (current_node->num_items > 0) {
                MenuNode *selected_node = current_node->items[selected_index];
                if (selected_node->num_items > 0) {
                    current_node = selected_node;
                    selected_index = 0;
                } else {
                    MenuNode *temp = selected_node;
                    int is_settings = 0;
                    while (temp) {
                        if (strcmp(temp->key, "settings") == 0) {
                            is_settings = 1;
                            break;
                        }
                        temp = temp->parent;
                    }
                    if (is_settings) {
                        handle_settings(selected_node, root);
                    }
                }
            }
        } else if (key > 0 && key < 1000) {
            // Check for shortcut
            for (int i = 0; i < current_node->num_items; i++) {
                if (current_node->items[i]->shortcut == tolower(key)) {
                    selected_index = i;
                    // Auto-enter if it has items
                    if (current_node->items[i]->num_items > 0) {
                        current_node = current_node->items[i];
                        selected_index = 0;
                    } else {
                        // Check if it's a setting
                        MenuNode *temp = current_node->items[i];
                        int is_settings = 0;
                        while (temp) {
                            if (strcmp(temp->key, "settings") == 0) {
                                is_settings = 1;
                                break;
                            }
                            temp = temp->parent;
                        }
                        if (is_settings) {
                            handle_settings(current_node->items[i], root);
                        }
                    }
                    break;
                }
            }
        }
    }

    free_menu(root);
    cleanup_config();
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "menu.h"

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
        if (n == 0) {
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
    return c;
}

int main() {
    MenuNode *root = load_menu_from_json("menu.json");
    if (!root) {
        fprintf(stderr, "Failed to load menu.json\n");
        return 1;
    }

    set_conio_terminal_mode();

    MenuNode *current_node = root;
    int selected_index = 0;

    while (1) {
        print_menu(current_node, selected_index);

        int key = read_key();
        if (key == KEY_ESC) {
            if (current_node->parent) {
                // Find index of current_node in parent
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
                    // Action: for now just print it
                    // printf("\nSelected: %s\n", selected_node->title);
                    // sleep(1);
                }
            }
        }
    }

    free_menu(root);
    return 0;
}

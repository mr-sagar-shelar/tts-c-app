#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "menu.h"
#include "config.h"

#define USER_SPACE "UserSpace"

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
#define KEY_BACKSPACE 127

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
    } else if (c == 127 || c == 8) {
        return KEY_BACKSPACE;
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

typedef struct {
    char name[256];
    int is_dir;
} FileEntry;

char* file_navigator(const char *start_path, int select_dir_only) {
    char current_path[1024];
    char abs_base[1024];
    realpath(USER_SPACE, abs_base);
    
    if (start_path && strlen(start_path) > 0) {
        strcpy(current_path, start_path);
    } else {
        strcpy(current_path, USER_SPACE);
    }

    while (1) {
        DIR *dir = opendir(current_path);
        if (!dir) return NULL;

        FileEntry entries[256];
        int count = 0;

        char abs_current[1024];
        realpath(current_path, abs_current);
        
        if (strcmp(abs_current, abs_base) != 0) {
            strcpy(entries[count].name, "..");
            entries[count].is_dir = 1;
            count++;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && count < 256) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            strncpy(entries[count].name, entry->d_name, 255);
            
            char full_path[2048];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                entries[count].is_dir = S_ISDIR(st.st_mode);
            } else {
                entries[count].is_dir = 0;
            }
            count++;
        }
        closedir(dir);

        int selected = 0;
        int confirmed = 0;
        while (!confirmed) {
            printf("\033[H\033[J");
            printf("--- Navigator: %s ---\n", current_path);
            if (select_dir_only) printf("[Press SPACE to select current directory]\n");
            printf("--------------------------------------------\n");
            if (count == 0) {
                printf("  (Empty directory)\n");
            }
            for (int i = 0; i < count; i++) {
                if (i == selected) printf("> [%s] %s\n", entries[i].is_dir ? "DIR" : "FILE", entries[i].name);
                else printf("  [%s] %s\n", entries[i].is_dir ? "DIR" : "FILE", entries[i].name);
            }
            fflush(stdout);

            int key = read_key();
            if (key == KEY_UP && selected > 0) selected--;
            else if (key == KEY_DOWN && selected < count - 1) selected++;
            else if (key == KEY_ENTER && count > 0) confirmed = 1;
            else if (key == KEY_ESC) return NULL;
            else if (key == ' ' && select_dir_only) {
                return strdup(current_path);
            }
        }

        if (strcmp(entries[selected].name, "..") == 0) {
            char *last_slash = strrchr(current_path, '/');
            if (last_slash && last_slash != current_path) {
                *last_slash = '\0';
            } else {
                strcpy(current_path, USER_SPACE);
            }
        } else {
            char next_path[2048];
            snprintf(next_path, sizeof(next_path), "%s/%s", current_path, entries[selected].name);
            if (entries[selected].is_dir) {
                strcpy(current_path, next_path);
            } else {
                if (!select_dir_only) return strdup(next_path);
            }
        }
    }
}

void handle_notepad_save(const char *buffer, char *filename) {
    if (strlen(filename) == 0) {
        printf("\nSelect target directory for saving...");
        fflush(stdout);
        sleep(1);
        char *dir_path = file_navigator(USER_SPACE, 1);
        if (dir_path) {
            char name[256];
            get_user_input(name, 256, "Enter filename");
            if (strlen(name) > 0) {
                snprintf(filename, 256, "%s/%s", dir_path, name);
            }
            free(dir_path);
        }
    }
    if (strlen(filename) > 0) {
        FILE *f = fopen(filename, "w");
        if (f) {
            fputs(buffer, f);
            fclose(f);
            printf("\nFile saved as '%s'.", filename);
        } else {
            printf("\nError saving file.");
        }
    }
}

void handle_notepad(const char *initial_content, const char *initial_filename) {
    char buffer[4096] = {0};
    char current_filename[256] = {0};
    if (initial_content) strncpy(buffer, initial_content, sizeof(buffer)-1);
    if (initial_filename) strncpy(current_filename, initial_filename, sizeof(current_filename)-1);
    
    int pos = strlen(buffer);
    
    while (1) {
        printf("\033[H\033[J");
        printf("--- Notepad: %s ---\n", strlen(current_filename) > 0 ? current_filename : "Untitled");
        printf("Type text. Press Enter for Menu, Esc to Exit without saving.\n");
        printf("----------------------------------------------------------\n");
        printf("%s", buffer);
        fflush(stdout);

        int in_menu = 0;
        while (!in_menu) {
            int key = read_key();
            if (key == KEY_ESC) {
                return;
            } else if (key == KEY_ENTER) {
                in_menu = 1;
            } else if (key == KEY_BACKSPACE) {
                if (pos > 0) {
                    pos--;
                    buffer[pos] = 0;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (key > 0 && key < 1000) {
                if (pos < (int)sizeof(buffer) - 1) {
                    buffer[pos++] = (char)key;
                    printf("%c", (char)key);
                    fflush(stdout);
                }
            }
        }

        int menu_sel = 0;
        const char *options[] = {"Save", "Save As", "Exit to Editor", "Close Without Saving"};
        int num_options = 4;

        while (in_menu) {
            printf("\033[H\033[J");
            printf("--- Notepad Menu ---\n");
            for (int i = 0; i < num_options; i++) {
                if (i == menu_sel) printf("> %s\n", options[i]);
                else printf("  %s\n", options[i]);
            }
            fflush(stdout);

            int key = read_key();
            if (key == KEY_UP) {
                if (menu_sel > 0) menu_sel--;
            } else if (key == KEY_DOWN) {
                if (menu_sel < num_options - 1) menu_sel++;
            } else if (key == KEY_ENTER) {
                if (menu_sel == 0) { // Save
                    handle_notepad_save(buffer, current_filename);
                    printf("\nPress any key to continue..."); fflush(stdout); read_key();
                    in_menu = 0;
                } else if (menu_sel == 1) { // Save As
                    char new_name[256] = {0};
                    handle_notepad_save(buffer, new_name);
                    if (strlen(new_name) > 0) strcpy(current_filename, new_name);
                    printf("\nPress any key to continue..."); fflush(stdout); read_key();
                    in_menu = 0;
                } else if (menu_sel == 2) { // Exit to Editor
                    in_menu = 0;
                } else if (menu_sel == 3) { // Close
                    return;
                }
            } else if (key == KEY_ESC) {
                in_menu = 0;
            }
        }
    }
}

void handle_file_viewer(const char *filename) {
    FILE *f = fopen(filename, "r");
    printf("\033[H\033[J");
    printf("--- File Viewer: %s ---\n", filename);
    printf("----------------------------------\n");
    if (!f) {
        printf("Error: Could not open file.\n");
    } else {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            printf("%s", line);
        }
        fclose(f);
    }
    printf("\n\nPress any key to go back...");
    fflush(stdout);
    read_key();
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

void handle_file_manager(MenuNode *node) {
    if (strcmp(node->key, "fm_browse") == 0) {
        char *path = file_navigator(USER_SPACE, 0);
        if (path) {
            struct stat st;
            if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                handle_file_viewer(path);
            }
            free(path);
        }
    } else if (strcmp(node->key, "fm_new_folder") == 0) {
        char *dir = file_navigator(USER_SPACE, 1);
        if (dir) {
            char name[256];
            get_user_input(name, 256, "Enter folder name");
            if (strlen(name) > 0) {
                char full[1024];
                snprintf(full, sizeof(full), "%s/%s", dir, name);
                mkdir(full, 0777);
                printf("\nFolder created. Press any key..."); fflush(stdout); read_key();
            }
            free(dir);
        }
    } else if (strcmp(node->key, "fm_new_file") == 0) {
        char *dir = file_navigator(USER_SPACE, 1);
        if (dir) {
            char name[256];
            get_user_input(name, 256, "Enter file name");
            if (strlen(name) > 0) {
                char full[1024];
                snprintf(full, sizeof(full), "%s/%s", dir, name);
                FILE *f = fopen(full, "w");
                if (f) fclose(f);
                printf("\nFile created. Press any key..."); fflush(stdout); read_key();
            }
            free(dir);
        }
    } else if (strcmp(node->key, "fm_rename") == 0) {
        char *path = file_navigator(USER_SPACE, 0);
        if (path) {
            char new_name[256];
            get_user_input(new_name, 256, "Enter new name (no path)");
            if (strlen(new_name) > 0) {
                char *last_slash = strrchr(path, '/');
                char new_path[1024] = {0};
                if (last_slash) {
                    strncpy(new_path, path, last_slash - path + 1);
                    strcat(new_path, new_name);
                } else {
                    strcpy(new_path, new_name);
                }
                rename(path, new_path);
                printf("\nRenamed. Press any key..."); fflush(stdout); read_key();
            }
            free(path);
        }
    } else if (strcmp(node->key, "fm_delete") == 0) {
        char *path = file_navigator(USER_SPACE, 0);
        if (path) {
            printf("\nAre you sure you want to delete %s? (y/n)", path);
            fflush(stdout);
            int k = read_key();
            if (k == 'y' || k == 'Y') {
                remove(path); // Works for files and empty dirs
                printf("\nDeleted. Press any key..."); fflush(stdout); read_key();
            }
            free(path);
        }
    }
}

int main() {
    init_config();
    mkdir(USER_SPACE, 0777);
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
                    if (strcmp(selected_node->key, "notepad_new") == 0) {
                        handle_notepad(NULL, NULL);
                    } else if (strcmp(selected_node->key, "notepad_open") == 0 || strcmp(selected_node->key, "wp_open") == 0) {
                        char *selected_path = file_navigator(USER_SPACE, 0);
                        if (selected_path) {
                            if (strcmp(selected_node->key, "notepad_open") == 0) {
                                FILE *f = fopen(selected_path, "r");
                                if (f) {
                                    char content[4096] = {0};
                                    fread(content, 1, sizeof(content)-1, f);
                                    fclose(f);
                                    handle_notepad(content, selected_path);
                                } else {
                                    printf("\nError opening file. Press any key..."); fflush(stdout); read_key();
                                }
                            } else {
                                handle_file_viewer(selected_path);
                            }
                            free(selected_path);
                        }
                    } else {
                        MenuNode *temp = selected_node;
                        int is_settings = 0;
                        int is_fm = 0;
                        while (temp) {
                            if (strcmp(temp->key, "settings") == 0) is_settings = 1;
                            if (strcmp(temp->key, "file_manager") == 0) is_fm = 1;
                            temp = temp->parent;
                        }
                        if (is_settings) {
                            handle_settings(selected_node, root);
                        } else if (is_fm) {
                            handle_file_manager(selected_node);
                        }
                    }
                }
            }
        } else if (key > 0 && key < 1000) {
            for (int i = 0; i < current_node->num_items; i++) {
                if (current_node->items[i]->shortcut == tolower(key)) {
                    selected_index = i;
                    if (current_node->items[i]->num_items > 0) {
                        current_node = current_node->items[i];
                        selected_index = 0;
                    } else {
                        if (strcmp(current_node->items[i]->key, "notepad_new") == 0) {
                            handle_notepad(NULL, NULL);
                        } else {
                            MenuNode *temp = current_node->items[i];
                            int is_settings = 0;
                            int is_fm = 0;
                            while (temp) {
                                if (strcmp(temp->key, "settings") == 0) is_settings = 1;
                                if (strcmp(temp->key, "file_manager") == 0) is_fm = 1;
                                temp = temp->parent;
                            }
                            if (is_settings) {
                                handle_settings(current_node->items[i], root);
                            } else if (is_fm) {
                                handle_file_manager(current_node->items[i]);
                            }
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

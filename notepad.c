#include "notepad.h"
#include "menu.h"

static void handle_notepad_save(const char *buffer, char *filename) {
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
    
    int pos = (int)strlen(buffer);
    
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

void handle_notepad_search() {
    char query[256];
    get_user_input(query, sizeof(query), menu_translate("ui_enter_search_term", "Enter search term"));
    if (strlen(query) == 0) return;

    SearchResult results[100];
    int count = 0;
    recursive_file_search(USER_SPACE, query, results, &count, 100);

    if (count == 0) {
        printf("\n%s", menu_translate("ui_no_results_found_press_any_key", "No results found. Press any key...")); fflush(stdout); read_key();
        return;
    }

    int sel = 0;
    while (1) {
        printf("\033[H\033[J--- Notepad: Open Search: %s ---\n", query);
        for (int i = 0; i < count; i++) {
            if (i == sel) printf("> [%s] %s\n", results[i].is_dir ? "DIR" : "FILE", results[i].path);
            else printf("  [%s] %s\n", results[i].is_dir ? "DIR" : "FILE", results[i].path);
        }
        fflush(stdout);
        int key = read_key();
        if (key == KEY_UP && sel > 0) sel--;
        else if (key == KEY_DOWN && sel < count - 1) sel++;
        else if (key == KEY_ENTER) {
            if (results[sel].is_dir) {
                printf("\nCannot open a directory in Notepad. Press any key..."); fflush(stdout); read_key();
            } else {
                FILE *f = fopen(results[sel].path, "r");
                if (f) {
                    char content[4096] = {0};
                    fread(content, 1, sizeof(content)-1, f);
                    fclose(f);
                    handle_notepad(content, results[sel].path);
                } else {
                    printf("\nError opening file. Press any key..."); fflush(stdout); read_key();
                }
                break;
            }
        } else if (key == KEY_ESC) break;
    }
}

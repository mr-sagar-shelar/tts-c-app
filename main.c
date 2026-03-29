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
#include "contacts.h"

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
#define KEY_TAB 9

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
    } else if (c == 9) {
        return KEY_TAB;
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

typedef struct {
    char path[1024];
    int is_dir;
} SearchResult;

void recursive_file_search(const char *base_path, const char *pattern, SearchResult *results, int *count, int max_results) {
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_results) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        // Case-insensitive search
        char lower_name[256] = {0}, lower_pattern[256] = {0};
        for(int i=0; entry->d_name[i] && i < 255; i++) lower_name[i] = tolower(entry->d_name[i]);
        for(int i=0; pattern[i] && i < 255; i++) lower_pattern[i] = tolower(pattern[i]);

        if (strstr(lower_name, lower_pattern) != NULL) {
            strncpy(results[*count].path, full_path, 1023);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                results[*count].is_dir = S_ISDIR(st.st_mode);
            }
            (*count)++;
        }

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            recursive_file_search(full_path, pattern, results, count, max_results);
        }
    }
    closedir(dir);
}

void handle_fm_search() {
    char query[256];
    get_user_input(query, sizeof(query), "Enter search term");
    if (strlen(query) == 0) return;

    SearchResult results[100];
    int count = 0;
    recursive_file_search(USER_SPACE, query, results, &count, 100);

    if (count == 0) {
        printf("\nNo results found. Press any key..."); fflush(stdout); read_key();
        return;
    }

    int sel = 0;
    while (1) {
        printf("\033[H\033[J--- Search Results: %s ---\n", query);
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
                char *path = file_navigator(results[sel].path, 0);
                if (path) {
                    struct stat st;
                    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                        handle_file_viewer(path);
                    }
                    free(path);
                }
            } else {
                handle_file_viewer(results[sel].path);
            }
            break;
        } else if (key == KEY_ESC) break;
    }
}

void handle_notepad_search() {
    char query[256];
    get_user_input(query, sizeof(query), "Enter search term");
    if (strlen(query) == 0) return;

    SearchResult results[100];
    int count = 0;
    recursive_file_search(USER_SPACE, query, results, &count, 100);

    // Filter results to show files only? Or let user pick. Notepad usually opens files.
    if (count == 0) {
        printf("\nNo results found. Press any key..."); fflush(stdout); read_key();
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
    } else if (strcmp(node->key, "fm_search") == 0) {
        handle_fm_search();
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
            if (is_edit) {
                // Already updated in buffers
            }
            return; // Caller will save
        } else if (key == KEY_ESC) {
            memset(c, 0, sizeof(Contact));
            return;
        } else if (key == KEY_BACKSPACE) {
            int len = strlen(buffers[field]);
            if (len > 0) buffers[field][len-1] = '\0';
        } else if (key > 0 && key < 1000) {
            int len = strlen(buffers[field]);
            if (len < sizes[field] - 1) {
                buffers[field][len] = (char)key;
                buffers[field][len+1] = '\0';
            }
        }
    }
}

void handle_dictionary() {
    char *lang = get_setting("language");
    if (!lang) lang = strdup("en");

    char dict_file[32];
    snprintf(dict_file, sizeof(dict_file), "dict_%s.json", lang);
    free(lang);

    FILE *f = fopen(dict_file, "rb");
    if (!f) {
        printf("\033[H\033[J--- Sense Dictionary ---\nDictionary file '%s' not found. Press any key...", dict_file);
        fflush(stdout); read_key();
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *dict_json = cJSON_Parse(data);
    free(data);
    if (!dict_json) {
        printf("\033[H\033[J--- Sense Dictionary ---\nError parsing dictionary file. Press any key...");
        fflush(stdout); read_key();
        return;
    }

    cJSON *words_array = cJSON_GetObjectItemCaseSensitive(dict_json, "words");
    int total_words = cJSON_GetArraySize(words_array);

    char search_term[256] = {0};
    int sel = 0;

    while (1) {
        printf("\033[H\033[J--- Sense Dictionary ---\n");
        printf("Search: %s_\n", search_term);
        printf("---------------------------\n");

        int matches[512];
        int match_count = 0;
        char lower_search[256] = {0};
        for(int i=0; search_term[i]; i++) lower_search[i] = (char)tolower(search_term[i]);

        for (int i = 0; i < total_words && match_count < 512; i++) {
            cJSON *item = cJSON_GetArrayItem(words_array, i);
            const char *word = cJSON_GetObjectItemCaseSensitive(item, "word")->valuestring;
            
            char lower_word[256] = {0};
            for(int j=0; word[j] && j < 255; j++) lower_word[j] = (char)tolower(word[j]);

            if (strlen(lower_search) == 0 || strstr(lower_word, lower_search) != NULL) {
                matches[match_count++] = i;
            }
        }

        if (sel >= match_count) sel = (match_count > 0) ? match_count - 1 : 0;

        if (match_count == 0) {
            printf("  (No matching words)\n");
        } else {
            for (int i = 0; i < match_count && i < 15; i++) { // Show up to 15 results
                cJSON *item = cJSON_GetArrayItem(words_array, matches[i]);
                const char *word = cJSON_GetObjectItemCaseSensitive(item, "word")->valuestring;
                if (i == sel) printf("> %s\n", word);
                else printf("  %s\n", word);
            }
            if (match_count > 15) printf("  ... and %d more\n", match_count - 15);
        }

        fflush(stdout);
        int key = read_key();

        if (key == KEY_ESC) break;
        else if (key == KEY_UP && sel > 0) sel--;
        else if (key == KEY_DOWN && sel < match_count - 1) sel++;
        else if (key == KEY_ENTER && match_count > 0) {
            cJSON *item = cJSON_GetArrayItem(words_array, matches[sel]);
            const char *word = cJSON_GetObjectItemCaseSensitive(item, "word")->valuestring;
            const char *meaning = cJSON_GetObjectItemCaseSensitive(item, "meaning")->valuestring;
            printf("\033[H\033[J--- Word Detail ---\n\nWord: %s\n\nMeaning:\n%s\n\nPress any key to return...", word, meaning);
            fflush(stdout); read_key();
        } else if (key == KEY_BACKSPACE) {
            int slen = (int)strlen(search_term);
            if (slen > 0) search_term[slen - 1] = '\0';
            sel = 0;
        } else if (key > 0 && key < 1000 && isprint(key)) {
            int slen = (int)strlen(search_term);
            if (slen < 254) {
                search_term[slen] = (char)key;
                search_term[slen + 1] = '\0';
            }
            sel = 0;
        }
    }

    cJSON_Delete(dict_json);
}

void handle_online_dictionary() {
    char word[256];
    get_user_input(word, sizeof(word), "Enter word to search online");
    if (strlen(word) == 0) return;

    char *lang = get_setting("language");
    char api_lang[16] = "en";
    if (lang) {
        if (strcmp(lang, "hn") == 0 || strcmp(lang, "hi") == 0) {
            strcpy(api_lang, "hi");
        } else {
            strncpy(api_lang, lang, sizeof(api_lang) - 1);
        }
        free(lang);
    }

    printf("\033[H\033[J--- Online Dictionary ---\nSearching for '%s' in %s...\n", word, api_lang);
    fflush(stdout);

    char url[512];
    snprintf(url, sizeof(url), "https://api.dictionaryapi.dev/api/v2/entries/%s/%s", api_lang, word);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s \"%s\"", url);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        printf("Failed to run curl command.\nPress any key..."); fflush(stdout); read_key();
        return;
    }

    char *response = NULL;
    size_t response_len = 0;
    size_t response_size = 4096;
    response = (char *)malloc(response_size);
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t len = strlen(buffer);
        if (response_len + len + 1 > response_size) {
            response_size *= 2;
            response = (char *)realloc(response, response_size);
        }
        strcpy(response + response_len, buffer);
        response_len += len;
    }
    pclose(fp);

    if (response_len == 0) {
        printf("\nNo response from server. Check internet connection.\nPress any key...");
        fflush(stdout); read_key();
        free(response);
        return;
    }

    cJSON *json = cJSON_Parse(response);
    free(response);

    if (!json) {
        printf("\nError parsing JSON response.\nPress any key...");
        fflush(stdout); read_key();
        return;
    }

    // Check if error response (not an array)
    if (!cJSON_IsArray(json)) {
        cJSON *title = cJSON_GetObjectItemCaseSensitive(json, "title");
        cJSON *message = cJSON_GetObjectItemCaseSensitive(json, "message");
        printf("\033[H\033[J--- Online Dictionary ---\n\n");
        if (title && cJSON_IsString(title)) printf("%s\n\n", title->valuestring);
        if (message && cJSON_IsString(message)) printf("%s\n", message->valuestring);
        printf("\nPress any key to go back...");
        fflush(stdout); read_key();
        cJSON_Delete(json);
        return;
    }

    // Parse successful response
    cJSON *first_entry = cJSON_GetArrayItem(json, 0);
    cJSON *word_node = cJSON_GetObjectItemCaseSensitive(first_entry, "word");
    cJSON *phonetics = cJSON_GetObjectItemCaseSensitive(first_entry, "phonetics");
    cJSON *meanings = cJSON_GetObjectItemCaseSensitive(first_entry, "meanings");

    char audio_url[1024] = {0};
    if (cJSON_IsArray(phonetics)) {
        int phonetics_count = cJSON_GetArraySize(phonetics);
        for (int i = 0; i < phonetics_count; i++) {
            cJSON *p = cJSON_GetArrayItem(phonetics, i);
            cJSON *a = cJSON_GetObjectItemCaseSensitive(p, "audio");
            if (a && cJSON_IsString(a) && strlen(a->valuestring) > 0) {
                strncpy(audio_url, a->valuestring, sizeof(audio_url) - 1);
                break;
            }
        }
    }

    printf("\033[H\033[J--- Online Dictionary: %s ---\n\n", word_node && cJSON_IsString(word_node) ? word_node->valuestring : word);
    
    if (cJSON_IsArray(meanings)) {
        int meanings_count = cJSON_GetArraySize(meanings);
        for (int i = 0; i < meanings_count && i < 3; i++) { // Show up to 3 meanings
            cJSON *m = cJSON_GetArrayItem(meanings, i);
            cJSON *pos = cJSON_GetObjectItemCaseSensitive(m, "partOfSpeech");
            cJSON *defs = cJSON_GetObjectItemCaseSensitive(m, "definitions");
            
            if (pos && cJSON_IsString(pos)) {
                printf("[%s]\n", pos->valuestring);
            }
            if (cJSON_IsArray(defs)) {
                cJSON *first_def = cJSON_GetArrayItem(defs, 0);
                cJSON *def_text = cJSON_GetObjectItemCaseSensitive(first_def, "definition");
                if (def_text && cJSON_IsString(def_text)) {
                    printf("  - %s\n\n", def_text->valuestring);
                }
            }
        }
    }

    if (strlen(audio_url) > 0) {
        printf("Press 'p' to play audio, Esc to go back.");
    } else {
        printf("Press any key to go back...");
    }
    fflush(stdout);

    while (1) {
        int key = read_key();
        if (key == KEY_ESC) {
            break;
        } else if (key == 'p' || key == 'P') {
            if (strlen(audio_url) > 0) {
                printf("\nPlaying audio...\n");
                fflush(stdout);
                char play_cmd[1024];
                snprintf(play_cmd, sizeof(play_cmd), "curl -s \"%s\" > /tmp/dict_audio.mp3 && afplay /tmp/dict_audio.mp3", audio_url);
                system(play_cmd);
                printf("Done. Press Esc to go back or 'p' to replay.");
                fflush(stdout);
            }
        } else if (strlen(audio_url) == 0) {
            break; // Exit on any key if no audio
        }
    }

    cJSON_Delete(json);
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
        get_user_input(query, sizeof(query), "Enter search term");
        if (strlen(query) == 0) return;

        int total_count = get_contact_count();
        int matches[256];
        int match_count = 0;

        char lower_query[256] = {0};
        for(int i=0; query[i] && i < 255; i++) lower_query[i] = tolower(query[i]);

        for (int i = 0; i < total_count && match_count < 256; i++) {
            Contact *c = get_contact(i);
            char full_data[1024];
            snprintf(full_data, sizeof(full_data), "%s %s %s %s %s %s", 
                c->first_name, c->last_name, c->phone, c->email, c->address, c->postal_code);
            
            for(int j=0; full_data[j]; j++) full_data[j] = tolower(full_data[j]);

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

int main() {
    init_config();
    init_contacts();
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
                    } else if (strcmp(selected_node->key, "notepad_search") == 0) {
                        handle_notepad_search();
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
                        int is_contacts = 0;
                        while (temp) {
                            if (strcmp(temp->key, "settings") == 0) is_settings = 1;
                            if (strcmp(temp->key, "file_manager") == 0) is_fm = 1;
                            if (strcmp(temp->key, "address_manager") == 0) is_contacts = 1;
                            temp = temp->parent;
                        }
                        if (is_settings) {
                            handle_settings(selected_node, root);
                        } else if (is_fm) {
                            handle_file_manager(selected_node);
                        } else if (is_contacts) {
                            handle_address_manager(selected_node);
                        } else if (strcmp(selected_node->key, "sense_dictionary") == 0) {
                            handle_dictionary();
                        } else if (strcmp(selected_node->key, "online_dictionary") == 0) {
                            handle_online_dictionary();
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
                        } else if (strcmp(current_node->items[i]->key, "notepad_search") == 0) {
                            handle_notepad_search();
                        } else if (strcmp(current_node->items[i]->key, "sense_dictionary") == 0) {
                            handle_dictionary();
                        } else if (strcmp(current_node->items[i]->key, "online_dictionary") == 0) {
                            handle_online_dictionary();
                        } else {
                            MenuNode *temp = current_node->items[i];
                            int is_settings = 0;
                            int is_fm = 0;
                            int is_contacts = 0;
                            while (temp) {
                                if (strcmp(temp->key, "settings") == 0) is_settings = 1;
                                if (strcmp(temp->key, "file_manager") == 0) is_fm = 1;
                                if (strcmp(temp->key, "address_manager") == 0) is_contacts = 1;
                                temp = temp->parent;
                            }
                            if (is_settings) {
                                handle_settings(current_node->items[i], root);
                            } else if (is_fm) {
                                handle_file_manager(current_node->items[i]);
                            } else if (is_contacts) {
                                handle_address_manager(current_node->items[i]);
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
    cleanup_contacts();
    return 0;
}

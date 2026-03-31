#include "file_manager.h"
#include <ctype.h>

static char* file_navigator_internal(const char *start_path, int select_dir_only, int supported_only) {
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

            char full_path[2048];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                entries[count].is_dir = S_ISDIR(st.st_mode);
            } else {
                entries[count].is_dir = 0;
            }

            if (supported_only && !entries[count].is_dir && !document_is_supported_file(full_path)) {
                continue;
            }

            strncpy(entries[count].name, entry->d_name, 255);
            entries[count].name[255] = '\0';
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

char* file_navigator(const char *start_path, int select_dir_only) {
    return file_navigator_internal(start_path, select_dir_only, 0);
}

char* file_navigator_supported(const char *start_path) {
    return file_navigator_internal(start_path, 0, 1);
}

void handle_file_viewer(const char *filename) {
    char error[128] = {0};
    char *text = document_load_text(filename, error, sizeof(error));
    printf("\033[H\033[J");
    printf("--- File Viewer: %s ---\n", filename);
    printf("----------------------------------\n");
    if (!text) {
        printf("Error: %s\n", error[0] ? error : "Could not open file.");
    } else {
        printf("%s", text);
        free(text);
    }
    printf("\n\nPress any key to go back...");
    fflush(stdout);
    read_key();
}

void recursive_file_search(const char *base_path, const char *pattern, SearchResult *results, int *count, int max_results) {
    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_results) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        char lower_name[256] = {0}, lower_pattern[256] = {0};
        for(int i=0; entry->d_name[i] && i < 255; i++) lower_name[i] = (char)tolower(entry->d_name[i]);
        for(int i=0; pattern[i] && i < 255; i++) lower_pattern[i] = (char)tolower(pattern[i]);

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

void handle_file_manager(MenuNode *node) {
    if (strcmp(node->key, "fm_browse") == 0) {
        char *path = file_navigator_supported(USER_SPACE);
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
                remove(path);
                printf("\nDeleted. Press any key..."); fflush(stdout); read_key();
            }
            free(path);
        }
    }
}

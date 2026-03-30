#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "menu.h"
#include "utils.h"

#define USER_SPACE "UserSpace"

typedef struct {
    char name[256];
    int is_dir;
} FileEntry;

typedef struct {
    char path[1024];
    int is_dir;
} SearchResult;

/**
 * Interactive file navigator.
 * @param start_path The directory to start in.
 * @param select_dir_only If 1, returns a directory; if 0, returns a file or directory.
 * @return A newly allocated path string (must be freed) or NULL.
 */
char* file_navigator(const char *start_path, int select_dir_only);

/**
 * Displays the content of a file.
 * @param filename The path to the file.
 */
void handle_file_viewer(const char *filename);

/**
 * Recursively searches for files matching a pattern.
 */
void recursive_file_search(const char *base_path, const char *pattern, SearchResult *results, int *count, int max_results);

/**
 * UI handler for file searching.
 */
void handle_fm_search();

/**
 * Main dispatcher for file manager menu items.
 */
void handle_file_manager(MenuNode *node);

#endif /* FILE_MANAGER_H */

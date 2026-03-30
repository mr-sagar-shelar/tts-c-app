#ifndef NOTEPAD_H
#define NOTEPAD_H

#include "utils.h"
#include "file_manager.h"

/**
 * Handles the Notepad editor.
 * @param initial_content Content to load initially.
 * @param initial_filename Filename to associate with the buffer.
 */
void handle_notepad(const char *initial_content, const char *initial_filename);

/**
 * UI handler to search for files and open them in Notepad.
 */
void handle_notepad_search();

#endif /* NOTEPAD_H */

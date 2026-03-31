#ifndef TEXT_PROCESSOR_H
#define TEXT_PROCESSOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Structure to hold information about a text file being read word by word.
 */
typedef struct {
    FILE *file;
    char *current_word;
    char *whitespace;
    char *prepunctuation;
    char *postpunctuation;
} TextProcessor;

/**
 * Open a text file for word-by-word processing.
 * @param filename The path to the text file.
 * @return A pointer to a TextProcessor structure, or NULL on failure.
 */
TextProcessor* text_processor_open(const char *filename);

/**
 * Get the next word from the text processor.
 * @param tp The text processor.
 * @return The next word, or NULL if end of file is reached.
 */
char* text_processor_get_next_word(TextProcessor *tp);

/**
 * Close the text processor and free resources.
 * @param tp The text processor.
 */
void text_processor_close(TextProcessor *tp);

/**
 * Handle the word-by-word viewer UI.
 * @param filename The path to the text file to view.
 */
void handle_word_by_word_viewer(const char *filename);

#endif // TEXT_PROCESSOR_H

#ifndef TEXT_PROCESSOR_H
#define TEXT_PROCESSOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Structure to hold information about a text file being read word by word.
 */
typedef struct {
    char *text;           // Original text content
    char *whitespace;
    char *prepunctuation;
    char *word;
    char *postpunctuation;
    int start_pos;        // Start position in 'text'
    int end_pos;          // End position in 'text'
} Token;

typedef struct {
    char *full_text;      // Full content of the file
    Token *tokens;        // Array of tokens
    int num_tokens;
    int current_token_idx;
} TextProcessor;

/**
 * Open and tokenize a text file for word-by-word processing with highlighting.
 * @param filename The path to the text file.
 * @return A pointer to a TextProcessor structure, or NULL on failure.
 */
TextProcessor* text_processor_open(const char *filename);

/**
 * Get the next token from the text processor.
 * @param tp The text processor.
 * @return The next token, or NULL if end of file is reached.
 */
Token* text_processor_get_next_token(TextProcessor *tp);

/**
 * Get the previous token from the text processor.
 * @param tp The text processor.
 * @return The previous token, or NULL if at the start.
 */
Token* text_processor_get_prev_token(TextProcessor *tp);

/**
 * Close the text processor and free resources.
 * @param tp The text processor.
 */
void text_processor_close(TextProcessor *tp);

/**
 * Handle the word-by-word viewer UI with full text display and highlighting.
 * @param filename The path to the text file to view.
 */
void handle_word_by_word_viewer(const char *filename);

#endif // TEXT_PROCESSOR_H

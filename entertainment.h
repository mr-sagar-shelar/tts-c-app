#ifndef ENTERTAINMENT_H
#define ENTERTAINMENT_H

#include "utils.h"
#include "cJSON.h"

/**
 * UI handler for jokes.
 */
void handle_joke();

/**
 * UI handler for short stories.
 */
void handle_short_stories();

/**
 * UI handler for poems.
 */
void handle_poems();

/**
 * UI handler for the text reader prototype that tokenizes files word by word.
 */
void handle_word_by_word_viewer();

#endif /* ENTERTAINMENT_H */

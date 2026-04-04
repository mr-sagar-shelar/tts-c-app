#ifndef ENTERTAINMENT_H
#define ENTERTAINMENT_H

#include "utils.h"
#include "cJSON.h"

/**
 * UI handler for jokes.
 */
void content_ui_show_joke(void);

/**
 * UI handler for short stories.
 */
void content_ui_show_short_stories(void);

/**
 * UI handler for poems.
 */
void content_ui_show_poems(void);

/**
 * UI handler for the text reader prototype that tokenizes files word by word.
 */
void content_ui_run_word_viewer(void);

/**
 * Displays arbitrary text with automatic speech playback and pause/resume support.
 */
void content_ui_show_spoken_text(const char *title, const char *source_name, const char *text);

#endif /* ENTERTAINMENT_H */

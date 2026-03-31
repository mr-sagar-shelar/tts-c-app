#ifndef TEXT_PROCESSOR_H
#define TEXT_PROCESSOR_H

#include <stddef.h>

typedef struct {
    char *whitespace;
    char *prepunctuation;
    char *word;
    char *postpunctuation;
    char *surface;
    size_t raw_offset;
    size_t content_offset;
    int line;
    int column;
} TextWord;

typedef struct {
    char *file_path;
    char *content;
    TextWord *tokens;
    size_t token_count;
} TextProcessor;

TextProcessor *text_processor_load(const char *filename);
void text_processor_free(TextProcessor *processor);
const TextWord *text_processor_get_word(const TextProcessor *processor, size_t index);

#endif /* TEXT_PROCESSOR_H */

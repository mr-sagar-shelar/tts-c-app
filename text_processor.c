#include "text_processor.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_prepunct_char(unsigned char ch) {
    return strchr("\"'`([{<", ch) != NULL;
}

static int is_postpunct_char(unsigned char ch) {
    return strchr(".,!?;:)]}\"'>", ch) != NULL;
}

static int is_word_char(unsigned char ch) {
    return isalnum(ch) || ch == '\'' || ch == '-' || ch == '_';
}

static char *duplicate_range(const char *start, size_t length) {
    char *copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    if (length > 0) {
        memcpy(copy, start, length);
    }
    copy[length] = '\0';
    return copy;
}

static char *join_segments(const char *a, const char *b, const char *c, const char *d) {
    size_t a_len = strlen(a);
    size_t b_len = strlen(b);
    size_t c_len = strlen(c);
    size_t d_len = strlen(d);
    size_t total = a_len + b_len + c_len + d_len;
    char *joined = (char *)malloc(total + 1);

    if (!joined) {
        return NULL;
    }

    memcpy(joined, a, a_len);
    memcpy(joined + a_len, b, b_len);
    memcpy(joined + a_len + b_len, c, c_len);
    memcpy(joined + a_len + b_len + c_len, d, d_len);
    joined[total] = '\0';

    return joined;
}

static void free_token_fields(TextWord *token) {
    free(token->whitespace);
    free(token->prepunctuation);
    free(token->word);
    free(token->postpunctuation);
    free(token->surface);
}

static void advance_position(unsigned char ch, int *line, int *column) {
    if (ch == '\n') {
        (*line)++;
        *column = 1;
    } else {
        (*column)++;
    }
}

static int push_token(TextProcessor *processor, size_t *capacity, const TextWord *token) {
    if (processor->token_count == *capacity) {
        size_t new_capacity = (*capacity == 0) ? 64 : (*capacity * 2);
        TextWord *grown = (TextWord *)realloc(processor->tokens, new_capacity * sizeof(TextWord));
        if (!grown) {
            return 0;
        }
        processor->tokens = grown;
        *capacity = new_capacity;
    }

    processor->tokens[processor->token_count++] = *token;
    return 1;
}

static int parse_tokens(TextProcessor *processor) {
    size_t capacity = 0;
    size_t pos = 0;
    int line = 1;
    int column = 1;
    size_t length = strlen(processor->content);

    if (length >= 3 &&
        (unsigned char)processor->content[0] == 0xEF &&
        (unsigned char)processor->content[1] == 0xBB &&
        (unsigned char)processor->content[2] == 0xBF) {
        pos = 3;
        column = 4;
    }

    while (pos < length) {
        size_t raw_offset = pos;
        size_t whitespace_start = pos;
        size_t pre_start;
        size_t word_start;
        size_t post_start;
        size_t content_offset;
        int word_line;
        int word_column;
        TextWord token;

        memset(&token, 0, sizeof(token));

        while (pos < length && isspace((unsigned char)processor->content[pos])) {
            advance_position((unsigned char)processor->content[pos], &line, &column);
            pos++;
        }

        if (pos >= length) {
            break;
        }

        content_offset = pos;
        word_line = line;
        word_column = column;
        pre_start = pos;
        while (pos < length && is_prepunct_char((unsigned char)processor->content[pos])) {
            advance_position((unsigned char)processor->content[pos], &line, &column);
            pos++;
        }

        word_start = pos;
        while (pos < length && is_word_char((unsigned char)processor->content[pos])) {
            advance_position((unsigned char)processor->content[pos], &line, &column);
            pos++;
        }

        if (pos == word_start) {
            while (pos < length &&
                   !isspace((unsigned char)processor->content[pos]) &&
                   !is_prepunct_char((unsigned char)processor->content[pos]) &&
                   !is_postpunct_char((unsigned char)processor->content[pos])) {
                advance_position((unsigned char)processor->content[pos], &line, &column);
                pos++;
            }

            if (pos == word_start && pos < length && !isspace((unsigned char)processor->content[pos])) {
                advance_position((unsigned char)processor->content[pos], &line, &column);
                pos++;
            }
        }

        post_start = pos;
        while (pos < length && is_postpunct_char((unsigned char)processor->content[pos])) {
            advance_position((unsigned char)processor->content[pos], &line, &column);
            pos++;
        }

        token.whitespace = duplicate_range(processor->content + whitespace_start, content_offset - whitespace_start);
        token.prepunctuation = duplicate_range(processor->content + pre_start, word_start - pre_start);
        token.word = duplicate_range(processor->content + word_start, post_start - word_start);
        token.postpunctuation = duplicate_range(processor->content + post_start, pos - post_start);

        if (!token.whitespace || !token.prepunctuation || !token.word || !token.postpunctuation) {
            free_token_fields(&token);
            return 0;
        }

        token.surface = join_segments(
            token.whitespace,
            token.prepunctuation,
            token.word,
            token.postpunctuation
        );
        if (!token.surface) {
            free_token_fields(&token);
            return 0;
        }

        token.raw_offset = raw_offset;
        token.content_offset = content_offset;
        token.line = word_line;
        token.column = word_column;

        if (!push_token(processor, &capacity, &token)) {
            free_token_fields(&token);
            return 0;
        }
    }

    return 1;
}

TextProcessor *text_processor_load(const char *filename) {
    FILE *file;
    long file_size;
    size_t read_size;
    TextProcessor *processor;

    if (!filename) {
        return NULL;
    }

    file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    processor = (TextProcessor *)calloc(1, sizeof(TextProcessor));
    if (!processor) {
        fclose(file);
        return NULL;
    }

    processor->content = (char *)malloc((size_t)file_size + 1);
    processor->file_path = strdup(filename);
    if (!processor->content || !processor->file_path) {
        text_processor_free(processor);
        fclose(file);
        return NULL;
    }

    read_size = fread(processor->content, 1, (size_t)file_size, file);
    processor->content[read_size] = '\0';
    fclose(file);

    if (!parse_tokens(processor)) {
        text_processor_free(processor);
        return NULL;
    }

    return processor;
}

void text_processor_free(TextProcessor *processor) {
    size_t i;

    if (!processor) {
        return;
    }

    for (i = 0; i < processor->token_count; i++) {
        free_token_fields(&processor->tokens[i]);
    }

    free(processor->tokens);
    free(processor->content);
    free(processor->file_path);
    free(processor);
}

const TextWord *text_processor_get_word(const TextProcessor *processor, size_t index) {
    if (!processor || index >= processor->token_count) {
        return NULL;
    }

    return &processor->tokens[index];
}

#include "text_processor.h"
#include "utils.h"
#include <ctype.h>

TextProcessor* text_processor_open(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    TextProcessor *tp = (TextProcessor*)malloc(sizeof(TextProcessor));
    tp->file = file;
    tp->current_word = NULL;
    tp->whitespace = (char*)malloc(256);
    tp->prepunctuation = (char*)malloc(256);
    tp->postpunctuation = (char*)malloc(256);
    
    tp->whitespace[0] = '\0';
    tp->prepunctuation[0] = '\0';
    tp->postpunctuation[0] = '\0';

    return tp;
}

char* text_processor_get_next_word(TextProcessor *tp) {
    if (!tp || !tp->file || feof(tp->file)) return NULL;

    int c;
    int ws_idx = 0;
    int pre_idx = 0;
    int word_idx = 0;
    int post_idx = 0;
    char word_buf[256] = {0};

    // 1. Read whitespace
    while ((c = fgetc(tp->file)) != EOF && isspace(c)) {
        if (ws_idx < 255) tp->whitespace[ws_idx++] = (char)c;
    }
    tp->whitespace[ws_idx] = '\0';

    if (c == EOF) return NULL;

    // 2. Read prepunctuation
    while (c != EOF && ispunct(c) && !isspace(c)) {
        if (pre_idx < 255) tp->prepunctuation[pre_idx++] = (char)c;
        c = fgetc(tp->file);
    }
    tp->prepunctuation[pre_idx] = '\0';

    // 3. Read word (alphanumeric)
    while (c != EOF && isalnum(c)) {
        if (word_idx < 255) word_buf[word_idx++] = (char)c;
        c = fgetc(tp->file);
    }
    word_buf[word_idx] = '\0';

    // 4. Read postpunctuation (if any before next whitespace/word)
    while (c != EOF && ispunct(c) && !isspace(c)) {
        if (post_idx < 255) tp->postpunctuation[post_idx++] = (char)c;
        c = fgetc(tp->file);
    }
    tp->postpunctuation[post_idx] = '\0';

    // Put back the character that ended the postpunctuation (likely whitespace)
    if (c != EOF) {
        ungetc(c, tp->file);
    }

    if (tp->current_word) free(tp->current_word);
    tp->current_word = strdup(word_buf);

    return tp->current_word;
}

void text_processor_close(TextProcessor *tp) {
    if (!tp) return;
    if (tp->file) fclose(tp->file);
    if (tp->current_word) free(tp->current_word);
    if (tp->whitespace) free(tp->whitespace);
    if (tp->prepunctuation) free(tp->prepunctuation);
    if (tp->postpunctuation) free(tp->postpunctuation);
    free(tp);
}

void handle_word_by_word_viewer(const char *filename) {
    TextProcessor *tp = text_processor_open(filename);
    if (!tp) {
        printf("\nError: Could not open file %s. Press any key...", filename);
        fflush(stdout);
        read_key();
        return;
    }

    printf("\033[H\033[J");
    printf("--- Word Viewer: %s ---\n", filename);
    printf("------------------------------------------\n");
    printf("Press SPACE to read next word, or Esc to exit.\n\n");

    char *word;
    while (1) {
        int key = read_key();
        if (key == KEY_ESC) break;
        if (key == ' ') {
            word = text_processor_get_next_word(tp);
            if (word) {
                printf("%s%s%s%s", tp->whitespace, tp->prepunctuation, word, tp->postpunctuation);
                fflush(stdout);
            } else {
                printf("\n\nEnd of file reached. Press any key to exit...");
                fflush(stdout);
                read_key();
                break;
            }
        }
    }

    text_processor_close(tp);
}

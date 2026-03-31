#include "text_processor.h"
#include "utils.h"
#include <ctype.h>
#include <sys/stat.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_BG_HIGHLIGHT  "\x1b[44m" // Blue background
#define ANSI_COLOR_RESET   "\x1b[0m"

static char* read_entire_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fclose(f);
        return NULL;
    }

    char *buffer = (char*)malloc(st.st_size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    fread(buffer, 1, st.st_size, f);
    buffer[st.st_size] = '\0';
    fclose(f);
    return buffer;
}

TextProcessor* text_processor_open(const char *filename) {
    char *full_text = read_entire_file(filename);
    if (!full_text) return NULL;

    TextProcessor *tp = (TextProcessor*)malloc(sizeof(TextProcessor));
    tp->full_text = full_text;
    tp->tokens = NULL;
    tp->num_tokens = 0;
    tp->current_token_idx = -1;

    // Tokenize
    int capacity = 100;
    tp->tokens = (Token*)malloc(sizeof(Token) * capacity);

    char *p = full_text;
    while (*p) {
        if (tp->num_tokens >= capacity) {
            capacity *= 2;
            tp->tokens = (Token*)realloc(tp->tokens, sizeof(Token) * capacity);
        }

        Token *t = &tp->tokens[tp->num_tokens];
        t->start_pos = (int)(p - full_text);

        // 1. Whitespace
        char *ws_start = p;
        while (*p && isspace((unsigned char)*p)) p++;
        int ws_len = (int)(p - ws_start);
        t->whitespace = (char*)malloc(ws_len + 1);
        strncpy(t->whitespace, ws_start, ws_len);
        t->whitespace[ws_len] = '\0';

        if (!*p) {
            // Only whitespace left
            if (ws_len > 0) {
                t->prepunctuation = strdup("");
                t->word = strdup("");
                t->postpunctuation = strdup("");
                t->end_pos = (int)(p - full_text);
                tp->num_tokens++;
            } else {
                // Should not happen
                free(t->whitespace);
            }
            break;
        }

        // 2. Prepunctuation
        char *pre_start = p;
        while (*p && ispunct((unsigned char)*p) && !isspace((unsigned char)*p)) {
            // Special check: if it's punctuation followed by alphanumeric, it might be prepunctuation.
            // But if it's alphanumeric after this punctuation, we stop if we hit something that's not punctuation.
            // Actually, Flite defines prepunctuation as punctuation before the word.
            // We'll peek ahead.
            char next = *(p + 1);
            if (isalnum((unsigned char)next) || isspace((unsigned char)next) || next == '\0') {
                p++;
                break; 
            }
            p++;
        }
        int pre_len = (int)(p - pre_start);
        t->prepunctuation = (char*)malloc(pre_len + 1);
        strncpy(t->prepunctuation, pre_start, pre_len);
        t->prepunctuation[pre_len] = '\0';

        // 3. Word
        char *word_start = p;
        while (*p && isalnum((unsigned char)*p)) p++;
        int word_len = (int)(p - word_start);
        t->word = (char*)malloc(word_len + 1);
        strncpy(t->word, word_start, word_len);
        t->word[word_len] = '\0';

        // 4. Postpunctuation
        char *post_start = p;
        while (*p && ispunct((unsigned char)*p) && !isspace((unsigned char)*p)) p++;
        int post_len = (int)(p - post_start);
        t->postpunctuation = (char*)malloc(post_len + 1);
        strncpy(t->postpunctuation, post_start, post_len);
        t->postpunctuation[post_len] = '\0';

        t->end_pos = (int)(p - full_text);
        tp->num_tokens++;
    }

    return tp;
}

Token* text_processor_get_next_token(TextProcessor *tp) {
    if (tp->current_token_idx + 1 < tp->num_tokens) {
        tp->current_token_idx++;
        return &tp->tokens[tp->current_token_idx];
    }
    return NULL;
}

Token* text_processor_get_prev_token(TextProcessor *tp) {
    if (tp->current_token_idx > 0) {
        tp->current_token_idx--;
        return &tp->tokens[tp->current_token_idx];
    }
    return NULL;
}

void text_processor_close(TextProcessor *tp) {
    if (!tp) return;
    for (int i = 0; i < tp->num_tokens; i++) {
        free(tp->tokens[i].whitespace);
        free(tp->tokens[i].prepunctuation);
        free(tp->tokens[i].word);
        free(tp->tokens[i].postpunctuation);
    }
    free(tp->tokens);
    free(tp->full_text);
    free(tp);
}

static void render_text_with_highlight(TextProcessor *tp) {
    printf("\033[H\033[J");
    printf("--- Word Viewer (Highlight Mode) ---\n");
    printf("------------------------------------------\n");
    printf("[SPACE: Next] [B: Back] [ESC: Exit]\n\n");

    for (int i = 0; i < tp->num_tokens; i++) {
        Token *t = &tp->tokens[i];
        printf("%s", t->whitespace);
        
        if (i == tp->current_token_idx) {
            printf(ANSI_BG_HIGHLIGHT "%s%s%s" ANSI_COLOR_RESET, 
                   t->prepunctuation, t->word, t->postpunctuation);
        } else {
            printf("%s%s%s", t->prepunctuation, t->word, t->postpunctuation);
        }
    }
    printf("\n");
    fflush(stdout);
}

void handle_word_by_word_viewer(const char *filename) {
    TextProcessor *tp = text_processor_open(filename);
    if (!tp) {
        printf("\nError: Could not open file %s. Press any key...", filename);
        fflush(stdout);
        read_key();
        return;
    }

    // Start with the first token
    tp->current_token_idx = 0;

    while (1) {
        render_text_with_highlight(tp);
        
        int key = read_key();
        if (key == KEY_ESC) break;
        if (key == ' ') {
            if (tp->current_token_idx + 1 < tp->num_tokens) {
                tp->current_token_idx++;
            } else {
                printf("\nEnd of file reached. Press any key to exit...");
                fflush(stdout);
                read_key();
                break;
            }
        } else if (key == 'b' || key == 'B') {
            if (tp->current_token_idx > 0) {
                tp->current_token_idx--;
            }
        }
    }

    text_processor_close(tp);
}

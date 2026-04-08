#include "entertainment.h"
#include "braille_ui.h"
#include "config.h"
#include "download_ui.h"
#include "document_reader.h"
#include "file_manager.h"
#include "keys_manager.h"
#include "menu_audio.h"
#include "menu.h"
#include "speech_engine.h"
#include "text_processor.h"
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>

typedef struct {
    size_t start_token;
    size_t end_token;
} ReaderPage;

typedef struct {
    int autoplay;
    int autoplay_mode;
    int waiting_for_line_continue;
    char status[160];
} WordReaderState;

typedef struct {
    const TextProcessor *processor;
    const char *selected_path;
    WordReaderState *reader_state;
    size_t chunk_start;
    size_t chunk_end;
    size_t *current_index;
} WordReaderPlaybackContext;

typedef struct {
    const TextProcessor *processor;
    const char *title;
    const char *footer_text;
    WordReaderState *reader_state;
    size_t chunk_start;
    size_t chunk_end;
    size_t *current_index;
    size_t *last_rendered_index;
} SpokenTextPlaybackContext;

enum {
    WORD_READER_AUTOPLAY_NONE = 0,
    WORD_READER_AUTOPLAY_CHUNK = 1,
    WORD_READER_AUTOPLAY_LINE = 2
};

static ReaderPage compute_reader_page(const TextProcessor *processor, size_t current_index, int width, int content_lines);
static void print_highlighted_word(const TextWord *word);
static char *build_reader_line_text(const TextProcessor *processor, size_t current_index);

static void content_ui_show_message_screen(const char *title, const char *message) {
    printf("\033[H\033[J--- %s ---\n\n%s\n\n%s",
           title ? title : "Message",
           message ? message : "",
           menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);
    read_key();
}

static void render_spoken_text_reader_with_footer(const TextProcessor *processor,
                                                  const char *title,
                                                  size_t current_index,
                                                  const WordReaderState *state,
                                                  const char *footer_text) {
    int rows;
    int cols;
    int content_lines;
    ReaderPage page;
    size_t i;
    const TextWord *current_word = text_processor_get_word(processor, current_index);

    get_terminal_size(&rows, &cols);
    content_lines = rows - 8;
    if (content_lines < 5) {
        content_lines = 5;
    }

    page = compute_reader_page(processor, current_index, cols, content_lines);

    printf("\033[H\033[J");
    print_memory_widget_line();
    printf("--- %s ---\n", title ? title : menu_translate("ui_text_reader", "Text Reader"));
    printf(menu_translate("ui_word_position_format", "Word %zu of %zu"), current_index + 1, processor->token_count);
    if (current_word) {
        printf("  [");
        printf(menu_translate("ui_line_column_format", "line %d, col %d"), current_word->line, current_word->column);
        printf("]");
    }
    printf("\n\n");

    for (i = page.start_token; i <= page.end_token && i < processor->token_count; i++) {
        if (i == current_index) {
            print_highlighted_word(&processor->tokens[i]);
        } else {
            printf("%s", processor->tokens[i].surface);
        }
    }

    printf("\n\n");
    {
        char *line_text = build_reader_line_text(processor, current_index);
        braille_ui_print_status_line(line_text ? line_text : (title ? title : ""));
        free(line_text);
    }
    printf("%s\n", footer_text ? footer_text : menu_translate("ui_footer_spoken_text", "[Space: Pause/Resume | Up: Previous | Down: Next | Enter: Repeat | Esc: Back]"));
    if (state && state->status[0]) {
        printf("%s\n", state->status);
    }
    fflush(stdout);
}

static void advance_reader_cursor(const char *text, int width, int *line, int *col) {
    const unsigned char *p = (const unsigned char *)text;

    while (*p) {
        if (*p == '\n') {
            (*line)++;
            *col = 1;
        } else if (*p == '\t') {
            int spaces = 4;
            while (spaces-- > 0) {
                if (*col > width) {
                    (*line)++;
                    *col = 1;
                }
                (*col)++;
            }
        } else {
            if (*col > width) {
                (*line)++;
                *col = 1;
            }
            (*col)++;
        }
        p++;
    }
}

static int token_starts_on_new_line(const TextWord *word) {
    const unsigned char *p = (const unsigned char *)word->whitespace;

    while (*p) {
        if (*p == '\n') {
            return 1;
        }
        p++;
    }

    return 0;
}

static ReaderPage compute_reader_page(const TextProcessor *processor, size_t current_index, int width, int content_lines) {
    ReaderPage page;
    size_t i;
    int line = 1;
    int col = 1;

    page.start_token = current_index;
    page.end_token = current_index;

    if (!processor || processor->token_count == 0) {
        return page;
    }

    if (current_index > 0) {
        for (i = current_index; i > 0; i--) {
            size_t candidate = i - 1;
            int test_line = 1;
            int test_col = 1;
            size_t j;

            for (j = candidate; j <= current_index; j++) {
                advance_reader_cursor(processor->tokens[j].surface, width, &test_line, &test_col);
            }

            if (test_line > (content_lines / 2) + 1) {
                break;
            }

            page.start_token = candidate;
        }
    }

    for (i = page.start_token; i < processor->token_count; i++) {
        if (i != page.start_token && token_starts_on_new_line(&processor->tokens[i]) && line >= content_lines) {
            break;
        }

        if (i != page.start_token && line > content_lines) {
            break;
        }

        page.end_token = i;
        advance_reader_cursor(processor->tokens[i].surface, width, &line, &col);
        if (line > content_lines) {
            break;
        }
    }

    if (page.end_token < current_index) {
        page.end_token = current_index;
    }

    return page;
}

static void print_highlighted_word(const TextWord *word) {
    printf("%s%s", word->whitespace, word->prepunctuation);
    printf("\033[30;47m%s\033[0m", word->word[0] ? word->word : " ");
    printf("%s", word->postpunctuation);
}

static void set_reader_status(WordReaderState *state, const char *message) {
    if (!state) {
        return;
    }

    if (!message) {
        state->status[0] = '\0';
        return;
    }

    snprintf(state->status, sizeof(state->status), "%s", message);
}

static void render_word_reader(const TextProcessor *processor, const char *selected_path, size_t current_index, const WordReaderState *state) {
    int rows;
    int cols;
    int content_lines;
    ReaderPage page;
    size_t i;
    const TextWord *current_word = text_processor_get_word(processor, current_index);

    get_terminal_size(&rows, &cols);
    content_lines = rows - 8;
    if (content_lines < 5) {
        content_lines = 5;
    }

    page = compute_reader_page(processor, current_index, cols, content_lines);

    printf("\033[H\033[J");
    print_memory_widget_line();
    printf("--- %s ---\n", menu_translate("color_reader", "Color Reader"));
    printf("%s: %s\n", menu_translate("ui_file_label", "File"), selected_path);
    printf(menu_translate("ui_word_position_format", "Word %zu of %zu"), current_index + 1, processor->token_count);
    if (current_word) {
        printf("  [");
        printf(menu_translate("ui_line_column_format", "line %d, col %d"), current_word->line, current_word->column);
        printf("]");
    }
    printf("\n\n");

    for (i = page.start_token; i <= page.end_token && i < processor->token_count; i++) {
        if (i == current_index) {
            print_highlighted_word(&processor->tokens[i]);
        } else {
            printf("%s", processor->tokens[i].surface);
        }
    }

    printf("\n\n");
    {
        char *line_text = build_reader_line_text(processor, current_index);
        braille_ui_print_status_line(line_text ? line_text : selected_path);
        free(line_text);
    }
    printf("%s\n", menu_translate("ui_footer_word_reader", "[Up: Previous | Down: Next | Enter: Next | Ctrl+E: Export to wave | Ctrl+P: Play | Ctrl+L: Line Play | Esc: Back]"));
    printf("%s\n", menu_translate("ui_reader_highlight_help", "The highlighted word represents the token currently being spoken."));
    printf("%s\n", menu_translate("ui_reader_export_help", "Use Ctrl+E to export the current document to a WAV file."));
    printf("%s\n", menu_translate("ui_reader_play_help", "Use Ctrl+P to play the document word by word with live highlighting."));
    if (state && state->status[0]) {
        printf("%s\n", state->status);
    } else if (state && state->autoplay) {
        if (state->autoplay_mode == WORD_READER_AUTOPLAY_LINE) {
            printf("%s\n", menu_translate("ui_reader_line_playing_status", "Line mode. Press a key to stop playback, then Space to continue to the next line."));
        } else {
            printf("%s\n", menu_translate("ui_reader_playing_status", "Playing. Press Ctrl+P to pause after the current word."));
        }
    }
    fflush(stdout);
}

static void render_spoken_text_reader(const TextProcessor *processor, const char *title, size_t current_index, const WordReaderState *state) {
    int rows;
    int cols;
    int content_lines;
    ReaderPage page;
    size_t i;
    const TextWord *current_word = text_processor_get_word(processor, current_index);

    get_terminal_size(&rows, &cols);
    content_lines = rows - 8;
    if (content_lines < 5) {
        content_lines = 5;
    }

    page = compute_reader_page(processor, current_index, cols, content_lines);

    printf("\033[H\033[J");
    print_memory_widget_line();
    printf("--- %s ---\n", title ? title : menu_translate("ui_text_reader", "Text Reader"));
    printf(menu_translate("ui_word_position_format", "Word %zu of %zu"), current_index + 1, processor->token_count);
    if (current_word) {
        printf("  [");
        printf(menu_translate("ui_line_column_format", "line %d, col %d"), current_word->line, current_word->column);
        printf("]");
    }
    printf("\n\n");

    for (i = page.start_token; i <= page.end_token && i < processor->token_count; i++) {
        if (i == current_index) {
            print_highlighted_word(&processor->tokens[i]);
        } else {
            printf("%s", processor->tokens[i].surface);
        }
    }

    printf("\n\n");
    {
        char *line_text = build_reader_line_text(processor, current_index);
        braille_ui_print_status_line(line_text ? line_text : (title ? title : ""));
        free(line_text);
    }
    printf("%s\n", menu_translate("ui_footer_spoken_text", "[Space: Pause/Resume | Up: Previous | Down: Next | Enter: Repeat | Esc: Back]"));
    if (state && state->status[0]) {
        printf("%s\n", state->status);
    }
    fflush(stdout);
}

static int token_is_sentence_break(const TextWord *word) {
    if (!word) {
        return 0;
    }

    return strchr(word->postpunctuation, '.') != NULL ||
           strchr(word->postpunctuation, '!') != NULL ||
           strchr(word->postpunctuation, '?') != NULL;
}

static size_t determine_reader_chunk_end(const TextProcessor *processor, size_t start_index) {
    const size_t min_chunk_words = 6;
    const size_t max_chunk_words = 24;
    size_t end_index;

    if (!processor || start_index >= processor->token_count) {
        return start_index;
    }

    end_index = start_index;
    while (end_index + 1 < processor->token_count &&
           (end_index - start_index + 1) < max_chunk_words) {
        if ((end_index - start_index + 1) >= min_chunk_words &&
            token_is_sentence_break(&processor->tokens[end_index])) {
            break;
        }
        end_index++;
    }

    return end_index;
}

static size_t determine_reader_line_end(const TextProcessor *processor, size_t start_index) {
    int line;
    size_t end_index;

    if (!processor || start_index >= processor->token_count) {
        return start_index;
    }

    line = processor->tokens[start_index].line;
    end_index = start_index;
    while (end_index + 1 < processor->token_count &&
           processor->tokens[end_index + 1].line == line) {
        end_index++;
    }

    return end_index;
}

static char *build_reader_chunk_text(const TextProcessor *processor, size_t start_index, size_t end_index) {
    size_t total = 0;
    size_t i;
    char *text;
    char *cursor;

    if (!processor || start_index >= processor->token_count || end_index < start_index) {
        return NULL;
    }

    total += strlen(processor->tokens[start_index].prepunctuation);
    total += strlen(processor->tokens[start_index].word);
    total += strlen(processor->tokens[start_index].postpunctuation);

    for (i = start_index + 1; i <= end_index && i < processor->token_count; i++) {
        total += strlen(processor->tokens[i].surface);
    }

    text = (char *)malloc(total + 1);
    if (!text) {
        return NULL;
    }

    cursor = text;
    strcpy(cursor, processor->tokens[start_index].prepunctuation);
    cursor += strlen(processor->tokens[start_index].prepunctuation);
    strcpy(cursor, processor->tokens[start_index].word);
    cursor += strlen(processor->tokens[start_index].word);
    strcpy(cursor, processor->tokens[start_index].postpunctuation);
    cursor += strlen(processor->tokens[start_index].postpunctuation);

    for (i = start_index + 1; i <= end_index && i < processor->token_count; i++) {
        strcpy(cursor, processor->tokens[i].surface);
        cursor += strlen(processor->tokens[i].surface);
    }

    *cursor = '\0';
    return text;
}

static char *build_reader_line_text(const TextProcessor *processor, size_t current_index) {
    int line;
    size_t start_index;
    size_t end_index;

    if (!processor || processor->token_count == 0 || current_index >= processor->token_count) {
        return NULL;
    }

    line = processor->tokens[current_index].line;
    start_index = current_index;
    while (start_index > 0 && processor->tokens[start_index - 1].line == line) {
        start_index--;
    }

    end_index = determine_reader_line_end(processor, current_index);
    return build_reader_chunk_text(processor, start_index, end_index);
}

static void reader_playback_progress(int token_index, void *userdata) {
    WordReaderPlaybackContext *context = (WordReaderPlaybackContext *)userdata;
    size_t absolute_index;

    if (!context || !context->current_index) {
        return;
    }

    absolute_index = context->chunk_start + (size_t)(token_index < 0 ? 0 : token_index);
    if (absolute_index > context->chunk_end) {
        absolute_index = context->chunk_end;
    }

    *context->current_index = absolute_index;
    render_word_reader(context->processor, context->selected_path, absolute_index, context->reader_state);
}

static int reader_playback_interrupt(void *userdata) {
    (void)userdata;
    return read_key_timeout(0);
}

static void spoken_text_progress(int token_index, void *userdata) {
    SpokenTextPlaybackContext *context = (SpokenTextPlaybackContext *)userdata;
    size_t absolute_index;

    if (!context || !context->current_index) {
        return;
    }

    absolute_index = context->chunk_start + (size_t)(token_index < 0 ? 0 : token_index);
    if (absolute_index > context->chunk_end) {
        absolute_index = context->chunk_end;
    }

    *context->current_index = absolute_index;
    if (!context->last_rendered_index || *context->last_rendered_index != absolute_index) {
        if (context->last_rendered_index) {
            *context->last_rendered_index = absolute_index;
        }
        render_spoken_text_reader(context->processor, context->title, absolute_index, context->reader_state);
    }
}

static int spoken_text_interrupt(void *userdata) {
    (void)userdata;
    return read_key_timeout(0);
}

static void spoken_text_progress_custom(int token_index, void *userdata) {
    SpokenTextPlaybackContext *context = (SpokenTextPlaybackContext *)userdata;
    size_t absolute_index;

    if (!context || !context->current_index) {
        return;
    }

    absolute_index = context->chunk_start + (size_t)(token_index < 0 ? 0 : token_index);
    if (absolute_index > context->chunk_end) {
        absolute_index = context->chunk_end;
    }

    *context->current_index = absolute_index;
    if (!context->last_rendered_index || *context->last_rendered_index != absolute_index) {
        if (context->last_rendered_index) {
            *context->last_rendered_index = absolute_index;
        }
        render_spoken_text_reader_with_footer(context->processor,
                                              context->title,
                                              absolute_index,
                                              context->reader_state,
                                              context->footer_text);
    }
}

static void drain_pending_input(void) {
    while (read_key_timeout(0) != 0) {
    }
}

static int read_nonzero_key(void) {
    int key;

    do {
        key = read_key();
    } while (key == 0);

    return key;
}

int content_ui_run_spoken_stage(const char *title, const char *text, const char *footer_text) {
    TextProcessor *processor;
    WordReaderState reader_state;
    size_t index = 0;
    size_t last_rendered_index = (size_t)-1;
    int paused_by_space = 0;
    int consume_space_key = 0;
    int needs_render = 1;

    if (!text || !text[0]) {
        return KEY_ENTER;
    }

    processor = text_processor_load_from_text(title ? title : "text", text);
    if (!processor || processor->token_count == 0) {
        text_processor_free(processor);
        return KEY_ENTER;
    }

    memset(&reader_state, 0, sizeof(reader_state));
    reader_state.autoplay = speech_engine_is_enabled();
    reader_state.autoplay_mode = WORD_READER_AUTOPLAY_LINE;
    set_reader_status(&reader_state,
                      reader_state.autoplay
                          ? menu_translate("ui_reader_line_play_starting", "Starting line-by-line playback...")
                          : menu_translate("ui_reader_speech_off", "Speech mode is off. Turn speech on to use Ctrl+P playback."));

    while (1) {
        char speech_error[128] = {0};
        int key = 0;
        size_t next_index = index;

        if (needs_render || last_rendered_index != index) {
            render_spoken_text_reader_with_footer(processor, title, index, &reader_state, footer_text);
            last_rendered_index = index;
            needs_render = 0;
        }

        if (reader_state.autoplay && speech_engine_is_enabled()) {
            size_t chunk_end = determine_reader_line_end(processor, index);
            char *chunk_text = build_reader_chunk_text(processor, index, chunk_end);
            SpokenTextPlaybackContext playback_context;
            int interrupted_key = 0;

            if (!chunk_text) {
                reader_state.autoplay = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_playback_failed", "Unable to play buffered speech for this word."));
                key = read_key();
            } else {
                playback_context.processor = processor;
                playback_context.title = title;
                playback_context.reader_state = &reader_state;
                playback_context.chunk_start = index;
                playback_context.chunk_end = chunk_end;
                playback_context.current_index = &index;
                playback_context.last_rendered_index = &last_rendered_index;
                playback_context.footer_text = footer_text;

                speech_engine_set_progress_callback(spoken_text_progress_custom, &playback_context);
                speech_engine_set_interrupt_callback(spoken_text_interrupt, &playback_context);
                if (!speech_engine_speak_text_buffered(chunk_text, speech_error, sizeof(speech_error))) {
                    speech_engine_set_progress_callback(NULL, NULL);
                    speech_engine_set_interrupt_callback(NULL, NULL);
                    interrupted_key = speech_engine_take_interrupt_key();
                    free(chunk_text);
                    reader_state.autoplay = 0;
                    if (interrupted_key != 0) {
                        if (interrupted_key == ' ') {
                            paused_by_space = 1;
                            consume_space_key = 1;
                            set_reader_status(&reader_state, menu_translate("ui_reader_play_paused", "Playback paused."));
                            drain_pending_input();
                            needs_render = 1;
                        }
                        key = interrupted_key;
                    } else {
                        set_reader_status(&reader_state, speech_error[0] ? speech_error : menu_translate("ui_reader_playback_failed", "Unable to play buffered speech for this word."));
                        needs_render = 1;
                        key = read_nonzero_key();
                    }
                } else {
                    speech_engine_set_progress_callback(NULL, NULL);
                    speech_engine_set_interrupt_callback(NULL, NULL);
                    free(chunk_text);
                    if (chunk_end + 1 < processor->token_count) {
                        index = chunk_end + 1;
                        paused_by_space = 0;
                        set_reader_status(&reader_state, NULL);
                        needs_render = 1;
                        continue;
                    }
                    reader_state.autoplay = 0;
                    paused_by_space = 0;
                    set_reader_status(&reader_state, menu_translate("ui_reader_play_complete", "Playback complete."));
                    needs_render = 1;
                    key = read_nonzero_key();
                }
            }
        } else {
            key = read_nonzero_key();
        }

        if (key == KEY_ESC) {
            text_processor_free(processor);
            return KEY_ESC;
        } else if (key == ' ') {
            if (consume_space_key) {
                consume_space_key = 0;
                drain_pending_input();
                needs_render = 1;
            } else if (paused_by_space) {
                reader_state.autoplay = 1;
                reader_state.autoplay_mode = WORD_READER_AUTOPLAY_LINE;
                paused_by_space = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_line_play_starting", "Starting line-by-line playback..."));
                drain_pending_input();
                needs_render = 1;
            } else if (reader_state.autoplay) {
                reader_state.autoplay = 0;
                paused_by_space = 1;
                set_reader_status(&reader_state, menu_translate("ui_reader_play_paused", "Playback paused."));
                drain_pending_input();
                needs_render = 1;
            }
        } else if (key == KEY_DOWN) {
            paused_by_space = 0;
            consume_space_key = 0;
            next_index = (size_t)menu_next_index((int)index, 1, (int)processor->token_count);
            set_reader_status(&reader_state, NULL);
            needs_render = 1;
        } else if (key == KEY_UP) {
            paused_by_space = 0;
            consume_space_key = 0;
            next_index = (size_t)menu_next_index((int)index, -1, (int)processor->token_count);
            set_reader_status(&reader_state, NULL);
            needs_render = 1;
        } else if (key == KEY_ENTER) {
            text_processor_free(processor);
            return KEY_ENTER;
        }

        if (next_index != index) {
            index = next_index;
            needs_render = 1;
        }
    }
}

static void build_wave_export_path(const char *source_path, char *buffer, size_t buffer_size) {
    const char *last_slash;
    const char *last_dot;
    char *voice_name;
    char voice_label[128];
    size_t dir_len;
    size_t name_len;

    if (!source_path || !buffer || buffer_size == 0) {
        return;
    }

    last_slash = strrchr(source_path, '/');
    last_dot = strrchr(source_path, '.');

    if (!last_slash) {
        last_slash = source_path - 1;
    }

    dir_len = (size_t)(last_slash - source_path + 1);
    if (!last_dot || last_dot < last_slash) {
        last_dot = source_path + strlen(source_path);
    }
    name_len = (size_t)(last_dot - (last_slash + 1));

    voice_name = get_setting("tts_voice");
    if (!voice_name || !voice_name[0]) {
        free(voice_name);
        voice_name = strdup("kal");
    }

    {
        const char *voice_base = strrchr(voice_name, '/');
        const char *voice_end;
        size_t voice_len;

        if (voice_base) {
            voice_base++;
        } else {
            voice_base = voice_name;
        }

        voice_end = strrchr(voice_base, '.');
        if (!voice_end || voice_end <= voice_base) {
            voice_end = voice_base + strlen(voice_base);
        }

        voice_len = (size_t)(voice_end - voice_base);
        if (voice_len >= sizeof(voice_label)) {
            voice_len = sizeof(voice_label) - 1;
        }

        memcpy(voice_label, voice_base, voice_len);
        voice_label[voice_len] = '\0';

        for (size_t i = 0; voice_label[i]; i++) {
            if (!isalnum((unsigned char)voice_label[i]) && voice_label[i] != '_' && voice_label[i] != '-') {
                voice_label[i] = '_';
            }
        }
    }

    snprintf(buffer, buffer_size, "%.*s%.*s_%s_export.wav",
             (int)dir_len, source_path,
             (int)name_len, last_slash + 1,
             voice_label);

    free(voice_name);
}

static void export_processor_to_wave(const TextProcessor *processor, const char *selected_path) {
    char export_path[PATH_MAX];
    char error[128] = {0};

    if (!processor || processor->token_count == 0) {
        return;
    }

    build_wave_export_path(selected_path, export_path, sizeof(export_path));

    printf("\033[H\033[J--- Color Reader ---\n");
    printf("Exporting audio to:\n%s\n\n", export_path);
    printf("Processing the full document through Flite streaming...\n");
    fflush(stdout);

    if (speech_engine_export_text_to_wave(processor->content, export_path, error, sizeof(error))) {
        printf("Export complete.\n");
    } else {
        printf("Export failed: %s\n", error[0] ? error : "Unknown speech export error");
    }
    printf("\nPress any key to continue...");
    fflush(stdout);
    read_key();
}

void content_ui_show_spoken_text(const char *title, const char *source_name, const char *text) {
    TextProcessor *processor;
    WordReaderState reader_state;
    size_t index = 0;
    size_t last_rendered_index = (size_t)-1;
    int paused_by_space = 0;
    int consume_space_key = 0;
    int needs_render = 1;

    if (!text || !text[0]) {
        printf("\033[H\033[J");
        print_memory_widget_line();
        printf("--- %s ---\n", title ? title : menu_translate("ui_text_reader", "Text Reader"));
        printf("%s\n", menu_translate("ui_no_content_available", "No content available."));
        printf("\n");
        braille_ui_print_status_line(menu_translate("ui_no_content_available", "No content available."));
        printf("%s\n", menu_translate("ui_footer_spoken_text", "[Space: Pause/Resume | Up: Previous | Down: Next | Enter: Repeat | Esc: Back]"));
        fflush(stdout);
        while (read_key() != KEY_ESC) {
        }
        return;
    }

    processor = text_processor_load_from_text(source_name ? source_name : (title ? title : "text"), text);
    if (!processor || processor->token_count == 0) {
        text_processor_free(processor);
        printf("\033[H\033[J");
        print_memory_widget_line();
        printf("--- %s ---\n", title ? title : menu_translate("ui_text_reader", "Text Reader"));
        printf("%s\n", menu_translate("ui_no_content_available", "No content available."));
        braille_ui_print_status_line(menu_translate("ui_no_content_available", "No content available."));
        fflush(stdout);
        while (read_key() != KEY_ESC) {
        }
        return;
    }

    memset(&reader_state, 0, sizeof(reader_state));
    reader_state.autoplay = speech_engine_is_enabled();
    reader_state.autoplay_mode = WORD_READER_AUTOPLAY_LINE;
    set_reader_status(&reader_state,
                      reader_state.autoplay
                          ? menu_translate("ui_reader_line_play_starting", "Starting line-by-line playback...")
                          : menu_translate("ui_reader_speech_off", "Speech mode is off. Turn speech on to use Ctrl+P playback."));

    while (1) {
        char speech_error[128] = {0};
        int key = 0;
        size_t next_index = index;

        if (needs_render || last_rendered_index != index) {
            render_spoken_text_reader(processor, title, index, &reader_state);
            last_rendered_index = index;
            needs_render = 0;
        }

        if (reader_state.autoplay && speech_engine_is_enabled()) {
            size_t chunk_end = determine_reader_line_end(processor, index);
            char *chunk_text = build_reader_chunk_text(processor, index, chunk_end);
            SpokenTextPlaybackContext playback_context;
            int interrupted_key = 0;

            if (!chunk_text) {
                reader_state.autoplay = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_playback_failed", "Unable to play buffered speech for this word."));
                key = read_key();
            } else {
                playback_context.processor = processor;
                playback_context.title = title;
                playback_context.reader_state = &reader_state;
                playback_context.chunk_start = index;
                playback_context.chunk_end = chunk_end;
                playback_context.current_index = &index;
                playback_context.last_rendered_index = &last_rendered_index;

                speech_engine_set_progress_callback(spoken_text_progress, &playback_context);
                speech_engine_set_interrupt_callback(spoken_text_interrupt, &playback_context);
                if (!speech_engine_speak_text_buffered(chunk_text, speech_error, sizeof(speech_error))) {
                    speech_engine_set_progress_callback(NULL, NULL);
                    speech_engine_set_interrupt_callback(NULL, NULL);
                    interrupted_key = speech_engine_take_interrupt_key();
                    free(chunk_text);
                    reader_state.autoplay = 0;
                    if (interrupted_key != 0) {
                        if (interrupted_key == ' ') {
                            paused_by_space = 1;
                            consume_space_key = 1;
                            set_reader_status(&reader_state, menu_translate("ui_reader_play_paused", "Playback paused."));
                            drain_pending_input();
                            needs_render = 1;
                        }
                        key = interrupted_key;
                    } else {
                        set_reader_status(&reader_state, speech_error[0] ? speech_error : menu_translate("ui_reader_playback_failed", "Unable to play buffered speech for this word."));
                        needs_render = 1;
                        key = read_nonzero_key();
                    }
                } else {
                    speech_engine_set_progress_callback(NULL, NULL);
                    speech_engine_set_interrupt_callback(NULL, NULL);
                    free(chunk_text);
                    if (chunk_end + 1 < processor->token_count) {
                        index = chunk_end + 1;
                        paused_by_space = 0;
                        set_reader_status(&reader_state, NULL);
                        needs_render = 1;
                        continue;
                    }
                    reader_state.autoplay = 0;
                    paused_by_space = 0;
                    set_reader_status(&reader_state, menu_translate("ui_reader_play_complete", "Playback complete."));
                    needs_render = 1;
                    key = read_nonzero_key();
                }
            }
        } else {
            key = read_nonzero_key();
        }

        if (key == KEY_ESC) {
            break;
        } else if (key == ' ') {
            if (consume_space_key) {
                consume_space_key = 0;
                drain_pending_input();
                needs_render = 1;
            } else if (paused_by_space) {
                reader_state.autoplay = 1;
                reader_state.autoplay_mode = WORD_READER_AUTOPLAY_LINE;
                paused_by_space = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_line_play_starting", "Starting line-by-line playback..."));
                drain_pending_input();
                needs_render = 1;
            } else if (reader_state.autoplay) {
                reader_state.autoplay = 0;
                paused_by_space = 1;
                set_reader_status(&reader_state, menu_translate("ui_reader_play_paused", "Playback paused."));
                drain_pending_input();
                needs_render = 1;
            }
        } else if (key == KEY_DOWN) {
            paused_by_space = 0;
            consume_space_key = 0;
            next_index = (size_t)menu_next_index((int)index, 1, (int)processor->token_count);
            set_reader_status(&reader_state, NULL);
            needs_render = 1;
        } else if (key == KEY_UP) {
            paused_by_space = 0;
            consume_space_key = 0;
            next_index = (size_t)menu_next_index((int)index, -1, (int)processor->token_count);
            set_reader_status(&reader_state, NULL);
            needs_render = 1;
        } else if (key == KEY_ENTER) {
            reader_state.autoplay = 1;
            reader_state.autoplay_mode = WORD_READER_AUTOPLAY_LINE;
            paused_by_space = 0;
            consume_space_key = 0;
            set_reader_status(&reader_state, menu_translate("ui_reader_line_play_starting", "Starting line-by-line playback..."));
            needs_render = 1;
        }

        if (next_index != index) {
            index = next_index;
            needs_render = 1;
        }
    }

    text_processor_free(processor);
}

void content_ui_show_joke(void) {
    int fetch_new = 1;
    char joke_text[2048] = {0};

    while (1) {
        if (fetch_new) {
            const char *url = "https://v2.jokeapi.dev/joke/Any?format=json&type=single&lang=en&amount=1";
            char error[256] = {0};
            char *response = fetch_text_with_progress_ui("Joke", url, "joke data", error, sizeof(error));
            if (response) {
                cJSON *json = cJSON_Parse(response);
                if (json) {
                    cJSON *joke_node = cJSON_GetObjectItemCaseSensitive(json, "joke");
                    if (joke_node && cJSON_IsString(joke_node)) {
                        strncpy(joke_text, joke_node->valuestring, sizeof(joke_text) - 1);
                    } else {
                        strcpy(joke_text, "Failed to parse joke.");
                    }
                    cJSON_Delete(json);
                } else {
                    strcpy(joke_text, "Error parsing API response.");
                }
                free(response);
            } else {
                snprintf(joke_text, sizeof(joke_text), "%s", error[0] ? error : "Failed to connect to Joke API.");
            }
            fetch_new = 0;
        }
        content_ui_show_spoken_text("Joke", "Joke", joke_text);
        break;
    }
}

static char *knowledge_fetch_response_from_url(const char *title,
                                               const char *url_without_key,
                                               char *error,
                                               size_t error_size);

static char *knowledge_fetch_response(const char *title,
                                      const char *endpoint,
                                      char *error,
                                      size_t error_size) {
    char url[1024];
    snprintf(url, sizeof(url), "%s", endpoint);
    return knowledge_fetch_response_from_url(title, url, error, error_size);
}

static char *knowledge_fetch_response_from_url(const char *title,
                                               const char *url_without_key,
                                               char *error,
                                               size_t error_size) {
    char *api_key;
    char *encoded_key;
    char url[1024];
    char *response;

    api_key = keys_manager_get_api_league_key();
    if (!api_key || api_key[0] == '\0') {
        if (error && error_size > 0) {
            snprintf(error, error_size,
                     "API League key is not set. Open Settings > Keys Manager and save the key first.");
        }
        free(api_key);
        return NULL;
    }

    encoded_key = url_encode(api_key);
    free(api_key);
    if (!encoded_key) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to encode API League key.");
        }
        return NULL;
    }

    snprintf(url, sizeof(url), "%s%sapi-key=%s",
             url_without_key,
             strchr(url_without_key, '?') ? "&" : "?",
             encoded_key);
    free(encoded_key);

    response = fetch_text_with_progress_ui(title, url, "knowledge data", error, error_size);
    return response;
}

static void content_ui_show_random_knowledge_item(const char *title,
                                                  const char *source_name,
                                                  const char *endpoint,
                                                  void (*formatter)(cJSON *json, char *buffer, size_t buffer_size)) {
    char error[256] = {0};
    char text[2048];
    char *response;
    cJSON *json;

    response = knowledge_fetch_response(title, endpoint, error, sizeof(error));
    if (!response) {
        snprintf(text, sizeof(text), "%s", error[0] ? error : "Unable to fetch knowledge item.");
        content_ui_show_spoken_text(title, source_name, text);
        return;
    }

    json = cJSON_Parse(response);
    free(response);

    if (!json) {
        content_ui_show_spoken_text(title, source_name, "Error parsing API response.");
        return;
    }

    text[0] = '\0';
    formatter(json, text, sizeof(text));
    if (text[0] == '\0') {
        snprintf(text, sizeof(text), "The API response did not include usable content.");
    }

    cJSON_Delete(json);
    content_ui_show_spoken_text(title, source_name, text);
}

static void content_ui_show_knowledge_item_from_url(const char *title,
                                                    const char *source_name,
                                                    const char *url,
                                                    void (*formatter)(cJSON *json, char *buffer, size_t buffer_size)) {
    char error[256] = {0};
    char text[4096];
    char *response;
    cJSON *json;

    response = knowledge_fetch_response_from_url(title, url, error, sizeof(error));
    if (!response) {
        snprintf(text, sizeof(text), "%s", error[0] ? error : "Unable to fetch knowledge item.");
        content_ui_show_spoken_text(title, source_name, text);
        return;
    }

    json = cJSON_Parse(response);
    free(response);

    if (!json) {
        content_ui_show_spoken_text(title, source_name, "Error parsing API response.");
        return;
    }

    text[0] = '\0';
    formatter(json, text, sizeof(text));
    if (text[0] == '\0') {
        snprintf(text, sizeof(text), "The API response did not include usable content.");
    }

    cJSON_Delete(json);
    content_ui_show_spoken_text(title, source_name, text);
}

static void format_life_hack(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *title = cJSON_GetObjectItemCaseSensitive(json, "title");
    cJSON *description = cJSON_GetObjectItemCaseSensitive(json, "description");

    snprintf(buffer, buffer_size,
             "%s%s%s%s",
             cJSON_IsString(title) ? title->valuestring : "Life Hack",
             cJSON_IsString(description) ? "\n\n" : "",
             cJSON_IsString(description) ? description->valuestring : "",
             "");
}

static void format_affirmation(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *affirmation = cJSON_GetObjectItemCaseSensitive(json, "affirmation");

    if (cJSON_IsString(affirmation)) {
        snprintf(buffer, buffer_size, "%s", affirmation->valuestring);
    }
}

static void format_trivia(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *trivia = cJSON_GetObjectItemCaseSensitive(json, "trivia");
    cJSON *category = cJSON_GetObjectItemCaseSensitive(json, "category");

    if (cJSON_IsString(category) && cJSON_IsString(trivia)) {
        snprintf(buffer, buffer_size, "Category: %s\n\n%s", category->valuestring, trivia->valuestring);
    } else if (cJSON_IsString(trivia)) {
        snprintf(buffer, buffer_size, "%s", trivia->valuestring);
    }
}

static void format_riddle(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *riddle = cJSON_GetObjectItemCaseSensitive(json, "riddle");
    cJSON *answer = cJSON_GetObjectItemCaseSensitive(json, "answer");
    cJSON *difficulty = cJSON_GetObjectItemCaseSensitive(json, "difficulty");

    snprintf(buffer, buffer_size,
             "%s%s%s%s%s%s",
             cJSON_IsString(riddle) ? riddle->valuestring : "",
             cJSON_IsString(difficulty) ? "\n\nDifficulty: " : "",
             cJSON_IsString(difficulty) ? difficulty->valuestring : "",
             cJSON_IsString(answer) ? "\n\nAnswer: " : "",
             cJSON_IsString(answer) ? answer->valuestring : "",
             "");
}

static void format_quote(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *quote = cJSON_GetObjectItemCaseSensitive(json, "quote");
    cJSON *author = cJSON_GetObjectItemCaseSensitive(json, "author");

    snprintf(buffer, buffer_size,
             "%s%s%s",
             cJSON_IsString(quote) ? quote->valuestring : "",
             cJSON_IsString(author) ? "\n\nAuthor: " : "",
             cJSON_IsString(author) ? author->valuestring : "");
}

static void format_poem(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *title = cJSON_GetObjectItemCaseSensitive(json, "title");
    cJSON *author = cJSON_GetObjectItemCaseSensitive(json, "author");
    cJSON *poem = cJSON_GetObjectItemCaseSensitive(json, "poem");

    snprintf(buffer, buffer_size,
             "%s%s%s%s%s",
             cJSON_IsString(title) ? title->valuestring : "Poem",
             cJSON_IsString(author) ? "\nBy: " : "",
             cJSON_IsString(author) ? author->valuestring : "",
             cJSON_IsString(poem) ? "\n\n" : "",
             cJSON_IsString(poem) ? poem->valuestring : "");
}

static void format_joke_api(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *joke = cJSON_GetObjectItemCaseSensitive(json, "joke");

    if (cJSON_IsString(joke)) {
        snprintf(buffer, buffer_size, "%s", joke->valuestring);
    }
}

static void format_synonyms(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *synonyms = cJSON_GetObjectItemCaseSensitive(json, "synonyms");
    int i;

    if (!cJSON_IsArray(synonyms)) {
        return;
    }

    for (i = 0; i < cJSON_GetArraySize(synonyms) && strlen(buffer) < buffer_size - 32; i++) {
        cJSON *item = cJSON_GetArrayItem(synonyms, i);
        if (cJSON_IsString(item) && item->valuestring) {
            if (buffer[0] != '\0') {
                strncat(buffer, "\n", buffer_size - strlen(buffer) - 1);
            }
            strncat(buffer, item->valuestring, buffer_size - strlen(buffer) - 1);
        }
    }
}

static void format_singularize(cJSON *json, char *buffer, size_t buffer_size) {
    cJSON *singular = cJSON_GetObjectItemCaseSensitive(json, "singular");
    cJSON *plural = cJSON_GetObjectItemCaseSensitive(json, "plural");

    if (cJSON_IsString(singular) && cJSON_IsString(plural)) {
        snprintf(buffer, buffer_size, "Plural: %s\nSingular: %s", plural->valuestring, singular->valuestring);
    } else if (cJSON_IsString(singular)) {
        snprintf(buffer, buffer_size, "Singular: %s", singular->valuestring);
    }
}

void content_ui_show_random_life_hack(void) {
    content_ui_show_random_knowledge_item("Random Life Hack",
                                          "Random Life Hack",
                                          "https://api.apileague.com/retrieve-random-life-hack",
                                          format_life_hack);
}

void content_ui_show_random_affirmation(void) {
    content_ui_show_random_knowledge_item("Random Affirmation",
                                          "Random Affirmation",
                                          "https://api.apileague.com/retrieve-random-affirmation",
                                          format_affirmation);
}

void content_ui_show_random_trivia(void) {
    content_ui_show_random_knowledge_item("Random Trivia",
                                          "Random Trivia",
                                          "https://api.apileague.com/retrieve-random-trivia",
                                          format_trivia);
}

void content_ui_show_random_riddle(void) {
    content_ui_show_random_knowledge_item("Random Riddle",
                                          "Random Riddle",
                                          "https://api.apileague.com/retrieve-random-riddle",
                                          format_riddle);
}

void content_ui_show_random_quote(void) {
    content_ui_show_random_knowledge_item("Random Quote",
                                          "Random Quote",
                                          "https://api.apileague.com/retrieve-random-quote",
                                          format_quote);
}

void content_ui_show_synonyms(void) {
    char word[128];
    char *encoded_word;
    char url[1024];

    get_user_input(word, sizeof(word), "Enter word");
    if (word[0] == '\0') {
        return;
    }

    encoded_word = url_encode(word);
    if (!encoded_word) {
        content_ui_show_spoken_text("Synonyms", "Synonyms", "Unable to encode the word.");
        return;
    }

    snprintf(url, sizeof(url), "https://api.apileague.com/synonyms?word=%s", encoded_word);
    free(encoded_word);

    content_ui_show_knowledge_item_from_url("Synonyms", "Synonyms", url, format_synonyms);
}

void content_ui_show_singularize(void) {
    char word[128];
    char *encoded_word;
    char url[1024];

    get_user_input(word, sizeof(word), "Enter plural word");
    if (word[0] == '\0') {
        return;
    }

    encoded_word = url_encode(word);
    if (!encoded_word) {
        content_ui_show_spoken_text("Singularize", "Singularize", "Unable to encode the word.");
        return;
    }

    snprintf(url, sizeof(url), "https://api.apileague.com/singularize?word=%s", encoded_word);
    free(encoded_word);

    content_ui_show_knowledge_item_from_url("Singularize", "Singularize", url, format_singularize);
}

void content_ui_show_random_poem_api(void) {
    content_ui_show_random_knowledge_item("Random Poem",
                                          "Random Poem",
                                          "https://api.apileague.com/retrieve-random-poem",
                                          format_poem);
}

void content_ui_show_random_joke_api(void) {
    content_ui_show_knowledge_item_from_url("Random Joke",
                                            "Random Joke",
                                            "https://api.apileague.com/retrieve-random-joke?exclude-tags=nsfw,sexual,racist,sexist,religious",
                                            format_joke_api);
}

void content_ui_show_local_riddle(void) {
    const char *path = "Downloads/riddles.json";
    FILE *file = fopen(path, "rb");
    cJSON *json;
    char *data;
    long length;
    int count;
    int index;
    cJSON *item;
    cJSON *riddle;
    cJSON *answer;
    const char *riddle_text;
    const char *answer_text;
    static int random_seeded = 0;

    if (!file) {
        content_ui_show_message_screen("Riddles", "Unable to open Downloads/riddles.json.");
        return;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    data = (char *)malloc((size_t)length + 1);
    if (!data) {
        fclose(file);
        content_ui_show_message_screen("Riddles", "Unable to allocate memory for riddles data.");
        return;
    }

    fread(data, 1, (size_t)length, file);
    data[length] = '\0';
    fclose(file);

    json = cJSON_Parse(data);
    free(data);

    if (!json || !cJSON_IsArray(json)) {
        if (json) {
            cJSON_Delete(json);
        }
        content_ui_show_message_screen("Riddles", "Error parsing Downloads/riddles.json.");
        return;
    }

    count = cJSON_GetArraySize(json);
    if (count <= 0) {
        cJSON_Delete(json);
        content_ui_show_message_screen("Riddles", "No riddles were found in Downloads/riddles.json.");
        return;
    }

    if (!random_seeded) {
        srand((unsigned int)time(NULL));
        random_seeded = 1;
    }

    while (1) {
        index = rand() % count;
        item = cJSON_GetArrayItem(json, index);
        riddle = cJSON_GetObjectItemCaseSensitive(item, "riddle");
        answer = cJSON_GetObjectItemCaseSensitive(item, "answer");
        riddle_text = (riddle && cJSON_IsString(riddle) && riddle->valuestring)
            ? riddle->valuestring
            : "No riddle text available.";
        answer_text = (answer && cJSON_IsString(answer) && answer->valuestring)
            ? answer->valuestring
            : menu_translate("ui_not_available", "Not available");

        if (content_ui_run_spoken_stage("Riddles",
                                        riddle_text,
                                        menu_translate("ui_footer_riddles_question", "[Enter: Show Answer | Esc: Back]")) == KEY_ESC) {
            cJSON_Delete(json);
            return;
        }

        if (content_ui_run_spoken_stage("Riddle Answer",
                                        answer_text,
                                        menu_translate("ui_footer_riddles_answer", "[Enter: Next Riddle | Esc: Back]")) == KEY_ESC) {
            cJSON_Delete(json);
            return;
        }
    }
}

void content_ui_show_short_stories(void) {
    const char *story_file = "Downloads/ShortStories.json";
    const char *url = "https://shortstories-api.onrender.com/stories";
    char *data = NULL;
    char error[256] = {0};
    FILE *f = fopen(story_file, "rb");

    if (f) {
        printf("\033[H\033[J--- Short Stories ---\nLoading stories from cache...\n");
        fflush(stdout);
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = (char *)malloc(len + 1);
        fread(data, 1, len, f);
        data[len] = '\0';
        fclose(f);
    } else {
        data = fetch_text_with_progress_ui("Short Stories", url, "short stories data", error, sizeof(error));
        if (!data || !data[0]) {
            printf("\033[H\033[J--- Short Stories ---\n%s\n\nPress any key...", error[0] ? error : "Failed to fetch stories.");
            fflush(stdout);
            read_key();
            free(data);
            return;
        }

        f = fopen(story_file, "wb");
        if (f) {
            fwrite(data, 1, strlen(data), f);
            fclose(f);
        }
    }

    cJSON *json = cJSON_Parse(data);
    free(data);

    if (!json || !cJSON_IsArray(json)) {
        printf("\nError parsing stories data.\nPress any key...");
        fflush(stdout); read_key();
        if (json) cJSON_Delete(json);
        return;
    }

    int story_count = cJSON_GetArraySize(json);
    int sel = 0;
    int STORY_PAGE_SIZE = 10;
    int last_spoken_sel = -1;

    while (1) {
        int start_idx = (sel / STORY_PAGE_SIZE) * STORY_PAGE_SIZE;
        int end_idx = start_idx + STORY_PAGE_SIZE;
        if (end_idx > story_count) end_idx = story_count;
        int total_pages = (story_count + STORY_PAGE_SIZE - 1) / STORY_PAGE_SIZE;
        int current_page = (start_idx / STORY_PAGE_SIZE) + 1;

        printf("\033[H\033[J--- Short Stories (Page %d of %d) ---\n", current_page, total_pages);
        for (int i = start_idx; i < end_idx; i++) {
            cJSON *story_obj = cJSON_GetArrayItem(json, i);
            cJSON *title_node = cJSON_GetObjectItemCaseSensitive(story_obj, "title");
            if (i == sel) printf("> %s\n", title_node ? title_node->valuestring : "Unknown Title");
            else printf("  %s\n", title_node ? title_node->valuestring : "Unknown Title");
        }
        if (end_idx < story_count) printf("  ... (Next page below) ...\n");
        if (start_idx > 0) printf("  ... (Previous page above) ...\n");
        
        printf("\n%s\n", menu_translate("ui_footer_read_back", "[Arrows: Navigate | Enter: Read | Esc: Back]"));
        fflush(stdout);

        if (sel != last_spoken_sel) {
            cJSON *story_obj = cJSON_GetArrayItem(json, sel);
            cJSON *title_node = cJSON_GetObjectItemCaseSensitive(story_obj, "title");
            if (title_node && cJSON_IsString(title_node)) {
                menu_audio_speak(title_node->valuestring);
            }
            last_spoken_sel = sel;
        }

        int key = read_key();
        if (key == KEY_ESC) break;
        else if (key == KEY_UP && sel > 0) {
            menu_audio_stop();
            sel--;
        }
        else if (key == KEY_DOWN && sel < story_count - 1) {
            menu_audio_stop();
            sel++;
        }
        else if (key == KEY_ENTER) {
            char story_text[8192];
            cJSON *story_obj = cJSON_GetArrayItem(json, sel);
            cJSON *title_node = cJSON_GetObjectItemCaseSensitive(story_obj, "title");
            cJSON *content_node = cJSON_GetObjectItemCaseSensitive(story_obj, "story");
            cJSON *moral_node = cJSON_GetObjectItemCaseSensitive(story_obj, "moral");
            menu_audio_stop();

            snprintf(story_text, sizeof(story_text), "%s%s%s%s%s",
                     content_node && cJSON_IsString(content_node) ? content_node->valuestring : "",
                     moral_node && cJSON_IsString(moral_node) ? "\n\nMoral: " : "",
                     moral_node && cJSON_IsString(moral_node) ? moral_node->valuestring : "",
                     "",
                     "");
            content_ui_show_spoken_text(title_node && cJSON_IsString(title_node) ? title_node->valuestring : "Story",
                                        title_node && cJSON_IsString(title_node) ? title_node->valuestring : "Story",
                                        story_text);
            last_spoken_sel = -1;
        }
    }

    cJSON_Delete(json);
}

void content_ui_show_poems(void) {
    const char *url = "https://poetrydb.org/random/1";
    char error[256] = {0};
    char *response = fetch_text_with_progress_ui("Poems", url, "poem data", error, sizeof(error));
    if (response) {
        char poem_text[8192];
        cJSON *json = cJSON_Parse(response);
        free(response);
        if (json && cJSON_IsArray(json)) {
            cJSON *item = cJSON_GetArrayItem(json, 0);
            cJSON *title = cJSON_GetObjectItemCaseSensitive(item, "title");
            cJSON *author = cJSON_GetObjectItemCaseSensitive(item, "author");
            cJSON *lines = cJSON_GetObjectItemCaseSensitive(item, "lines");

            if (cJSON_IsArray(lines)) {
                size_t used = 0;
                int total_lines = cJSON_GetArraySize(lines);

                poem_text[0] = '\0';
                if (author && cJSON_IsString(author)) {
                    used += (size_t)snprintf(poem_text + used, sizeof(poem_text) - used, "By %s\n\n", author->valuestring);
                }
                for (int i = 0; i < total_lines && used + 2 < sizeof(poem_text); i++) {
                    cJSON *line = cJSON_GetArrayItem(lines, i);
                    if (line && cJSON_IsString(line)) {
                        used += (size_t)snprintf(poem_text + used, sizeof(poem_text) - used, "%s\n", line->valuestring);
                    }
                }
                content_ui_show_spoken_text(title && cJSON_IsString(title) ? title->valuestring : "Poem",
                                            title && cJSON_IsString(title) ? title->valuestring : "Poem",
                                            poem_text);
            }
            cJSON_Delete(json);
        } else {
            printf("\nError parsing poem data. Press any key...");
            fflush(stdout); read_key();
        }
    } else {
        printf("\n%s\nPress any key...", error[0] ? error : "Failed to connect to Poem API.");
        fflush(stdout); read_key();
    }
}

void content_ui_run_word_viewer(void) {
    char *selected_path = file_navigator_supported(USER_SPACE);
    struct stat st;
    char error[128] = {0};
    char *document_text = NULL;
    WordReaderState reader_state;

    if (!selected_path) {
        return;
    }

    if (stat(selected_path, &st) != 0 || S_ISDIR(st.st_mode)) {
        printf("\033[H\033[J--- Color Reader ---\n");
        printf("Please select a text file.\n");
        printf("\nPress any key to continue...");
        fflush(stdout);
        read_key();
        free(selected_path);
        return;
    }

    document_text = document_load_text_with_progress(selected_path, error, sizeof(error));
    if (!document_text) {
        printf("\033[H\033[J--- Color Reader ---\n");
        printf("Failed to read file:\n%s\n", selected_path);
        printf("%s\n", error[0] ? error : "Unsupported or unreadable document.");
        printf("\nPress any key to continue...");
        fflush(stdout);
        read_key();
        free(selected_path);
        return;
    }

    TextProcessor *processor = text_processor_load_from_text(selected_path, document_text);
    free(document_text);
    if (!processor) {
        printf("\033[H\033[J--- Color Reader ---\n");
        printf("Failed to parse file:\n%s\n", selected_path);
        printf("\nPress any key to continue...");
        fflush(stdout);
        read_key();
        free(selected_path);
        return;
    }

    if (processor->token_count == 0) {
        printf("\033[H\033[J--- Color Reader ---\n");
        printf("No readable words found in:\n%s\n", selected_path);
        printf("\nPress any key to continue...");
        fflush(stdout);
        read_key();
        text_processor_free(processor);
        free(selected_path);
        return;
    }

    size_t index = 0;
    size_t last_spoken_index = (size_t)-1;
    memset(&reader_state, 0, sizeof(reader_state));
    while (1) {
        const TextWord *current_word = text_processor_get_word(processor, index);
        char speech_error[128] = {0};
        size_t next_index = index;
        int interrupted_key = 0;
        int key;

        if (!current_word) {
            break;
        }

        render_word_reader(processor, selected_path, index, &reader_state);

        if (reader_state.autoplay) {
            if (!speech_engine_is_enabled()) {
                reader_state.autoplay = 0;
                reader_state.autoplay_mode = WORD_READER_AUTOPLAY_NONE;
                reader_state.waiting_for_line_continue = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_speech_off", "Speech mode is off. Turn speech on to use Ctrl+P playback."));
                key = read_key();
            } else {
                size_t chunk_end = (reader_state.autoplay_mode == WORD_READER_AUTOPLAY_LINE)
                    ? determine_reader_line_end(processor, index)
                    : determine_reader_chunk_end(processor, index);
                char *chunk_text = build_reader_chunk_text(processor, index, chunk_end);
                WordReaderPlaybackContext playback_context;

                if (!chunk_text) {
                    reader_state.autoplay = 0;
                    reader_state.autoplay_mode = WORD_READER_AUTOPLAY_NONE;
                    reader_state.waiting_for_line_continue = 0;
                    set_reader_status(&reader_state, menu_translate("ui_reader_playback_failed", "Unable to play buffered speech for this word."));
                    key = read_key();
                } else {
                    playback_context.processor = processor;
                    playback_context.selected_path = selected_path;
                    playback_context.reader_state = &reader_state;
                    playback_context.chunk_start = index;
                    playback_context.chunk_end = chunk_end;
                    playback_context.current_index = &index;

                    set_reader_status(&reader_state, NULL);
                    speech_engine_set_progress_callback(reader_playback_progress, &playback_context);
                    speech_engine_set_interrupt_callback(reader_playback_interrupt, NULL);
                    if (!speech_engine_speak_text_buffered(chunk_text, speech_error, sizeof(speech_error))) {
                        speech_engine_set_progress_callback(NULL, NULL);
                        speech_engine_set_interrupt_callback(NULL, NULL);
                        interrupted_key = speech_engine_take_interrupt_key();
                        free(chunk_text);
                        if (interrupted_key != 0) {
                            reader_state.autoplay = 0;
                            reader_state.autoplay_mode = WORD_READER_AUTOPLAY_NONE;
                            reader_state.waiting_for_line_continue = 0;
                            set_reader_status(&reader_state, NULL);
                            key = interrupted_key;
                        } else {
                            reader_state.autoplay = 0;
                            reader_state.autoplay_mode = WORD_READER_AUTOPLAY_NONE;
                            reader_state.waiting_for_line_continue = 0;
                            set_reader_status(&reader_state, speech_error[0] ? speech_error : menu_translate("ui_reader_playback_failed", "Unable to play buffered speech for this word."));
                            key = read_key();
                        }
                    } else {
                        speech_engine_set_progress_callback(NULL, NULL);
                        speech_engine_set_interrupt_callback(NULL, NULL);
                        free(chunk_text);
                        index = chunk_end;
                        last_spoken_index = index;

                        if (reader_state.autoplay_mode == WORD_READER_AUTOPLAY_LINE) {
                            reader_state.autoplay = 0;
                            reader_state.autoplay_mode = WORD_READER_AUTOPLAY_NONE;
                            if (chunk_end + 1 < processor->token_count) {
                                reader_state.waiting_for_line_continue = 1;
                                set_reader_status(&reader_state, menu_translate("ui_reader_line_wait_status", "Line complete. Press Space to play the next line."));
                                key = read_key();
                            } else {
                                reader_state.waiting_for_line_continue = 0;
                                set_reader_status(&reader_state, menu_translate("ui_reader_play_complete", "Playback complete."));
                                key = read_key();
                            }
                        } else {
                            if (chunk_end + 1 < processor->token_count) {
                                index = chunk_end + 1;
                                continue;
                            }

                            reader_state.autoplay = 0;
                            reader_state.autoplay_mode = WORD_READER_AUTOPLAY_NONE;
                            reader_state.waiting_for_line_continue = 0;
                            set_reader_status(&reader_state, menu_translate("ui_reader_play_complete", "Playback complete."));
                            key = read_key();
                        }
                    }
                }
            }
        } else {
            if (speech_engine_is_enabled() && last_spoken_index != index) {
                speech_engine_speak_text(current_word->word, speech_error, sizeof(speech_error));
                last_spoken_index = index;
            }
            key = read_key();
            if (key == KEY_DOWN || key == KEY_ENTER || key == KEY_UP) {
                int next_key;

                do {
                    if (key == KEY_DOWN || key == KEY_ENTER) {
                        next_index = (size_t)menu_next_index((int)next_index, 1, (int)processor->token_count);
                    } else if (key == KEY_UP) {
                        next_index = (size_t)menu_next_index((int)next_index, -1, (int)processor->token_count);
                    }

                    next_key = read_key_timeout(120);
                    if (next_key == KEY_DOWN || next_key == KEY_ENTER || next_key == KEY_UP) {
                        key = next_key;
                    } else {
                        if (next_key != 0) {
                            key = next_key;
                        }
                        break;
                    }
                } while (1);
            }
        }

        if (key == KEY_ESC) {
            break;
        } else if (key == KEY_CTRL_E) {
            export_processor_to_wave(processor, selected_path);
            last_spoken_index = (size_t)-1;
            set_reader_status(&reader_state, NULL);
        } else if (key == KEY_CTRL_P) {
            reader_state.autoplay = !reader_state.autoplay;
            if (reader_state.autoplay) {
                reader_state.autoplay_mode = WORD_READER_AUTOPLAY_CHUNK;
                reader_state.waiting_for_line_continue = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_play_starting", "Starting word-by-word playback..."));
                last_spoken_index = (size_t)-1;
            } else {
                reader_state.autoplay_mode = WORD_READER_AUTOPLAY_NONE;
                reader_state.waiting_for_line_continue = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_play_paused", "Playback paused."));
            }
        } else if (key == KEY_CTRL_L) {
            reader_state.autoplay = 1;
            reader_state.autoplay_mode = WORD_READER_AUTOPLAY_LINE;
            reader_state.waiting_for_line_continue = 0;
            set_reader_status(&reader_state, menu_translate("ui_reader_line_play_starting", "Starting line-by-line playback..."));
            last_spoken_index = (size_t)-1;
        } else if (key == KEY_DOWN || key == KEY_ENTER) {
            next_index = (size_t)menu_next_index((int)index, 1, (int)processor->token_count);
        } else if (key == KEY_UP) {
            next_index = (size_t)menu_next_index((int)index, -1, (int)processor->token_count);
        } else if (key == ' ' && reader_state.waiting_for_line_continue && index + 1 < processor->token_count) {
            reader_state.autoplay = 1;
            reader_state.autoplay_mode = WORD_READER_AUTOPLAY_LINE;
            reader_state.waiting_for_line_continue = 0;
            next_index = index + 1;
            set_reader_status(&reader_state, menu_translate("ui_reader_line_play_starting", "Starting line-by-line playback..."));
        } else if (key != 0 && reader_state.waiting_for_line_continue) {
            reader_state.waiting_for_line_continue = 0;
        }

        if (next_index != index) {
            index = next_index;
            if (!reader_state.autoplay) {
                set_reader_status(&reader_state, NULL);
            }
        }
    }

    speech_engine_shutdown();
    text_processor_free(processor);
    free(selected_path);
}

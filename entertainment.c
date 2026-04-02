#include "entertainment.h"
#include "config.h"
#include "download_ui.h"
#include "document_reader.h"
#include "file_manager.h"
#include "menu.h"
#include "speech_engine.h"
#include "text_processor.h"
#include <ctype.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

typedef struct {
    size_t start_token;
    size_t end_token;
} ReaderPage;

typedef struct {
    int autoplay;
    char status[160];
} WordReaderState;

static void get_terminal_size(int *rows, int *cols) {
    struct winsize ws;

    *rows = 24;
    *cols = 80;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_row > 0) {
            *rows = ws.ws_row;
        }
        if (ws.ws_col > 0) {
            *cols = ws.ws_col;
        }
    }
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

static int reader_autoplay_pause_ms(const TextWord *word) {
    if (!word) {
        return 0;
    }

    if (strchr(word->postpunctuation, '.') || strchr(word->postpunctuation, '!') || strchr(word->postpunctuation, '?')) {
        return 300;
    }
    if (strchr(word->postpunctuation, ',') || strchr(word->postpunctuation, ';') || strchr(word->postpunctuation, ':')) {
        return 150;
    }
    if (strchr(word->whitespace, '\n')) {
        return 180;
    }

    return 35;
}

static void render_word_reader(const TextProcessor *processor, const char *selected_path, size_t current_index, const WordReaderState *state) {
    int rows;
    int cols;
    int content_lines;
    ReaderPage page;
    size_t i;
    const TextWord *current_word = text_processor_get_word(processor, current_index);

    get_terminal_size(&rows, &cols);
    content_lines = rows - 7;
    if (content_lines < 5) {
        content_lines = 5;
    }

    page = compute_reader_page(processor, current_index, cols, content_lines);

    printf("\033[H\033[J--- %s ---\n", menu_translate("color_reader", "Color Reader"));
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

    printf("\n\n%s\n", menu_translate("ui_footer_word_reader", "[Up: Previous | Down: Next | Enter: Next | Ctrl+E: Export to wave | Ctrl+P: Play | Esc: Back]"));
    printf("%s\n", menu_translate("ui_reader_highlight_help", "The highlighted word represents the token currently being spoken."));
    printf("%s\n", menu_translate("ui_reader_export_help", "Use Ctrl+E to export the current document to a WAV file."));
    printf("%s\n", menu_translate("ui_reader_play_help", "Use Ctrl+P to play the document word by word with live highlighting."));
    if (state && state->status[0]) {
        printf("%s\n", state->status);
    } else if (state && state->autoplay) {
        printf("%s\n", menu_translate("ui_reader_playing_status", "Playing. Press Ctrl+P to pause after the current word."));
    }
    fflush(stdout);
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

        printf("\033[H\033[J--- Joke ---\n\n%s\n\n", joke_text);
        printf("------------------------------------------\n");
        printf("%s\n", menu_translate("ui_footer_joke", "[Space: New Joke | Esc: Back]"));
        fflush(stdout);

        int key = read_key();
        if (key == KEY_ESC) break;
        else if (key == ' ') fetch_new = 1;
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

        int key = read_key();
        if (key == KEY_ESC) break;
        else if (key == KEY_UP && sel > 0) sel--;
        else if (key == KEY_DOWN && sel < story_count - 1) sel++;
        else if (key == KEY_ENTER) {
            cJSON *story_obj = cJSON_GetArrayItem(json, sel);
            cJSON *title_node = cJSON_GetObjectItemCaseSensitive(story_obj, "title");
            cJSON *content_node = cJSON_GetObjectItemCaseSensitive(story_obj, "story");
            cJSON *moral_node = cJSON_GetObjectItemCaseSensitive(story_obj, "moral");
            
            printf("\033[H\033[J--- %s ---\n\n", title_node ? title_node->valuestring : "Story");
            if (content_node) {
                printf("%s\n", content_node->valuestring);
            }
            if (moral_node) {
                printf("\n\nMoral: %s\n", moral_node->valuestring);
            }
            printf("\n\n%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
            fflush(stdout); read_key();
        }
    }

    cJSON_Delete(json);
}

void content_ui_show_poems(void) {
    const char *url = "https://poetrydb.org/random/1";
    char error[256] = {0};
    char *response = fetch_text_with_progress_ui("Poems", url, "poem data", error, sizeof(error));
    if (response) {
        cJSON *json = cJSON_Parse(response);
        free(response);
        if (json && cJSON_IsArray(json)) {
            cJSON *item = cJSON_GetArrayItem(json, 0);
            cJSON *title = cJSON_GetObjectItemCaseSensitive(item, "title");
            cJSON *author = cJSON_GetObjectItemCaseSensitive(item, "author");
            cJSON *lines = cJSON_GetObjectItemCaseSensitive(item, "lines");

            if (cJSON_IsArray(lines)) {
                int total_lines = cJSON_GetArraySize(lines);
                int current_line = 0;
                int POEM_PAGE_SIZE = 20;

                while (current_line < total_lines) {
                    printf("\033[H\033[J--- %s ---\nBy %s\n\n", 
                        title ? title->valuestring : "Poem", 
                        author ? author->valuestring : "Unknown");

                    int end_line = current_line + POEM_PAGE_SIZE;
                    if (end_line > total_lines) end_line = total_lines;

                    for (int i = current_line; i < end_line; i++) {
                        cJSON *line = cJSON_GetArrayItem(lines, i);
                        printf("%s\n", line->valuestring);
                    }

                    if (end_line < total_lines) {
                        printf("\n[Space: Next Page | Esc: Back]\n");
                    } else {
                        printf("\n[%s]\n", menu_translate("ui_end_of_poem_go_back", "End of Poem. Press any key to go back..."));
                    }
                    fflush(stdout);

                    int key = read_key();
                    if (key == KEY_ESC) break;
                    if (key == ' ' && end_line < total_lines) {
                        current_line = end_line;
                    } else if (end_line >= total_lines) {
                        break;
                    }
                }
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
        int key;

        if (!current_word) {
            break;
        }

        render_word_reader(processor, selected_path, index, &reader_state);

        if (reader_state.autoplay) {
            if (!speech_engine_is_enabled()) {
                reader_state.autoplay = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_speech_off", "Speech mode is off. Turn speech on to use Ctrl+P playback."));
                key = read_key();
            } else if (!speech_engine_speak_text_buffered(current_word->word, speech_error, sizeof(speech_error))) {
                reader_state.autoplay = 0;
                set_reader_status(&reader_state, speech_error[0] ? speech_error : menu_translate("ui_reader_playback_failed", "Unable to play buffered speech for this word."));
                key = read_key();
            } else {
                last_spoken_index = index;
                set_reader_status(&reader_state, NULL);
                key = read_key_timeout(reader_autoplay_pause_ms(current_word));
            }
        } else {
            if (speech_engine_is_enabled() && last_spoken_index != index) {
                speech_engine_speak_text(current_word->word, speech_error, sizeof(speech_error));
                last_spoken_index = index;
            }
            key = read_key();
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
                set_reader_status(&reader_state, menu_translate("ui_reader_play_starting", "Starting word-by-word playback..."));
                last_spoken_index = (size_t)-1;
            } else {
                set_reader_status(&reader_state, menu_translate("ui_reader_play_paused", "Playback paused."));
            }
        } else if (key == KEY_DOWN || key == KEY_ENTER) {
            next_index = (size_t)menu_next_index((int)index, 1, (int)processor->token_count);
        } else if (key == KEY_UP) {
            next_index = (size_t)menu_next_index((int)index, -1, (int)processor->token_count);
        } else if (reader_state.autoplay && key == 0) {
            if (index + 1 < processor->token_count) {
                next_index = index + 1;
            } else {
                reader_state.autoplay = 0;
                set_reader_status(&reader_state, menu_translate("ui_reader_play_complete", "Playback complete."));
            }
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

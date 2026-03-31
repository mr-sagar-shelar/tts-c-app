#include "entertainment.h"
#include "file_manager.h"
#include "text_processor.h"
#include <ctype.h>
#include <sys/stat.h>

void handle_joke() {
    int fetch_new = 1;
    char joke_text[2048] = {0};

    while (1) {
        if (fetch_new) {
            printf("\033[H\033[J--- Joke ---\nFetching a new joke...\n");
            fflush(stdout);

            const char *url = "https://v2.jokeapi.dev/joke/Any?format=json&type=single&lang=en&amount=1";
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "curl -s \"%s\"", url);

            FILE *fp = popen(cmd, "r");
            if (fp) {
                char response[4096] = {0};
                fread(response, 1, sizeof(response) - 1, fp);
                pclose(fp);

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
            } else {
                strcpy(joke_text, "Failed to connect to Joke API.");
            }
            fetch_new = 0;
        }

        printf("\033[H\033[J--- Joke ---\n\n%s\n\n", joke_text);
        printf("------------------------------------------\n");
        printf("[Space: New Joke | Esc: Back]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_ESC) break;
        else if (key == ' ') fetch_new = 1;
    }
}

void handle_short_stories() {
    const char *story_file = "Downloads/ShortStories.json";
    char *data = NULL;
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
        printf("\033[H\033[J--- Short Stories ---\nDownloading stories...\n");
        fflush(stdout);

        const char *url = "https://shortstories-api.onrender.com/stories";
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "curl -s \"%s\"", url);

        FILE *fp = popen(cmd, "r");
        if (!fp) {
            printf("Failed to fetch stories.\nPress any key..."); fflush(stdout); read_key();
            return;
        }

        size_t response_len = 0;
        size_t response_size = 16384;
        data = (char *)malloc(response_size);
        
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            size_t len = strlen(buffer);
            if (response_len + len + 1 > response_size) {
                response_size *= 2;
                data = (char *)realloc(data, response_size);
            }
            strcpy(data + response_len, buffer);
            response_len += len;
        }
        pclose(fp);

        if (response_len == 0) {
            printf("\nNo response from server. Check internet connection.\nPress any key...");
            fflush(stdout); read_key();
            free(data);
            return;
        }

        f = fopen(story_file, "wb");
        if (f) {
            fwrite(data, 1, response_len, f);
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
        
        printf("\n[Arrows: Navigate | Enter: Read | Esc: Back]\n");
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
            printf("\n\nPress any key to go back...");
            fflush(stdout); read_key();
        }
    }

    cJSON_Delete(json);
}

void handle_poems() {
    printf("\033[H\033[J--- Poems ---\n");
    printf("Fetching a random poem...\n");
    
    const char *url = "https://poetrydb.org/random/1";
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "curl -s \"%s\"", url);

    FILE *fp = popen(cmd, "r");
    if (fp) {
        char response[16384] = {0};
        fread(response, 1, sizeof(response) - 1, fp);
        pclose(fp);

        cJSON *json = cJSON_Parse(response);
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
                        printf("\n[End of Poem. Press any key to go back...]\n");
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
        printf("\nFailed to connect to Poem API. Press any key...");
        fflush(stdout); read_key();
    }
}

void handle_word_by_word_viewer() {
    char *selected_path = file_navigator(USER_SPACE, 0);
    struct stat st;

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

    TextProcessor *processor = text_processor_load(selected_path);
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
    while (1) {
        const TextWord *word = text_processor_get_word(processor, index);
        if (!word) {
            break;
        }

        printf("\033[H\033[J--- Color Reader ---\n");
        printf("File: %s\n", selected_path);
        printf("Word %zu of %zu\n\n", index + 1, processor->token_count);
        printf("Surface: %s\n", word->surface);
        printf("Word: %s\n", word->word);
        printf("Pre-punctuation: %s\n", word->prepunctuation[0] ? word->prepunctuation : "(none)");
        printf("Post-punctuation: %s\n", word->postpunctuation[0] ? word->postpunctuation : "(none)");
        printf("Leading whitespace length: %zu\n", strlen(word->whitespace));
        printf("File offset: %zu\n", word->content_offset);
        printf("Line: %d  Column: %d\n", word->line, word->column);
        printf("\nThis token stream is ready to be passed word-by-word into Flite later.\n");
        printf("[Up: Previous | Down: Next | Enter: Next | Esc: Back]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_ESC) {
            break;
        } else if ((key == KEY_DOWN || key == KEY_ENTER) && index + 1 < processor->token_count) {
            index++;
        } else if (key == KEY_UP && index > 0) {
            index--;
        }
    }

    text_processor_free(processor);
    free(selected_path);
}

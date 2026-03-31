#include "dictionary.h"
#include <ctype.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "download_ui.h"
#include "menu.h"

static void play_downloaded_audio_file(const char *path) {
    pid_t pid = fork();

    if (pid == 0) {
        execlp("afplay", "afplay", path, (char *)NULL);
        _exit(1);
    }

    if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

static void download_and_play_audio(const char *screen_title, const char *audio_url) {
    char error[256] = {0};
    const char *audio_path = "/tmp/dict_audio.mp3";

    if (!audio_url || !audio_url[0]) {
        return;
    }

    if (!download_file_with_progress_ui(screen_title, audio_url, audio_path, "dict_audio.mp3", error, sizeof(error))) {
        printf("\033[H\033[J--- %s ---\n%s\n\nPress any key...", screen_title, error[0] ? error : "Audio download failed.");
        fflush(stdout);
        read_key();
        return;
    }

    printf("\033[H\033[J--- %s ---\nPlaying audio...\n", screen_title);
    fflush(stdout);
    play_downloaded_audio_file(audio_path);
    printf("Done. Press Esc to go back or 'p' to replay.");
    fflush(stdout);
}

void handle_dictionary() {
    char *lang = get_setting("language");
    if (!lang) lang = strdup("en");

    char dict_file[32];
    snprintf(dict_file, sizeof(dict_file), "dict_%s.json", lang);
    free(lang);

    FILE *f = fopen(dict_file, "rb");
    if (!f) {
        printf("\033[H\033[J--- Sense Dictionary ---\n");
        printf(menu_translate("ui_dictionary_file_missing_format", "Dictionary file '%s' not found. Press any key..."), dict_file);
        fflush(stdout); read_key();
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *dict_json = cJSON_Parse(data);
    free(data);
    if (!dict_json) {
        printf("\033[H\033[J--- Sense Dictionary ---\nError parsing dictionary file. Press any key...");
        fflush(stdout); read_key();
        return;
    }

    cJSON *words_array = cJSON_GetObjectItemCaseSensitive(dict_json, "words");
    int total_words = cJSON_GetArraySize(words_array);

    char search_term[256] = {0};
    int sel = 0;

    while (1) {
        printf("\033[H\033[J--- Sense Dictionary ---\n");
        printf("%s: %s_\n", menu_translate("ui_search", "Search"), search_term);
        printf("---------------------------\n");

        int matches[512];
        int match_count = 0;
        char lower_search[256] = {0};
        for(int i=0; search_term[i]; i++) lower_search[i] = (char)tolower(search_term[i]);

        for (int i = 0; i < total_words && match_count < 512; i++) {
            cJSON *item = cJSON_GetArrayItem(words_array, i);
            const char *word = cJSON_GetObjectItemCaseSensitive(item, "word")->valuestring;
            
            char lower_word[256] = {0};
            for(int j=0; word[j] && j < 255; j++) lower_word[j] = (char)tolower(word[j]);

            if (strlen(lower_search) == 0 || strstr(lower_word, lower_search) != NULL) {
                matches[match_count++] = i;
            }
        }

        if (sel >= match_count) sel = (match_count > 0) ? match_count - 1 : 0;

        if (match_count == 0) {
            printf("  (No matching words)\n");
        } else {
            for (int i = 0; i < match_count && i < 15; i++) {
                cJSON *item = cJSON_GetArrayItem(words_array, matches[i]);
                const char *word = cJSON_GetObjectItemCaseSensitive(item, "word")->valuestring;
                if (i == sel) printf("> %s\n", word);
                else printf("  %s\n", word);
            }
            if (match_count > 15) printf("  ... and %d more\n", match_count - 15);
        }

        fflush(stdout);
        int key = read_key();

        if (key == KEY_ESC) break;
        else if (key == KEY_UP && sel > 0) sel--;
        else if (key == KEY_DOWN && sel < match_count - 1) sel++;
        else if (key == KEY_ENTER && match_count > 0) {
            cJSON *item = cJSON_GetArrayItem(words_array, matches[sel]);
            const char *word = cJSON_GetObjectItemCaseSensitive(item, "word")->valuestring;
            const char *meaning = cJSON_GetObjectItemCaseSensitive(item, "meaning")->valuestring;
            printf("\033[H\033[J--- Word Detail ---\n\nWord: %s\n\nMeaning:\n%s\n\nPress any key to return...", word, meaning);
            fflush(stdout); read_key();
        } else if (key == KEY_BACKSPACE) {
            int slen = (int)strlen(search_term);
            if (slen > 0) search_term[slen - 1] = '\0';
            sel = 0;
        } else if (key > 0 && key < 1000 && isprint(key)) {
            int slen = (int)strlen(search_term);
            if (slen < 254) {
                search_term[slen] = (char)key;
                search_term[slen + 1] = '\0';
            }
            sel = 0;
        }
    }

    cJSON_Delete(dict_json);
}

void handle_english_only_dictionary() {
    char word[256];
    char error[256] = {0};
    char *response;
    get_user_input(word, sizeof(word), "Enter word for English Only Dictionary");
    if (strlen(word) == 0) return;

    char *encoded_word = url_encode(word);
    if (!encoded_word) return;

    printf("\033[H\033[J--- English Only Dictionary ---\nSearching for '%s'...\n", word);
    fflush(stdout);

    char url[1024];
    snprintf(url, sizeof(url), "https://api.dictionaryapi.dev/api/v2/entries/en/%s", encoded_word);
    free(encoded_word);

    response = fetch_text_with_progress_ui("English Only Dictionary", url, "dictionary lookup", error, sizeof(error));
    if (!response || !response[0]) {
        printf("\n%s\nPress any key...", error[0] ? error : "No response from server. Check internet connection.");
        fflush(stdout);
        read_key();
        free(response);
        return;
    }

    cJSON *json = cJSON_Parse(response);
    free(response);

    if (!json) {
        printf("\nError parsing JSON response.\nPress any key...");
        fflush(stdout); read_key();
        return;
    }

    if (!cJSON_IsArray(json)) {
        cJSON *title = cJSON_GetObjectItemCaseSensitive(json, "title");
        cJSON *message = cJSON_GetObjectItemCaseSensitive(json, "message");
        printf("\033[H\033[J--- English Only Dictionary ---\n\n");
        if (title && cJSON_IsString(title)) printf("%s\n\n", title->valuestring);
        if (message && cJSON_IsString(message)) printf("%s\n", message->valuestring);
        printf("\n%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
        fflush(stdout); read_key();
        cJSON_Delete(json);
        return;
    }

    cJSON *first_entry = cJSON_GetArrayItem(json, 0);
    cJSON *word_node = cJSON_GetObjectItemCaseSensitive(first_entry, "word");
    cJSON *phonetics = cJSON_GetObjectItemCaseSensitive(first_entry, "phonetics");
    cJSON *meanings = cJSON_GetObjectItemCaseSensitive(first_entry, "meanings");

    char audio_url[1024] = {0};
    if (cJSON_IsArray(phonetics)) {
        int phonetics_count = cJSON_GetArraySize(phonetics);
        for (int i = 0; i < phonetics_count; i++) {
            cJSON *p = cJSON_GetArrayItem(phonetics, i);
            cJSON *a = cJSON_GetObjectItemCaseSensitive(p, "audio");
            if (a && cJSON_IsString(a) && strlen(a->valuestring) > 0) {
                strncpy(audio_url, a->valuestring, sizeof(audio_url) - 1);
                break;
            }
        }
    }

    printf("\033[H\033[J--- English Only Dictionary: %s ---\n\n", word_node && cJSON_IsString(word_node) ? word_node->valuestring : word);
    
    if (cJSON_IsArray(meanings)) {
        int meanings_count = cJSON_GetArraySize(meanings);
        for (int i = 0; i < meanings_count && i < 3; i++) {
            cJSON *m = cJSON_GetArrayItem(meanings, i);
            cJSON *pos = cJSON_GetObjectItemCaseSensitive(m, "partOfSpeech");
            cJSON *defs = cJSON_GetObjectItemCaseSensitive(m, "definitions");
            
            if (pos && cJSON_IsString(pos)) {
                printf("[%s]\n", pos->valuestring);
            }
            if (cJSON_IsArray(defs)) {
                cJSON *first_def = cJSON_GetArrayItem(defs, 0);
                cJSON *def_text = cJSON_GetObjectItemCaseSensitive(first_def, "definition");
                if (def_text && cJSON_IsString(def_text)) {
                    printf("  - %s\n\n", def_text->valuestring);
                }
            }
        }
    }

    if (strlen(audio_url) > 0) {
        printf("Press 'p' to play audio, Esc to go back.");
    } else {
        printf("%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
    }
    fflush(stdout);

    while (1) {
        int key = read_key();
        if (key == KEY_ESC) break;
        else if (key == 'p' || key == 'P') {
            if (strlen(audio_url) > 0) {
                download_and_play_audio("English Only Dictionary", audio_url);
            }
        } else if (strlen(audio_url) == 0) break;
    }
    cJSON_Delete(json);
}

void handle_multi_lang_dictionary() {
    char word[256];
    char error[256] = {0};
    char *response;
    get_user_input(word, sizeof(word), "Enter word for Multi Language Dictionary");
    if (strlen(word) == 0) return;

    char *encoded_word = url_encode(word);
    if (!encoded_word) return;

    char *lang = get_setting("language");
    char api_lang[16] = "en";
    if (lang) {
        if (strcmp(lang, "hi") == 0) {
            strcpy(api_lang, "hi");
        } else {
            strncpy(api_lang, lang, sizeof(api_lang) - 1);
        }
        free(lang);
    }

    printf("\033[H\033[J--- Multi Language Dictionary ---\nSearching for '%s' in %s...\n", word, api_lang);
    fflush(stdout);

    char url[1024];
    snprintf(url, sizeof(url), "https://freedictionaryapi.com/api/v1/entries/%s/%s", api_lang, encoded_word);
    free(encoded_word);

    response = fetch_text_with_progress_ui("Multi Language Dictionary", url, "dictionary lookup", error, sizeof(error));
    if (!response || !response[0]) {
        printf("\n%s\nPress any key...", error[0] ? error : "No response from server. Check internet connection.");
        fflush(stdout);
        read_key();
        free(response);
        return;
    }

    cJSON *json = cJSON_Parse(response);
    free(response);

    if (!json) {
        printf("\nError parsing JSON response.\nPress any key...");
        fflush(stdout); read_key();
        return;
    }

    cJSON *word_node = cJSON_GetObjectItemCaseSensitive(json, "word");
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(json, "entries");

    if (!word_node || !cJSON_IsArray(entries)) {
        printf("\033[H\033[J--- Multi Language Dictionary ---\n\nWord not found or error in response.\n");
        printf("\n%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
        fflush(stdout); read_key();
        cJSON_Delete(json);
        return;
    }

    printf("\033[H\033[J--- Multi Language Dictionary: %s ---\n\n", word_node->valuestring);
    
    int entries_count = cJSON_GetArraySize(entries);
    char audio_url[1024] = {0};

    for (int i = 0; i < entries_count && i < 2; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries, i);
        cJSON *senses = cJSON_GetObjectItemCaseSensitive(entry, "senses");
        cJSON *phonetics = cJSON_GetObjectItemCaseSensitive(entry, "phonetics");

        if (strlen(audio_url) == 0 && cJSON_IsArray(phonetics)) {
            int p_count = cJSON_GetArraySize(phonetics);
            for (int j = 0; j < p_count; j++) {
                cJSON *p = cJSON_GetArrayItem(phonetics, j);
                cJSON *a = cJSON_GetObjectItemCaseSensitive(p, "audio");
                if (a && cJSON_IsString(a) && strlen(a->valuestring) > 0) {
                    strncpy(audio_url, a->valuestring, sizeof(audio_url) - 1);
                    break;
                }
            }
        }

        if (cJSON_IsArray(senses)) {
            int senses_count = cJSON_GetArraySize(senses);
            for (int j = 0; j < senses_count && j < 2; j++) {
                cJSON *sense = cJSON_GetArrayItem(senses, j);
                cJSON *def = cJSON_GetObjectItemCaseSensitive(sense, "definition");
                if (def && cJSON_IsString(def)) {
                    printf("  - %s\n", def->valuestring);
                }
            }
            printf("\n");
        }
    }

    if (strlen(audio_url) > 0) {
        printf("Press 'p' to play audio, Esc to go back.");
    } else {
        printf("%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
    }
    fflush(stdout);

    while (1) {
        int key = read_key();
        if (key == KEY_ESC) break;
        else if (key == 'p' || key == 'P') {
            if (strlen(audio_url) > 0) {
                download_and_play_audio("Multi Language Dictionary", audio_url);
            }
        } else if (strlen(audio_url) == 0) break;
    }

    cJSON_Delete(json);
}

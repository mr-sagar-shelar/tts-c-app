#include "trivia.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "download_ui.h"
#include "entertainment.h"
#include "menu.h"
#include "menu_audio.h"
#include "speech_engine.h"
#include "utils.h"

#define TRIVIA_CONFIG_FILE "trivia.json"

typedef struct {
    const char *label;
    const char *value;
} TriviaOption;

typedef struct {
    cJSON *json;
} TriviaState;

static TriviaState trivia_state = {0};

static const TriviaOption trivia_amount_options[] = {
    {"5", "5"},
    {"10", "10"},
    {"15", "15"},
    {"20", "20"},
    {"25", "25"},
    {"50", "50"}
};

static const TriviaOption trivia_category_options[] = {
    {"Any Category", ""},
    {"General Knowledge", "9"},
    {"Entertainment: Books", "10"},
    {"Entertainment: Film", "11"},
    {"Entertainment: Music", "12"},
    {"Entertainment: Musicals & Theatres", "13"},
    {"Entertainment: Television", "14"},
    {"Entertainment: Video Games", "15"},
    {"Entertainment: Board Games", "16"},
    {"Science & Nature", "17"},
    {"Science: Computers", "18"},
    {"Science: Mathematics", "19"},
    {"Mythology", "20"},
    {"Sports", "21"},
    {"Geography", "22"},
    {"History", "23"},
    {"Politics", "24"},
    {"Art", "25"},
    {"Celebrities", "26"},
    {"Animals", "27"},
    {"Vehicles", "28"},
    {"Entertainment: Comics", "29"},
    {"Science: Gadgets", "30"},
    {"Entertainment: Japanese Anime & Manga", "31"},
    {"Entertainment: Cartoon & Animations", "32"}
};

static const TriviaOption trivia_difficulty_options[] = {
    {"Any Difficulty", ""},
    {"Easy", "easy"},
    {"Medium", "medium"},
    {"Hard", "hard"}
};

static const TriviaOption trivia_type_options[] = {
    {"Any Type", ""},
    {"Multiple Choice", "multiple"},
    {"True / False", "boolean"}
};

static const TriviaOption trivia_encoding_options[] = {
    {"Default Encoding", ""},
    {"Legacy URL Encoding", "urlLegacy"},
    {"URL Encoding (RFC 3986)", "url3986"},
    {"Base64 Encoding", "base64"}
};

static void trivia_set_defaults(void) {
    if (trivia_state.json) {
        cJSON_Delete(trivia_state.json);
    }

    trivia_state.json = cJSON_CreateObject();
    cJSON_AddStringToObject(trivia_state.json, "amount", "5");
    cJSON_AddStringToObject(trivia_state.json, "category", "");
    cJSON_AddStringToObject(trivia_state.json, "difficulty", "");
    cJSON_AddStringToObject(trivia_state.json, "type", "");
    cJSON_AddStringToObject(trivia_state.json, "encoding", "");
}

static void trivia_save(void) {
    char *output;
    FILE *file;

    if (!trivia_state.json) {
        trivia_set_defaults();
    }

    output = cJSON_Print(trivia_state.json);
    if (!output) {
        return;
    }

    file = fopen(TRIVIA_CONFIG_FILE, "w");
    if (file) {
        fputs(output, file);
        fclose(file);
    }
    free(output);
}

static void trivia_ensure_string(const char *key, const char *default_value) {
    cJSON *item;

    if (!trivia_state.json) {
        trivia_set_defaults();
    }

    item = cJSON_GetObjectItemCaseSensitive(trivia_state.json, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        if (item) {
            cJSON_ReplaceItemInObject(trivia_state.json, key, cJSON_CreateString(default_value));
        } else {
            cJSON_AddStringToObject(trivia_state.json, key, default_value);
        }
    }
}

void init_trivia(void) {
    FILE *file;
    long length;
    char *data;

    if (trivia_state.json) {
        return;
    }

    file = fopen(TRIVIA_CONFIG_FILE, "rb");
    if (!file) {
        trivia_set_defaults();
        trivia_save();
        return;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    data = (char *)malloc((size_t)length + 1);
    if (!data) {
        fclose(file);
        trivia_set_defaults();
        trivia_save();
        return;
    }

    fread(data, 1, (size_t)length, file);
    data[length] = '\0';
    fclose(file);

    trivia_state.json = cJSON_Parse(data);
    free(data);

    if (!trivia_state.json || !cJSON_IsObject(trivia_state.json)) {
        trivia_set_defaults();
        trivia_save();
        return;
    }

    trivia_ensure_string("amount", "5");
    trivia_ensure_string("category", "");
    trivia_ensure_string("difficulty", "");
    trivia_ensure_string("type", "");
    trivia_ensure_string("encoding", "");
    trivia_save();
}

void cleanup_trivia(void) {
    if (trivia_state.json) {
        cJSON_Delete(trivia_state.json);
        trivia_state.json = NULL;
    }
}

static const char *trivia_get_value(const char *key) {
    cJSON *item;

    if (!trivia_state.json) {
        init_trivia();
    }

    item = cJSON_GetObjectItemCaseSensitive(trivia_state.json, key);
    if (cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }

    return "";
}

static const TriviaOption *trivia_options_for_key(const char *menu_key, int *count, const char **config_key) {
    if (count) {
        *count = 0;
    }
    if (config_key) {
        *config_key = NULL;
    }

    if (strcmp(menu_key, "trivia_amount") == 0) {
        if (count) *count = (int)(sizeof(trivia_amount_options) / sizeof(trivia_amount_options[0]));
        if (config_key) *config_key = "amount";
        return trivia_amount_options;
    }
    if (strcmp(menu_key, "trivia_category") == 0) {
        if (count) *count = (int)(sizeof(trivia_category_options) / sizeof(trivia_category_options[0]));
        if (config_key) *config_key = "category";
        return trivia_category_options;
    }
    if (strcmp(menu_key, "trivia_difficulty") == 0) {
        if (count) *count = (int)(sizeof(trivia_difficulty_options) / sizeof(trivia_difficulty_options[0]));
        if (config_key) *config_key = "difficulty";
        return trivia_difficulty_options;
    }
    if (strcmp(menu_key, "trivia_type") == 0) {
        if (count) *count = (int)(sizeof(trivia_type_options) / sizeof(trivia_type_options[0]));
        if (config_key) *config_key = "type";
        return trivia_type_options;
    }
    if (strcmp(menu_key, "trivia_encoding") == 0) {
        if (count) *count = (int)(sizeof(trivia_encoding_options) / sizeof(trivia_encoding_options[0]));
        if (config_key) *config_key = "encoding";
        return trivia_encoding_options;
    }

    return NULL;
}

char *trivia_get_selected_label(const char *menu_key) {
    int count = 0;
    int i;
    const char *config_key = NULL;
    const char *value;
    const TriviaOption *options = trivia_options_for_key(menu_key, &count, &config_key);

    if (!options || !config_key) {
        return NULL;
    }

    value = trivia_get_value(config_key);
    for (i = 0; i < count; i++) {
        if (strcmp(options[i].value, value) == 0) {
            return strdup(options[i].label);
        }
    }

    return NULL;
}

static void trivia_show_picker(const char *title, const char *menu_key) {
    int count = 0;
    int selected = 0;
    int i;
    const char *config_key = NULL;
    const char *current_value;
    const TriviaOption *options = trivia_options_for_key(menu_key, &count, &config_key);

    if (!options || !config_key || count <= 0) {
        return;
    }

    current_value = trivia_get_value(config_key);
    for (i = 0; i < count; i++) {
        if (strcmp(options[i].value, current_value) == 0) {
            selected = i;
            break;
        }
    }

    while (1) {
        printf("\033[H\033[J--- %s ---\n", title);
        printf("%s: %s\n\n",
               menu_translate("ui_selected_value", "Selected Value"),
               options[selected].label);
        for (i = 0; i < count; i++) {
            printf("%c %s\n", i == selected ? '>' : ' ', options[i].label);
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, count);
            } else if (key == KEY_ENTER) {
                cJSON_ReplaceItemInObject(trivia_state.json, config_key, cJSON_CreateString(options[selected].value));
                trivia_save();
                return;
            } else if (key == KEY_ESC) {
                return;
            }
        }
    }
}

void trivia_show_settings_menu(const char *menu_key) {
    if (!menu_key) {
        return;
    }

    if (strcmp(menu_key, "trivia_amount") == 0) {
        trivia_show_picker("Number Of Questions", menu_key);
    } else if (strcmp(menu_key, "trivia_category") == 0) {
        trivia_show_picker("Category", menu_key);
    } else if (strcmp(menu_key, "trivia_difficulty") == 0) {
        trivia_show_picker("Difficulty", menu_key);
    } else if (strcmp(menu_key, "trivia_type") == 0) {
        trivia_show_picker("Type", menu_key);
    } else if (strcmp(menu_key, "trivia_encoding") == 0) {
        trivia_show_picker("Encoding", menu_key);
    }
}

static int trivia_hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static char *trivia_decode_url(const char *text, int plus_as_space) {
    size_t len;
    char *out;
    size_t i;
    size_t pos = 0;

    if (!text) {
        return strdup("");
    }

    len = strlen(text);
    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }

    for (i = 0; i < len; i++) {
        if (text[i] == '%' && i + 2 < len) {
            int hi = trivia_hex_value(text[i + 1]);
            int lo = trivia_hex_value(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out[pos++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        if (plus_as_space && text[i] == '+') {
            out[pos++] = ' ';
        } else {
            out[pos++] = text[i];
        }
    }
    out[pos] = '\0';
    return out;
}

static int trivia_base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static char *trivia_decode_base64(const char *text) {
    size_t len;
    char *out;
    size_t out_len = 0;
    size_t i;

    if (!text) {
        return strdup("");
    }

    len = strlen(text);
    out = (char *)malloc((len * 3) / 4 + 4);
    if (!out) {
        return NULL;
    }

    for (i = 0; i < len; ) {
        int vals[4] = {0, 0, 0, 0};
        int pads = 0;
        int j;

        for (j = 0; j < 4 && i < len; ) {
            char c = text[i++];
            if (c == '=') {
                vals[j++] = 0;
                pads++;
            } else {
                int v = trivia_base64_value(c);
                if (v >= 0) {
                    vals[j++] = v;
                }
            }
        }
        if (j < 4) {
            break;
        }

        out[out_len++] = (char)((vals[0] << 2) | (vals[1] >> 4));
        if (pads < 2) {
            out[out_len++] = (char)(((vals[1] & 0x0F) << 4) | (vals[2] >> 2));
        }
        if (pads < 1) {
            out[out_len++] = (char)(((vals[2] & 0x03) << 6) | vals[3]);
        }
    }

    out[out_len] = '\0';
    return out;
}

static char *trivia_decode_html_entities(const char *text) {
    size_t len;
    char *out;
    size_t i;
    size_t pos = 0;

    if (!text) {
        return strdup("");
    }

    len = strlen(text);
    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }

    for (i = 0; i < len; i++) {
        if (text[i] == '&') {
            if (strncmp(text + i, "&quot;", 6) == 0) {
                out[pos++] = '"';
                i += 5;
                continue;
            }
            if (strncmp(text + i, "&#039;", 6) == 0 || strncmp(text + i, "&apos;", 6) == 0) {
                out[pos++] = '\'';
                i += 5;
                continue;
            }
            if (strncmp(text + i, "&amp;", 5) == 0) {
                out[pos++] = '&';
                i += 4;
                continue;
            }
            if (strncmp(text + i, "&lt;", 4) == 0) {
                out[pos++] = '<';
                i += 3;
                continue;
            }
            if (strncmp(text + i, "&gt;", 4) == 0) {
                out[pos++] = '>';
                i += 3;
                continue;
            }
            if (strncmp(text + i, "&uuml;", 6) == 0) {
                out[pos++] = 'u';
                i += 5;
                continue;
            }
            if (text[i + 1] == '#') {
                long value = 0;
                char *endptr = NULL;
                if (text[i + 2] == 'x' || text[i + 2] == 'X') {
                    value = strtol(text + i + 3, &endptr, 16);
                } else {
                    value = strtol(text + i + 2, &endptr, 10);
                }
                if (endptr && *endptr == ';' && value > 0 && value < 256) {
                    out[pos++] = (char)value;
                    i = (size_t)(endptr - text);
                    continue;
                }
            }
        }
        out[pos++] = text[i];
    }
    out[pos] = '\0';
    return out;
}

static char *trivia_decode_text(const char *text) {
    const char *encoding = trivia_get_value("encoding");

    if (strcmp(encoding, "base64") == 0) {
        return trivia_decode_base64(text);
    }
    if (strcmp(encoding, "url3986") == 0) {
        return trivia_decode_url(text, 0);
    }
    if (strcmp(encoding, "urlLegacy") == 0) {
        return trivia_decode_url(text, 1);
    }

    return trivia_decode_html_entities(text);
}

static void trivia_shuffle_strings(char **items, int count) {
    int i;

    for (i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        char *tmp = items[i];
        items[i] = items[j];
        items[j] = tmp;
    }
}

static void trivia_show_message(const char *title, const char *message) {
    printf("\033[H\033[J--- %s ---\n\n%s\n\n%s",
           title,
           message,
           menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);
    read_key();
}

static int trivia_play_question(const char *question, int question_number, int total_questions) {
    char title[64];
    char footer[128];

    snprintf(title, sizeof(title), "Trivia %d of %d", question_number, total_questions);
    snprintf(footer, sizeof(footer), "[Enter: Show Answers | Esc: Back]");
    return content_ui_run_spoken_stage(title, question, footer);
}

static int trivia_choose_answer(const char *question,
                                char **answers,
                                int answer_count,
                                int question_number,
                                int total_questions) {
    int selected = 0;
    int last_spoken = -1;
    int i;

    while (1) {
        printf("\033[H\033[J--- Trivia ---\n");
        printf("Question %d of %d\n\n", question_number, total_questions);
        printf("%s\n\n", question ? question : "");
        for (i = 0; i < answer_count; i++) {
            printf("%c %s\n", i == selected ? '>' : ' ', answers[i] ? answers[i] : "");
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        if (selected != last_spoken && answers[selected] && answers[selected][0]) {
            menu_audio_speak(answers[selected]);
            last_spoken = selected;
        }

        {
            int key = read_key();
            if (key == KEY_UP) {
                selected = menu_next_index(selected, -1, answer_count);
            } else if (key == KEY_DOWN) {
                selected = menu_next_index(selected, 1, answer_count);
            } else if (key == KEY_ENTER) {
                menu_audio_stop();
                return selected;
            } else if (key == KEY_ESC) {
                menu_audio_stop();
                return -1;
            }
        }
    }
}

static int trivia_show_question_result(const char *result_text) {
    char speech_error[128] = {0};

    printf("\033[H\033[J--- Trivia Result ---\n\n%s\n\n%s\n",
           result_text ? result_text : "",
           "[Enter: Next Trivia | Esc: Back]");
    fflush(stdout);

    menu_audio_stop();
    if (result_text && result_text[0] && speech_engine_is_enabled()) {
        speech_engine_speak_text_buffered(result_text, speech_error, sizeof(speech_error));
    }

    while (1) {
        int key = read_key();
        if (key == KEY_ENTER) {
            return 1;
        }
        if (key == KEY_ESC) {
            return 0;
        }
    }
}

static int trivia_run_question(cJSON *item, int question_number, int total_questions, char *summary, size_t summary_size) {
    cJSON *question_item = cJSON_GetObjectItemCaseSensitive(item, "question");
    cJSON *correct_item = cJSON_GetObjectItemCaseSensitive(item, "correct_answer");
    cJSON *incorrect_item = cJSON_GetObjectItemCaseSensitive(item, "incorrect_answers");
    char *question = trivia_decode_text(cJSON_IsString(question_item) ? question_item->valuestring : "");
    char *correct = trivia_decode_text(cJSON_IsString(correct_item) ? correct_item->valuestring : "");
    char *answers[8] = {0};
    int answer_count = 0;
    int i;
    int is_correct = 0;
    int selected = -1;
    char result_text[2048];

    if (!question || !correct) {
        free(question);
        free(correct);
        return 0;
    }

    answers[answer_count++] = strdup(correct);
    if (cJSON_IsArray(incorrect_item)) {
        for (i = 0; i < cJSON_GetArraySize(incorrect_item) && answer_count < 8; i++) {
            cJSON *wrong = cJSON_GetArrayItem(incorrect_item, i);
            answers[answer_count++] = trivia_decode_text(cJSON_IsString(wrong) ? wrong->valuestring : "");
        }
    }

    trivia_shuffle_strings(answers, answer_count);
    if (trivia_play_question(question, question_number, total_questions) == KEY_ESC) {
        snprintf(summary, summary_size, "Trivia session canceled on question %d.\n", question_number);
        free(question);
        free(correct);
        for (i = 0; i < answer_count; i++) {
            free(answers[i]);
        }
        return -1;
    }
    selected = trivia_choose_answer(question, answers, answer_count, question_number, total_questions);
    if (selected < 0) {
        snprintf(summary, summary_size, "Trivia session canceled on question %d.\n", question_number);
        is_correct = -1;
    } else {
        is_correct = strcmp(answers[selected] ? answers[selected] : "", correct) == 0;
        snprintf(summary, summary_size,
                 "Question %d\n%s\n\nYour answer: %s\nCorrect answer: %s\nResult: %s\n\n",
                 question_number,
                 question,
                 answers[selected] ? answers[selected] : "",
                 correct,
                 is_correct ? "Correct" : "Incorrect");
        snprintf(result_text, sizeof(result_text),
                 "%s\n\nYour answer: %s\nCorrect answer: %s",
                 is_correct ? "Correct" : "Incorrect",
                 answers[selected] ? answers[selected] : "",
                 correct);
        if (!trivia_show_question_result(result_text)) {
            is_correct = -1;
        }
    }

    free(question);
    free(correct);
    for (i = 0; i < answer_count; i++) {
        free(answers[i]);
    }

    return is_correct;
}

void trivia_run_quiz(void) {
    char url[1024];
    char error[256] = {0};
    char *response;
    cJSON *json;
    cJSON *results;
    cJSON *response_code;
    char report[16384];
    int total_questions;
    int correct_count = 0;
    int i;
    int canceled = 0;
    static int random_seeded = 0;

    if (!random_seeded) {
        srand((unsigned int)time(NULL));
        random_seeded = 1;
    }

    snprintf(url, sizeof(url), "https://opentdb.com/api.php?amount=%s", trivia_get_value("amount"));
    if (trivia_get_value("category")[0] != '\0') {
        strncat(url, "&category=", sizeof(url) - strlen(url) - 1);
        strncat(url, trivia_get_value("category"), sizeof(url) - strlen(url) - 1);
    }
    if (trivia_get_value("difficulty")[0] != '\0') {
        strncat(url, "&difficulty=", sizeof(url) - strlen(url) - 1);
        strncat(url, trivia_get_value("difficulty"), sizeof(url) - strlen(url) - 1);
    }
    if (trivia_get_value("type")[0] != '\0') {
        strncat(url, "&type=", sizeof(url) - strlen(url) - 1);
        strncat(url, trivia_get_value("type"), sizeof(url) - strlen(url) - 1);
    }
    if (trivia_get_value("encoding")[0] != '\0') {
        strncat(url, "&encode=", sizeof(url) - strlen(url) - 1);
        strncat(url, trivia_get_value("encoding"), sizeof(url) - strlen(url) - 1);
    }

    response = fetch_text_with_progress_ui("Trivia", url, "trivia questions", error, sizeof(error));
    if (!response) {
        trivia_show_message("Trivia", error[0] ? error : "Failed to fetch trivia questions.");
        return;
    }

    json = cJSON_Parse(response);
    free(response);
    if (!json) {
        trivia_show_message("Trivia", "Error parsing trivia response.");
        return;
    }

    response_code = cJSON_GetObjectItemCaseSensitive(json, "response_code");
    results = cJSON_GetObjectItemCaseSensitive(json, "results");
    if (!cJSON_IsNumber(response_code) || response_code->valueint != 0 || !cJSON_IsArray(results)) {
        cJSON_Delete(json);
        trivia_show_message("Trivia", "No trivia questions were returned for the selected settings.");
        return;
    }

    total_questions = cJSON_GetArraySize(results);
    if (total_questions <= 0) {
        cJSON_Delete(json);
        trivia_show_message("Trivia", "No trivia questions were returned for the selected settings.");
        return;
    }

    report[0] = '\0';
    for (i = 0; i < total_questions; i++) {
        char summary[2048];
        int result = trivia_run_question(cJSON_GetArrayItem(results, i), i + 1, total_questions, summary, sizeof(summary));
        strncat(report, summary, sizeof(report) - strlen(report) - 1);
        if (result > 0) {
            correct_count++;
        } else if (result < 0) {
            canceled = 1;
            break;
        }
    }

    {
        char header[256];
        snprintf(header, sizeof(header),
                 "Trivia Results\n\nScore: %d out of %d%s\n\n",
                 correct_count,
                 total_questions,
                 canceled ? " (session canceled early)" : "");
        memmove(report + strlen(header), report, strlen(report) + 1);
        memcpy(report, header, strlen(header));
    }

    cJSON_Delete(json);
    content_ui_show_spoken_text("Trivia Results", "Trivia Results", report);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "typing_tutor.h"
#include "utils.h"
#include "cJSON.h"

typedef struct {
    char text[256];
    int time_limit;
} TypingExample;

typedef struct {
    int id;
    char name[128];
    TypingExample *examples;
    int example_count;
} TypingLevel;

static TypingLevel *levels = NULL;
static int level_count = 0;

static void load_typing_levels() {
    FILE *f = fopen("typing_tutor.json", "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json) return;

    cJSON *levels_array = cJSON_GetObjectItemCaseSensitive(json, "levels");
    level_count = cJSON_GetArraySize(levels_array);
    levels = (TypingLevel*)malloc(sizeof(TypingLevel) * level_count);

    for (int i = 0; i < level_count; i++) {
        cJSON *level_obj = cJSON_GetArrayItem(levels_array, i);
        levels[i].id = cJSON_GetObjectItemCaseSensitive(level_obj, "id")->valueint;
        strncpy(levels[i].name, cJSON_GetObjectItemCaseSensitive(level_obj, "name")->valuestring, 127);
        
        cJSON *examples_array = cJSON_GetObjectItemCaseSensitive(level_obj, "examples");
        levels[i].example_count = cJSON_GetArraySize(examples_array);
        levels[i].examples = (TypingExample*)malloc(sizeof(TypingExample) * levels[i].example_count);
        
        for (int j = 0; j < levels[i].example_count; j++) {
            cJSON *example_obj = cJSON_GetArrayItem(examples_array, j);
            strncpy(levels[i].examples[j].text, cJSON_GetObjectItemCaseSensitive(example_obj, "text")->valuestring, 255);
            levels[i].examples[j].time_limit = cJSON_GetObjectItemCaseSensitive(example_obj, "time_limit")->valueint;
        }
    }
    cJSON_Delete(json);
}

// static void free_typing_levels() {
//     for (int i = 0; i < level_count; i++) {
//         free(levels[i].examples);
//     }
//     free(levels);
//     levels = NULL;
//     level_count = 0;
// }

double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

void play_level(TypingLevel *level) {
    int total_chars = 0;
    int correct_chars = 0;
    double total_time = 0;

    for (int i = 0; i < level->example_count; i++) {
        TypingExample *ex = &level->examples[i];
        char user_input[256] = {0};
        int input_len = 0;
        int target_len = (int)strlen(ex->text);

        printf("\033[H\033[J--- Typing Tutor: %s ---\n", level->name);
        printf("Example %d/%d\n", i + 1, level->example_count);
        printf("Time Limit: %d seconds\n", ex->time_limit);
        printf("--------------------------------------------\n");
        printf("TYPE THIS: %s\n", ex->text);
        printf("YOUR TYPE: ");
        fflush(stdout);

        double start_time = get_time_in_seconds();
        int timed_out = 0;

        while (input_len < target_len) {
            double current_time = get_time_in_seconds();
            if (current_time - start_time > ex->time_limit) {
                timed_out = 1;
                break;
            }

            // Using read_key but we need to check if a key is available without blocking for long
            // Actually our read_key is blocking. For a real game we might need non-blocking.
            // But let's stick to the current architecture.
            int key = read_key();
            if (key == KEY_ESC) return;
            if (key == KEY_BACKSPACE) {
                if (input_len > 0) {
                    input_len--;
                    user_input[input_len] = '\0';
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (key > 0 && key < 1000) {
                user_input[input_len++] = (char)key;
                user_input[input_len] = '\0';
                printf("%c", (char)key);
                fflush(stdout);
            }
        }

        double end_time = get_time_in_seconds();
        double time_taken = end_time - start_time;
        if (timed_out) time_taken = ex->time_limit;

        // Calculate accuracy for this example
        int ex_correct = 0;
        for (int j = 0; j < input_len && j < target_len; j++) {
            if (user_input[j] == ex->text[j]) ex_correct++;
        }

        total_chars += target_len;
        correct_chars += ex_correct;
        total_time += time_taken;

        printf("\n\n--- Result ---\n");
        if (timed_out) printf("TIME OUT!\n");
        printf("Accuracy: %.2f%%\n", (double)ex_correct / target_len * 100.0);
        printf("Speed: %.2f WPM\n", (ex_correct / 5.0) / (time_taken / 60.0));
        printf("\nPress any key for next example...");
        fflush(stdout);
        read_key();
    }

    // Level Summary
    printf("\033[H\033[J--- Level Summary: %s ---\n", level->name);
    printf("Total Characters: %d\n", total_chars);
    printf("Correct Characters: %d\n", correct_chars);
    printf("Total Time: %.2f seconds\n", total_time);
    printf("Overall Accuracy: %.2f%%\n", (double)correct_chars / total_chars * 100.0);
    printf("Average Speed: %.2f WPM\n", (correct_chars / 5.0) / (total_time / 60.0));
    printf("\nPress any key to go back...");
    fflush(stdout);
    read_key();
}

void handle_typing_tutor() {
    if (levels == NULL) {
        load_typing_levels();
    }
    if (level_count == 0) {
        printf("No typing levels found. Press any key...");
        fflush(stdout); read_key();
        return;
    }

    int sel = 0;
    while (1) {
        printf("\033[H\033[J--- Typing Tutor ---\n");
        printf("Select Level:\n");
        for (int i = 0; i < level_count; i++) {
            if (i == sel) printf("> %s\n", levels[i].name);
            else printf("  %s\n", levels[i].name);
        }
        printf("\n[Arrows: Navigate | Enter: Start | Esc: Back]\n");
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP && sel > 0) sel--;
        else if (key == KEY_DOWN && sel < level_count - 1) sel++;
        else if (key == KEY_ENTER) {
            play_level(&levels[sel]);
        } else if (key == KEY_ESC) {
            break;
        }
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "cJSON.h"
#include "file_manager.h"
#include "menu.h"
#include "speech_engine.h"
#include "typing_tutor.h"
#include "utils.h"

#define TYPING_LEVELS_FILE "typing_tutor.json"
#define TYPING_PROGRESS_FILE "typing_progress.json"
#define TYPING_LEADERBOARD_FILE "typing_leaderboard_mock.json"
#define TYPING_SHARE_FILE USER_SPACE "/typing_share_latest.txt"
#define TYPING_MAX_LEVELS 32
#define TYPING_MAX_EXAMPLES 32
#define TYPING_MAX_BADGE_MESSAGE 256

typedef struct {
    char text[256];
    int time_limit;
} TypingExample;

typedef struct {
    int id;
    char name[128];
    char focus[128];
    TypingExample examples[TYPING_MAX_EXAMPLES];
    int example_count;
} TypingLevel;

typedef struct {
    int completed;
    int canceled;
    int level_id;
    char level_name[128];
    double wpm;
    double accuracy;
    double elapsed_seconds;
    int correct_chars;
    int total_target_chars;
    int typed_chars;
    int prompt_replays;
    char timestamp[64];
} TypingSessionResult;

typedef struct {
    const char *period_key;
    const char *title_key;
    const char *title_fallback;
} LeaderboardPeriod;

typedef struct {
    const char *key;
    const char *title_key;
    const char *title_fallback;
    const char *description_key;
    const char *description_fallback;
} TypingBadgeDefinition;

static TypingLevel levels[TYPING_MAX_LEVELS];
static int level_count = 0;

static const LeaderboardPeriod leaderboard_periods[] = {
    {"daily", "ui_typing_daily_leaderboard", "Daily Leaderboard"},
    {"weekly", "ui_typing_weekly_leaderboard", "Weekly Leaderboard"},
    {"monthly", "ui_typing_monthly_leaderboard", "Monthly Leaderboard"}
};

static const TypingBadgeDefinition badge_definitions[] = {
    {"first_finish", "ui_badge_first_finish_title", "First Finish", "ui_badge_first_finish_desc", "Completed the first typing session."},
    {"accuracy_90", "ui_badge_accuracy_90_title", "Steady Hands", "ui_badge_accuracy_90_desc", "Reached at least 90% accuracy."},
    {"speed_20", "ui_badge_speed_20_title", "Rising Typist", "ui_badge_speed_20_desc", "Reached 20 WPM in a session."},
    {"speed_35", "ui_badge_speed_35_title", "Trailblazer", "ui_badge_speed_35_desc", "Reached 35 WPM in a session."},
    {"streak_3", "ui_badge_streak_3_title", "Consistency Star", "ui_badge_streak_3_desc", "Completed three strong sessions in a row."},
    {"sessions_10", "ui_badge_sessions_10_title", "Typing Champion", "ui_badge_sessions_10_desc", "Completed ten practice sessions."}
};

static const TypingBadgeDefinition *find_badge_definition(const char *badge_key) {
    size_t i;

    for (i = 0; i < sizeof(badge_definitions) / sizeof(badge_definitions[0]); i++) {
        if (strcmp(badge_definitions[i].key, badge_key) == 0) {
            return &badge_definitions[i];
        }
    }

    return NULL;
}

static void typing_speak(const char *text) {
    char error[128];

    if (!text || !text[0] || !speech_engine_is_enabled() || !speech_engine_is_available()) {
        return;
    }

    speech_engine_speak_text(text, error, sizeof(error));
}

static double get_time_in_seconds(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void get_timestamp_now(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm *tm_info;

    if (!buffer || buffer_size == 0) {
        return;
    }

    now = time(NULL);
    tm_info = localtime(&now);
    if (!tm_info) {
        snprintf(buffer, buffer_size, "Unknown");
        return;
    }

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static char *read_text_file(const char *path) {
    FILE *file;
    long len;
    char *data;

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    len = ftell(file);
    fseek(file, 0, SEEK_SET);

    data = (char *)malloc((size_t)len + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }

    fread(data, 1, (size_t)len, file);
    data[len] = '\0';
    fclose(file);
    return data;
}

static int write_json_file(const char *path, cJSON *json) {
    FILE *file;
    char *data;

    if (!path || !json) {
        return 0;
    }

    data = cJSON_Print(json);
    if (!data) {
        return 0;
    }

    file = fopen(path, "wb");
    if (!file) {
        free(data);
        return 0;
    }

    fputs(data, file);
    fclose(file);
    free(data);
    return 1;
}

static cJSON *load_progress_json(void) {
    char *data = read_text_file(TYPING_PROGRESS_FILE);
    cJSON *json;

    if (!data) {
        json = cJSON_CreateObject();
        cJSON_AddItemToObject(json, "profile", cJSON_CreateObject());
        cJSON_AddItemToObject(json, "sessions", cJSON_CreateArray());
        cJSON_AddItemToObject(json, "badges", cJSON_CreateArray());
        return json;
    }

    json = cJSON_Parse(data);
    free(data);
    if (!json) {
        json = cJSON_CreateObject();
    }

    if (!cJSON_GetObjectItemCaseSensitive(json, "profile")) {
        cJSON_AddItemToObject(json, "profile", cJSON_CreateObject());
    }
    if (!cJSON_GetObjectItemCaseSensitive(json, "sessions")) {
        cJSON_AddItemToObject(json, "sessions", cJSON_CreateArray());
    }
    if (!cJSON_GetObjectItemCaseSensitive(json, "badges")) {
        cJSON_AddItemToObject(json, "badges", cJSON_CreateArray());
    }

    return json;
}

static cJSON *ensure_object(cJSON *parent, const char *key) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);

    if (!cJSON_IsObject(item)) {
        if (item) {
            cJSON_DeleteItemFromObject(parent, key);
        }
        item = cJSON_CreateObject();
        cJSON_AddItemToObject(parent, key, item);
    }

    return item;
}

static cJSON *ensure_array(cJSON *parent, const char *key) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);

    if (!cJSON_IsArray(item)) {
        if (item) {
            cJSON_DeleteItemFromObject(parent, key);
        }
        item = cJSON_CreateArray();
        cJSON_AddItemToObject(parent, key, item);
    }

    return item;
}

static void profile_set_number(cJSON *profile, const char *key, double value) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(profile, key);

    if (item) {
        cJSON_SetNumberValue(item, value);
    } else {
        cJSON_AddNumberToObject(profile, key, value);
    }
}

static double profile_get_number(cJSON *profile, const char *key, double fallback) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(profile, key);

    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return fallback;
}

static int badge_already_earned(cJSON *badges, const char *badge_key) {
    int i;

    for (i = 0; i < cJSON_GetArraySize(badges); i++) {
        cJSON *badge = cJSON_GetArrayItem(badges, i);
        cJSON *key = cJSON_GetObjectItemCaseSensitive(badge, "key");
        if (cJSON_IsString(key) && strcmp(key->valuestring, badge_key) == 0) {
            return 1;
        }
    }

    return 0;
}

static void append_badge_message(char *buffer, size_t buffer_size, const char *title) {
    if (!buffer || !title || buffer_size == 0) {
        return;
    }

    if (buffer[0]) {
        strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
    }
    strncat(buffer, title, buffer_size - strlen(buffer) - 1);
}

static void award_badge_if_needed(cJSON *badges,
                                  const char *badge_key,
                                  char *badge_message,
                                  size_t badge_message_size) {
    const TypingBadgeDefinition *definition = find_badge_definition(badge_key);

    if (definition) {
        cJSON *badge;
        char timestamp[64];

        if (badge_already_earned(badges, badge_key)) {
            return;
        }

        get_timestamp_now(timestamp, sizeof(timestamp));
        badge = cJSON_CreateObject();
        cJSON_AddStringToObject(badge, "key", definition->key);
        cJSON_AddStringToObject(badge, "title", definition->title_fallback);
        cJSON_AddStringToObject(badge, "description", definition->description_fallback);
        cJSON_AddStringToObject(badge, "earned_at", timestamp);
        cJSON_AddItemToArray(badges, badge);
        append_badge_message(badge_message,
                             badge_message_size,
                             menu_translate(definition->title_key, definition->title_fallback));
    }
}

static void save_typing_session(const TypingSessionResult *result,
                                char *badge_message,
                                size_t badge_message_size) {
    cJSON *root;
    cJSON *profile;
    cJSON *sessions;
    cJSON *badges;
    cJSON *session;
    int sessions_completed;
    int current_streak;
    int best_streak;
    double best_wpm;
    double best_accuracy;

    if (!result || !result->completed) {
        return;
    }

    root = load_progress_json();
    profile = ensure_object(root, "profile");
    sessions = ensure_array(root, "sessions");
    badges = ensure_array(root, "badges");

    session = cJSON_CreateObject();
    cJSON_AddStringToObject(session, "timestamp", result->timestamp);
    cJSON_AddNumberToObject(session, "level_id", result->level_id);
    cJSON_AddStringToObject(session, "level_name", result->level_name);
    cJSON_AddNumberToObject(session, "wpm", result->wpm);
    cJSON_AddNumberToObject(session, "accuracy", result->accuracy);
    cJSON_AddNumberToObject(session, "elapsed_seconds", result->elapsed_seconds);
    cJSON_AddNumberToObject(session, "correct_chars", result->correct_chars);
    cJSON_AddNumberToObject(session, "target_chars", result->total_target_chars);
    cJSON_AddNumberToObject(session, "typed_chars", result->typed_chars);
    cJSON_AddNumberToObject(session, "prompt_replays", result->prompt_replays);
    cJSON_AddItemToArray(sessions, session);

    while (cJSON_GetArraySize(sessions) > 100) {
        cJSON_DeleteItemFromArray(sessions, 0);
    }

    sessions_completed = (int)profile_get_number(profile, "sessions_completed", 0) + 1;
    current_streak = (int)profile_get_number(profile, "current_streak", 0);
    best_streak = (int)profile_get_number(profile, "best_streak", 0);
    best_wpm = profile_get_number(profile, "best_wpm", 0.0);
    best_accuracy = profile_get_number(profile, "best_accuracy", 0.0);

    if (result->accuracy >= 85.0) {
        current_streak++;
    } else {
        current_streak = 0;
    }
    if (current_streak > best_streak) {
        best_streak = current_streak;
    }
    if (result->wpm > best_wpm) {
        best_wpm = result->wpm;
    }
    if (result->accuracy > best_accuracy) {
        best_accuracy = result->accuracy;
    }

    profile_set_number(profile, "sessions_completed", sessions_completed);
    profile_set_number(profile, "best_wpm", best_wpm);
    profile_set_number(profile, "best_accuracy", best_accuracy);
    profile_set_number(profile, "last_wpm", result->wpm);
    profile_set_number(profile, "last_accuracy", result->accuracy);
    profile_set_number(profile, "last_elapsed_seconds", result->elapsed_seconds);
    profile_set_number(profile, "last_correct_chars", result->correct_chars);
    profile_set_number(profile, "last_target_chars", result->total_target_chars);
    profile_set_number(profile, "prompt_replays", result->prompt_replays);
    profile_set_number(profile, "current_streak", current_streak);
    profile_set_number(profile, "best_streak", best_streak);
    cJSON_ReplaceItemInObject(profile, "last_level_name", cJSON_CreateString(result->level_name));
    cJSON_ReplaceItemInObject(profile, "last_timestamp", cJSON_CreateString(result->timestamp));

    if (sessions_completed >= 1) {
        award_badge_if_needed(badges, "first_finish", badge_message, badge_message_size);
    }
    if (result->accuracy >= 90.0) {
        award_badge_if_needed(badges, "accuracy_90", badge_message, badge_message_size);
    }
    if (result->wpm >= 20.0) {
        award_badge_if_needed(badges, "speed_20", badge_message, badge_message_size);
    }
    if (result->wpm >= 35.0) {
        award_badge_if_needed(badges, "speed_35", badge_message, badge_message_size);
    }
    if (best_streak >= 3) {
        award_badge_if_needed(badges, "streak_3", badge_message, badge_message_size);
    }
    if (sessions_completed >= 10) {
        award_badge_if_needed(badges, "sessions_10", badge_message, badge_message_size);
    }

    write_json_file(TYPING_PROGRESS_FILE, root);
    cJSON_Delete(root);
}

static int compute_accuracy(const char *target, const char *typed) {
    size_t i;
    size_t correct = 0;
    size_t max_len = strlen(target);
    size_t typed_len = strlen(typed);

    if (typed_len > max_len) {
        max_len = typed_len;
    }
    if (max_len == 0) {
        return 100;
    }

    for (i = 0; target[i] && typed[i]; i++) {
        if (target[i] == typed[i]) {
            correct++;
        }
    }

    return (int)((double)correct * 100.0 / (double)max_len);
}

static int count_correct_chars(const char *target, const char *typed) {
    int i;
    int correct = 0;

    for (i = 0; target[i] && typed[i]; i++) {
        if (target[i] == typed[i]) {
            correct++;
        }
    }

    return correct;
}

static void render_practice_screen(const TypingLevel *level,
                                   const TypingExample *example,
                                   int example_index,
                                   char *typed,
                                   double time_left,
                                   int replays) {
    printf("\033[H\033[J--- %s ---\n", menu_translate("learn_typing", "Typing"));
    printf("%s: %s\n", menu_translate("ui_typing_level", "Level"), level->name);
    printf("%s: %d/%d\n", menu_translate("ui_typing_example", "Prompt"), example_index + 1, level->example_count);
    printf("%s: %s\n", menu_translate("ui_typing_focus", "Focus"), level->focus[0] ? level->focus : level->name);
    printf("%s: %.1f %s\n\n",
           menu_translate("ui_typing_time_left", "Time Left"),
           time_left > 0.0 ? time_left : 0.0,
           menu_translate("ui_typing_seconds", "seconds"));
    printf("%s\n%s\n\n", menu_translate("ui_typing_target_text", "Listen and type this:"), example->text);
    printf("%s\n%s\n\n", menu_translate("ui_typing_your_input", "Your typing:"), typed);
    printf("%s: %d\n", menu_translate("ui_typing_replays", "Prompt repeats"), replays);
    printf("%s\n", menu_translate("ui_typing_practice_help", "[Type directly | Enter: Submit | Backspace: Edit | Tab: Repeat prompt | Esc: Cancel]"));
    fflush(stdout);
}

static int run_single_example(const TypingLevel *level,
                              const TypingExample *example,
                              int example_index,
                              TypingSessionResult *result,
                              int *session_correct_chars,
                              int *session_target_chars,
                              int *session_typed_chars) {
    char typed[256] = {0};
    int typed_len = 0;
    int prompt_replays = 0;
    double start_time;
    double end_time;

    typing_speak(example->text);
    start_time = get_time_in_seconds();

    while (1) {
        int key;
        double now = get_time_in_seconds();
        double time_left = (double)example->time_limit - (now - start_time);

        render_practice_screen(level, example, example_index, typed, time_left, prompt_replays);

        if (time_left <= 0.0) {
            break;
        }

        key = read_key_timeout(150);
        if (key == 0) {
            continue;
        }
        if (key == KEY_ESC) {
            result->canceled = 1;
            return 0;
        }
        if (key == KEY_TAB) {
            prompt_replays++;
            result->prompt_replays++;
            typing_speak(example->text);
            continue;
        }
        if (key == KEY_ENTER) {
            break;
        }
        if (key == KEY_BACKSPACE) {
            if (typed_len > 0) {
                typed[--typed_len] = '\0';
            }
            continue;
        }
        if (key > 0 && key < 1000 && typed_len < (int)sizeof(typed) - 1) {
            typed[typed_len++] = (char)key;
            typed[typed_len] = '\0';
        }
    }

    end_time = get_time_in_seconds();
    if (session_correct_chars) {
        *session_correct_chars += count_correct_chars(example->text, typed);
    }
    if (session_target_chars) {
        *session_target_chars += (int)strlen(example->text);
    }
    if (session_typed_chars) {
        *session_typed_chars += typed_len;
    }

    printf("\033[H\033[J--- %s ---\n", menu_translate("ui_typing_prompt_result", "Prompt Result"));
    printf("%s: %d%%\n",
           menu_translate("ui_typing_accuracy", "Accuracy"),
           compute_accuracy(example->text, typed));
    printf("%s: %s\n", menu_translate("ui_typing_expected", "Expected"), example->text);
    printf("%s: %s\n\n", menu_translate("ui_typing_entered", "Entered"), typed);
    printf("%s", menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);
    read_key();

    result->elapsed_seconds += end_time - start_time;
    return 1;
}

static void summarize_session(TypingSessionResult *result, const char *badge_message) {
    char speech_summary[256];

    if (!result || !result->completed) {
        return;
    }

    printf("\033[H\033[J--- %s ---\n", menu_translate("ui_typing_session_complete", "Typing Session Complete"));
    printf("%s: %s\n", menu_translate("ui_typing_level", "Level"), result->level_name);
    printf("%s: %.2f %s\n",
           menu_translate("ui_typing_speed", "Speed"),
           result->wpm,
           menu_translate("ui_typing_wpm", "WPM"));
    printf("%s: %.2f%%\n", menu_translate("ui_typing_accuracy", "Accuracy"), result->accuracy);
    printf("%s: %.1f %s\n",
           menu_translate("ui_typing_elapsed", "Time"),
           result->elapsed_seconds,
           menu_translate("ui_typing_seconds", "seconds"));
    printf("%s: %d\n", menu_translate("ui_typing_prompt_replays", "Prompt replays"), result->prompt_replays);
    if (badge_message && badge_message[0]) {
        printf("\n%s: %s\n", menu_translate("ui_typing_badges_earned", "New badges"), badge_message);
    } else {
        printf("\n%s\n", menu_translate("ui_typing_keep_going", "Keep going. Every session makes you stronger."));
    }
    printf("\n%s", menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
    fflush(stdout);

    snprintf(speech_summary, sizeof(speech_summary),
             "%s %.0f %s, %.0f %s. %s",
             menu_translate("ui_typing_speed", "Speed"),
             result->wpm,
             menu_translate("ui_typing_wpm", "WPM"),
             result->accuracy,
             menu_translate("ui_typing_percent_accuracy", "percent accuracy"),
             (badge_message && badge_message[0]) ? badge_message : menu_translate("ui_typing_keep_practicing", "Keep practicing"));
    typing_speak(speech_summary);
    read_key();
}

static int run_level_session(const TypingLevel *level, TypingSessionResult *result) {
    int i;
    int session_correct_chars = 0;
    int session_target_chars = 0;
    int session_typed_chars = 0;

    if (!level || !result) {
        return 0;
    }

    memset(result, 0, sizeof(*result));
    result->level_id = level->id;
    snprintf(result->level_name, sizeof(result->level_name), "%s", level->name);
    get_timestamp_now(result->timestamp, sizeof(result->timestamp));

    for (i = 0; i < level->example_count; i++) {
        if (!run_single_example(level,
                                &level->examples[i],
                                i,
                                result,
                                &session_correct_chars,
                                &session_target_chars,
                                &session_typed_chars)) {
            return 0;
        }
    }

    result->completed = 1;
    result->correct_chars = session_correct_chars;
    result->total_target_chars = session_target_chars;
    result->typed_chars = session_typed_chars;
    result->accuracy = session_target_chars > 0
        ? ((double)session_correct_chars * 100.0 / (double)session_target_chars)
        : 0.0;
    result->wpm = result->elapsed_seconds > 0.0
        ? ((double)session_correct_chars / 5.0) / (result->elapsed_seconds / 60.0)
        : 0.0;
    return 1;
}

static void load_typing_levels(void) {
    char *data;
    cJSON *json;
    cJSON *levels_array;
    int i;

    if (level_count > 0) {
        return;
    }

    data = read_text_file(TYPING_LEVELS_FILE);
    if (!data) {
        return;
    }

    json = cJSON_Parse(data);
    free(data);
    if (!json) {
        return;
    }

    levels_array = cJSON_GetObjectItemCaseSensitive(json, "levels");
    if (!cJSON_IsArray(levels_array)) {
        cJSON_Delete(json);
        return;
    }

    for (i = 0; i < cJSON_GetArraySize(levels_array) && i < TYPING_MAX_LEVELS; i++) {
        cJSON *level_obj = cJSON_GetArrayItem(levels_array, i);
        cJSON *examples_array = cJSON_GetObjectItemCaseSensitive(level_obj, "examples");
        int j;

        memset(&levels[level_count], 0, sizeof(TypingLevel));
        levels[level_count].id = cJSON_GetObjectItemCaseSensitive(level_obj, "id")->valueint;
        snprintf(levels[level_count].name,
                 sizeof(levels[level_count].name),
                 "%s",
                 cJSON_GetObjectItemCaseSensitive(level_obj, "name")->valuestring);

        {
            cJSON *focus = cJSON_GetObjectItemCaseSensitive(level_obj, "focus");
            if (cJSON_IsString(focus)) {
                snprintf(levels[level_count].focus, sizeof(levels[level_count].focus), "%s", focus->valuestring);
            } else {
                snprintf(levels[level_count].focus, sizeof(levels[level_count].focus), "%s", levels[level_count].name);
            }
        }

        if (!cJSON_IsArray(examples_array)) {
            continue;
        }

        for (j = 0; j < cJSON_GetArraySize(examples_array) && j < TYPING_MAX_EXAMPLES; j++) {
            cJSON *example_obj = cJSON_GetArrayItem(examples_array, j);
            cJSON *text = cJSON_GetObjectItemCaseSensitive(example_obj, "text");
            cJSON *time_limit = cJSON_GetObjectItemCaseSensitive(example_obj, "time_limit");

            if (!cJSON_IsString(text) || !cJSON_IsNumber(time_limit)) {
                continue;
            }

            snprintf(levels[level_count].examples[j].text,
                     sizeof(levels[level_count].examples[j].text),
                     "%s",
                     text->valuestring);
            levels[level_count].examples[j].time_limit = time_limit->valueint;
            levels[level_count].example_count++;
        }

        if (levels[level_count].example_count > 0) {
            level_count++;
        }
    }

    cJSON_Delete(json);
}

static cJSON *load_leaderboard_json(void) {
    char *data = read_text_file(TYPING_LEADERBOARD_FILE);
    cJSON *json;

    if (!data) {
        return NULL;
    }

    json = cJSON_Parse(data);
    free(data);
    return json;
}

static TypingSessionResult load_latest_session(void) {
    TypingSessionResult result;
    cJSON *root = load_progress_json();
    cJSON *sessions = ensure_array(root, "sessions");
    cJSON *last;

    memset(&result, 0, sizeof(result));
    if (cJSON_GetArraySize(sessions) <= 0) {
        cJSON_Delete(root);
        return result;
    }

    last = cJSON_GetArrayItem(sessions, cJSON_GetArraySize(sessions) - 1);
    if (cJSON_IsObject(last)) {
        cJSON *level_name = cJSON_GetObjectItemCaseSensitive(last, "level_name");
        cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(last, "timestamp");
        result.completed = 1;
        result.wpm = cJSON_GetObjectItemCaseSensitive(last, "wpm")->valuedouble;
        result.accuracy = cJSON_GetObjectItemCaseSensitive(last, "accuracy")->valuedouble;
        result.elapsed_seconds = cJSON_GetObjectItemCaseSensitive(last, "elapsed_seconds")->valuedouble;
        if (cJSON_IsString(level_name)) {
            snprintf(result.level_name, sizeof(result.level_name), "%s", level_name->valuestring);
        }
        if (cJSON_IsString(timestamp)) {
            snprintf(result.timestamp, sizeof(result.timestamp), "%s", timestamp->valuestring);
        }
    }

    cJSON_Delete(root);
    return result;
}

static void show_progress_summary(void) {
    cJSON *root = load_progress_json();
    cJSON *profile = ensure_object(root, "profile");

    printf("\033[H\033[J--- %s ---\n", menu_translate("ui_typing_my_progress", "My Progress"));
    printf("%s: %.0f\n", menu_translate("ui_typing_sessions_completed", "Sessions completed"),
           profile_get_number(profile, "sessions_completed", 0));
    printf("%s: %.2f %s\n", menu_translate("ui_typing_best_speed", "Best speed"),
           profile_get_number(profile, "best_wpm", 0.0),
           menu_translate("ui_typing_wpm", "WPM"));
    printf("%s: %.2f%%\n", menu_translate("ui_typing_best_accuracy", "Best accuracy"),
           profile_get_number(profile, "best_accuracy", 0.0));
    printf("%s: %.0f\n", menu_translate("ui_typing_current_streak", "Current streak"),
           profile_get_number(profile, "current_streak", 0.0));
    printf("%s: %.0f\n", menu_translate("ui_typing_best_streak", "Best streak"),
           profile_get_number(profile, "best_streak", 0.0));
    printf("%s: %s\n",
           menu_translate("ui_typing_last_level", "Last level"),
           cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(profile, "last_level_name")) ?
               cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(profile, "last_level_name")) :
               menu_translate("ui_not_set", "Not set"));
    printf("\n%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
    fflush(stdout);
    read_key();
    cJSON_Delete(root);
}

static void show_badges(void) {
    cJSON *root = load_progress_json();
    cJSON *badges = ensure_array(root, "badges");
    int i;

    printf("\033[H\033[J--- %s ---\n", menu_translate("ui_typing_my_badges", "My Badges"));
    if (cJSON_GetArraySize(badges) == 0) {
        printf("%s\n", menu_translate("ui_typing_no_badges_yet", "No badges yet. Finish a session to start collecting them."));
    } else {
        for (i = 0; i < cJSON_GetArraySize(badges); i++) {
            cJSON *badge = cJSON_GetArrayItem(badges, i);
            cJSON *key = cJSON_GetObjectItemCaseSensitive(badge, "key");
            const TypingBadgeDefinition *definition = cJSON_IsString(key)
                ? find_badge_definition(key->valuestring)
                : NULL;
            printf("> %s\n",
                   definition
                       ? menu_translate(definition->title_key, definition->title_fallback)
                       : cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(badge, "title")));
            printf("  %s\n",
                   definition
                       ? menu_translate(definition->description_key, definition->description_fallback)
                       : cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(badge, "description")));
        }
    }
    printf("\n%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
    fflush(stdout);
    read_key();
    cJSON_Delete(root);
}

static void show_leaderboard(const LeaderboardPeriod *period) {
    cJSON *root;
    cJSON *entries;
    TypingSessionResult last_result = load_latest_session();
    int i;
    int virtual_rank = 1;

    if (!period) {
        return;
    }

    root = load_leaderboard_json();
    if (!root) {
        printf("\033[H\033[J--- %s ---\n%s\n\n%s",
               menu_translate(period->title_key, period->title_fallback),
               menu_translate("ui_typing_leaderboard_unavailable", "Leaderboard data is unavailable."),
               menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
        fflush(stdout);
        read_key();
        return;
    }

    entries = cJSON_GetObjectItemCaseSensitive(root, period->period_key);
    printf("\033[H\033[J--- %s ---\n", menu_translate(period->title_key, period->title_fallback));
    if (!cJSON_IsArray(entries)) {
        printf("%s\n", menu_translate("ui_typing_leaderboard_unavailable", "Leaderboard data is unavailable."));
    } else {
        for (i = 0; i < cJSON_GetArraySize(entries); i++) {
            cJSON *entry = cJSON_GetArrayItem(entries, i);
            double wpm = cJSON_GetObjectItemCaseSensitive(entry, "wpm")->valuedouble;
            printf("%d. %s - %.1f %s - %.0f%%\n",
                   cJSON_GetObjectItemCaseSensitive(entry, "rank")->valueint,
                   cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(entry, "name")),
                   wpm,
                   menu_translate("ui_typing_wpm", "WPM"),
                   cJSON_GetObjectItemCaseSensitive(entry, "accuracy")->valuedouble);
            if (last_result.completed && wpm > last_result.wpm) {
                virtual_rank++;
            }
        }
    }

    printf("\n");
    if (last_result.completed) {
        printf("%s: %.1f %s / %.0f%%\n",
               menu_translate("ui_typing_last_session", "Your last session"),
               last_result.wpm,
               menu_translate("ui_typing_wpm", "WPM"),
               last_result.accuracy);
        printf("%s #%d\n",
               menu_translate("ui_typing_virtual_rank", "Estimated rank"),
               virtual_rank);
    } else {
        printf("%s\n", menu_translate("ui_typing_finish_session_for_rank", "Finish a typing session to compare yourself on the leaderboard."));
    }
    printf("\n%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
    fflush(stdout);
    read_key();
    cJSON_Delete(root);
}

static void share_latest_result(void) {
    TypingSessionResult last_result = load_latest_session();
    FILE *file;
    char message[512];

    mkdir(USER_SPACE, 0777);

    printf("\033[H\033[J--- %s ---\n", menu_translate("ui_typing_share_latest", "Share Latest Result"));
    if (!last_result.completed) {
        printf("%s\n\n%s",
               menu_translate("ui_typing_nothing_to_share", "No completed typing session is available to share yet."),
               menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
        fflush(stdout);
        read_key();
        return;
    }

    snprintf(message, sizeof(message),
             "%s\n%s: %s\n%s: %.2f %s\n%s: %.2f%%\n%s: %.1f %s\n%s: %s\n",
             menu_translate("ui_typing_share_heading", "Typing update"),
             menu_translate("ui_typing_level", "Level"),
             last_result.level_name,
             menu_translate("ui_typing_speed", "Speed"),
             last_result.wpm,
             menu_translate("ui_typing_wpm", "WPM"),
             menu_translate("ui_typing_accuracy", "Accuracy"),
             last_result.accuracy,
             menu_translate("ui_typing_elapsed", "Time"),
             last_result.elapsed_seconds,
             menu_translate("ui_typing_seconds", "seconds"),
             menu_translate("ui_typing_completed", "Completed"),
             last_result.timestamp);

    file = fopen(TYPING_SHARE_FILE, "wb");
    if (file) {
        fputs(message, file);
        fclose(file);
    }

    printf("%s\n%s\n\n%s",
           menu_translate("ui_typing_share_saved", "A shareable summary was saved to"),
           TYPING_SHARE_FILE,
           menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
    fflush(stdout);
    read_key();
}

void handle_typing_tutor(void) {
    int selected_index = 0;
    const char *options[] = {
        "ui_typing_start_practice",
        "ui_typing_daily_leaderboard",
        "ui_typing_weekly_leaderboard",
        "ui_typing_monthly_leaderboard",
        "ui_typing_my_badges",
        "ui_typing_my_progress",
        "ui_typing_share_latest",
        "ui_go_back"
    };
    const int option_count = 8;

    load_typing_levels();
    if (level_count == 0) {
        printf("%s\n", menu_translate("ui_typing_levels_not_found", "No typing levels found."));
        printf("%s", menu_translate("ui_press_any_key_to_go_back", "Press any key to go back..."));
        fflush(stdout);
        read_key();
        return;
    }

    while (1) {
        int key;
        int i;

        printf("\033[H\033[J--- %s ---\n", menu_translate("learn_typing", "Typing"));
        printf("%s\n\n", menu_translate("ui_typing_welcome", "Build speed, confidence, and consistency one session at a time."));
        for (i = 0; i < option_count; i++) {
            const char *label = menu_translate(options[i], options[i]);
            if (i == selected_index) {
                printf("> %s\n", label);
            } else {
                printf("  %s\n", label);
            }
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        key = read_key();
        if (key == KEY_UP) {
            selected_index = menu_next_index(selected_index, -1, option_count);
        } else if (key == KEY_DOWN) {
            selected_index = menu_next_index(selected_index, 1, option_count);
        } else if (key == KEY_ESC) {
            return;
        } else if (key == KEY_ENTER) {
            if (selected_index == 0) {
                int level_index = 0;
                while (1) {
                    int level_key;
                    TypingSessionResult result;
                    char badge_message[TYPING_MAX_BADGE_MESSAGE] = {0};

                    printf("\033[H\033[J--- %s ---\n", menu_translate("ui_select_level", "Select Level"));
                    for (i = 0; i < level_count; i++) {
                        if (i == level_index) {
                            printf("> %s\n", levels[i].name);
                        } else {
                            printf("  %s\n", levels[i].name);
                        }
                    }
                    printf("\n%s\n", menu_translate("ui_footer_start_back", "[Arrows: Navigate | Enter: Start | Esc: Back]"));
                    fflush(stdout);

                    level_key = read_key();
                    if (level_key == KEY_UP) {
                        level_index = menu_next_index(level_index, -1, level_count);
                    } else if (level_key == KEY_DOWN) {
                        level_index = menu_next_index(level_index, 1, level_count);
                    } else if (level_key == KEY_ESC) {
                        break;
                    } else if (level_key == KEY_ENTER) {
                        typing_speak(menu_translate("ui_typing_session_start_voice", "Typing session starting."));
                        if (run_level_session(&levels[level_index], &result) && result.completed) {
                            save_typing_session(&result, badge_message, sizeof(badge_message));
                            summarize_session(&result, badge_message);
                        }
                        break;
                    }
                }
            } else if (selected_index >= 1 && selected_index <= 3) {
                show_leaderboard(&leaderboard_periods[selected_index - 1]);
            } else if (selected_index == 4) {
                show_badges();
            } else if (selected_index == 5) {
                show_progress_summary();
            } else if (selected_index == 6) {
                share_latest_result();
            } else {
                return;
            }
        }
    }
}

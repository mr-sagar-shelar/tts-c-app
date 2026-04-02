#include "task_ui.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "menu_audio.h"
#include "menu.h"
#include "utils.h"

static void write_result_file(const char *result_path, const char *status, const char *payload) {
    FILE *file = fopen(result_path, "wb");
    if (!file) {
        return;
    }

    fprintf(file, "%s\n", status ? status : "ERROR");
    if (payload) {
        fputs(payload, file);
    }
    fclose(file);
}

char *run_text_task_with_progress_ui(const char *title,
                                     const char *label,
                                     const char *path,
                                     TaskTextLoader loader,
                                     char *error,
                                     size_t error_size) {
    char result_template[] = "/tmp/sai_task_XXXXXX";
    int result_fd;
    pid_t pid;
    int progress = 0;
    static const char spinner[] = "|/-\\";
    int spinner_index = 0;
    int status;
    FILE *file;
    long len;
    char *raw;
    char *newline;
    int last_announced_progress = -10;

    if (!loader || !path) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Invalid task request");
        }
        return NULL;
    }

    result_fd = mkstemp(result_template);
    if (result_fd < 0) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to create task result file");
        }
        return NULL;
    }
    close(result_fd);

    pid = fork();
    if (pid < 0) {
        unlink(result_template);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to start background task");
        }
        return NULL;
    }

    if (pid == 0) {
        char local_error[256] = {0};
        char *text = loader(path, local_error, sizeof(local_error));

        if (text) {
            write_result_file(result_template, "OK", text);
            free(text);
        } else {
            write_result_file(result_template, "ERROR", local_error[0] ? local_error : "Task failed");
        }
        _exit(0);
    }

    while (waitpid(pid, &status, WNOHANG) == 0) {
        char spin[2];

        if (progress < 95) {
            progress += 5;
        }
        spin[0] = spinner[spinner_index++ % 4];
        spin[1] = '\0';

        printf("\033[H\033[J");
        print_memory_widget_line();
        printf("--- %s ---\n", title ? title : "Working");
        printf("%s %s\n", spin, label ? label : "Processing");
        printf("%s: %s\n", menu_translate("ui_target", "Target"), path);
        printf("%s: %d%%\n\n", menu_translate("ui_progress", "Progress"), progress);
        printf("%s\n", menu_translate("ui_transfer_lock_message", "The current menu remains active until the transfer finishes."));
        fflush(stdout);
        if (progress >= last_announced_progress + 10 || progress == 100) {
            char speech[64];
            snprintf(speech, sizeof(speech), "%d percent", progress);
            menu_audio_speak(speech);
            last_announced_progress = progress;
        }
        read_key_timeout(150);
    }

    printf("\033[H\033[J");
    print_memory_widget_line();
    printf("--- %s ---\n", title ? title : "Working");
    printf("Processing complete.\n");
    printf("%s: %s\n", menu_translate("ui_target", "Target"), path);
    printf("%s: 100%%\n", menu_translate("ui_progress", "Progress"));
    fflush(stdout);

    file = fopen(result_template, "rb");
    unlink(result_template);
    if (!file) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to read task result");
        }
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    len = ftell(file);
    fseek(file, 0, SEEK_SET);
    raw = (char *)malloc((size_t)len + 1);
    if (!raw) {
        fclose(file);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to allocate task result");
        }
        return NULL;
    }

    fread(raw, 1, (size_t)len, file);
    raw[len] = '\0';
    fclose(file);

    newline = strchr(raw, '\n');
    if (!newline) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Invalid task result");
        }
        free(raw);
        return NULL;
    }

    *newline = '\0';
    if (strcmp(raw, "OK") == 0) {
        char *payload = strdup(newline + 1);
        free(raw);
        if (error && error_size > 0) {
            error[0] = '\0';
        }
        return payload;
    }

    if (error && error_size > 0) {
        snprintf(error, error_size, "%s", newline + 1);
    }
    free(raw);
    return NULL;
}

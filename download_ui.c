#include "download_ui.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "download_manager.h"
#include "app_logger.h"
#include "menu_audio.h"
#include "menu.h"
#include "utils.h"

static char *shell_quote(const char *text) {
    size_t i;
    size_t extra = 2;
    char *quoted;
    char *out;

    for (i = 0; text && text[i]; i++) {
        extra++;
        if (text[i] == '\'') {
            extra += 3;
        }
    }

    quoted = (char *)malloc(extra + 1);
    if (!quoted) {
        return NULL;
    }

    out = quoted;
    *out++ = '\'';
    for (i = 0; text && text[i]; i++) {
        if (text[i] == '\'') {
            memcpy(out, "'\\''", 4);
            out += 4;
        } else {
            *out++ = text[i];
        }
    }
    *out++ = '\'';
    *out = '\0';
    return quoted;
}

static void api_log_redacted_url(const char *input, char *output, size_t output_size) {
    const char *needle = "api-key=";
    const char *found;

    if (!output || output_size == 0) {
        return;
    }

    output[0] = '\0';
    if (!input) {
        return;
    }

    found = strstr(input, needle);
    if (!found) {
        snprintf(output, output_size, "%s", input);
        return;
    }

    {
        size_t prefix_len = (size_t)(found - input) + strlen(needle);
        const char *value_start = found + strlen(needle);
        const char *value_end = value_start;

        while (*value_end && *value_end != '&') {
            value_end++;
        }

        if (prefix_len >= output_size) {
            prefix_len = output_size - 1;
        }
        memcpy(output, input, prefix_len);
        output[prefix_len] = '\0';
        strncat(output, "***", output_size - strlen(output) - 1);
        strncat(output, value_end, output_size - strlen(output) - 1);
    }
}

static void api_log_fetch_result(const char *title,
                                 const char *url,
                                 const char *label,
                                 const char *response,
                                 const char *error) {
    char safe_url[2048];

    api_log_redacted_url(url, safe_url, sizeof(safe_url));
    app_log_message("api", "title=%s label=%s url=%s", title ? title : "", label ? label : "", safe_url);
    if (error && error[0]) {
        app_log_message("api", "result=error error=%s", error);
    } else if (response) {
        app_log_message("api", "result=success response=%s", response);
    }
}

static void api_log_error_file(const char *title, const char *url, const char *label, const char *error_path) {
    FILE *file;
    char buffer[2048];
    size_t len;

    if (!error_path || !error_path[0]) {
        return;
    }

    file = fopen(error_path, "rb");
    if (!file) {
        return;
    }

    len = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[len] = '\0';

    if (buffer[0]) {
        api_log_fetch_result(title, url, label, NULL, buffer);
    }
}

static int run_transfer_progress_ui(const char *title,
                                    const char *target_path,
                                    const char *initial_status,
                                    pid_t pid,
                                    int progress_fd,
                                    char *error,
                                    size_t error_size) {
    int progress = 0;
    static const char spinner[] = "|/-\\";
    int spinner_index = 0;
    char progress_text[512] = {0};
    size_t progress_len = 0;
    int status;
    int last_announced_progress = -10;

    if (progress_fd >= 0) {
        fcntl(progress_fd, F_SETFL, fcntl(progress_fd, F_GETFL, 0) | O_NONBLOCK);
    }

    while (waitpid(pid, &status, WNOHANG) == 0) {
        char buffer[256];
        ssize_t bytes_read;
        char spin[2];

        while (progress_fd >= 0 && (bytes_read = read(progress_fd, buffer, sizeof(buffer) - 1)) > 0) {
            const char *p;

            buffer[bytes_read] = '\0';
            if (progress_len + (size_t)bytes_read >= sizeof(progress_text)) {
                progress_len = 0;
                progress_text[0] = '\0';
            }
            memcpy(progress_text + progress_len, buffer, (size_t)bytes_read + 1);
            progress_len += (size_t)bytes_read;

            p = progress_text;
            while (*p) {
                if ((*p >= '0' && *p <= '9') || *p == '.') {
                    char *endptr;
                    double value = strtod(p, &endptr);
                    if (endptr > p && *endptr == '%' && value >= 0.0 && value <= 100.0) {
                        if ((int)value > progress) {
                            progress = (int)value;
                        }
                        p = endptr + 1;
                        continue;
                    }
                }
                p++;
            }
        }

        if (progress < 95) {
            progress++;
        }
        spin[0] = spinner[spinner_index++ % 4];
        spin[1] = '\0';

        printf("\033[H\033[J");
        print_memory_widget_line();
        printf("--- %s ---\n", title ? title : menu_translate("ui_transfer", "Transfer"));
        printf("%s %s\n", spin, initial_status ? initial_status : menu_translate("ui_transferring", "Transferring"));
        printf("%s: %s\n", menu_translate("ui_target", "Target"), target_path ? target_path : "(none)");
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

    if (progress_fd >= 0) {
        close(progress_fd);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "%s failed", title ? title : menu_translate("ui_transfer", "Transfer"));
        }
        return 0;
    }

    if (error && error_size > 0) {
        error[0] = '\0';
    }
    return 1;
}

int download_file_with_progress_ui(const char *title,
                                   const char *url,
                                   const char *output_path,
                                   const char *label,
                                   char *error,
                                   size_t error_size) {
    DownloadTask task;
    static const char spinner[] = "|/-\\";
    int spinner_index = 0;
    int last_announced_progress = -10;

    download_task_init(&task);
    if (!download_task_start(&task, url, output_path, label, error, error_size)) {
        return 0;
    }

    while (task.active) {
        char spin[2];

        download_task_poll(&task);
        spin[0] = spinner[spinner_index++ % 4];
        spin[1] = '\0';

        printf("\033[H\033[J");
        print_memory_widget_line();
        printf("--- %s ---\n", title ? title : "Download");
        printf("%s %s\n", spin, task.status);
        printf("%s: %s\n", menu_translate("ui_target", "Target"), output_path);
        printf("%s: %d%%\n\n", menu_translate("ui_progress", "Progress"), task.progress_percent);
        printf("%s\n", menu_translate("ui_transfer_lock_message", "The current menu remains active until the transfer finishes."));
        fflush(stdout);
        if (task.progress_percent >= last_announced_progress + 10 || task.progress_percent == 100) {
            char speech[64];
            snprintf(speech, sizeof(speech), "%d percent", task.progress_percent);
            menu_audio_speak(speech);
            last_announced_progress = task.progress_percent;
        }

        if (task.active) {
            read_key_timeout(150);
        }
    }

    download_task_poll(&task);
    if (!task.success) {
        snprintf(error, error_size, "%s", task.status[0] ? task.status : "Download failed");
        unlink(output_path);
    } else if (error && error_size > 0) {
        error[0] = '\0';
    }

    download_task_reset(&task);
    return task.success;
}

int upload_file_with_progress_ui(const char *title,
                                 const char *url,
                                 const char *input_path,
                                 const char *label,
                                 char *error,
                                 size_t error_size) {
    int pipefd[2];
    pid_t pid;
    char status[256];

    if (!url || !input_path) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Invalid upload request");
        }
        return 0;
    }

    if (pipe(pipefd) != 0) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to create upload progress pipe");
        }
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to start upload process");
        }
        return 0;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        char data_arg[PATH_MAX + 2];

        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        snprintf(data_arg, sizeof(data_arg), "@%s", input_path);
        execlp("curl", "curl", "--fail", "--progress-bar", "-X", "POST",
               "-H", "Content-Type: application/json",
               "--data-binary", data_arg, url, (char *)NULL);
        _exit(1);
    }

    close(pipefd[1]);
    snprintf(status, sizeof(status), "Uploading %s", label ? label : "data");
    return run_transfer_progress_ui(title, input_path, status, pid, pipefd[0], error, error_size);
}

char *fetch_text_with_progress_ui(const char *title,
                                  const char *url,
                                  const char *label,
                                  char *error,
                                  size_t error_size) {
    char tmp_template[] = "/tmp/sai_fetch_XXXXXX";
    int fd;
    pid_t pid;
    FILE *file;
    long len;
    char *data;
    char *quoted_url;
    char *quoted_path;
    char err_template[] = "/tmp/sai_fetch_err_XXXXXX";
    int err_fd;
    char *quoted_err_path;
    char command[PATH_MAX * 2];

    fd = mkstemp(tmp_template);
    if (fd < 0) {
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to create temporary fetch file");
        }
        api_log_fetch_result(title, url, label, NULL, error && error_size > 0 ? error : "Unable to create temporary fetch file");
        return NULL;
    }
    close(fd);
    err_fd = mkstemp(err_template);
    if (err_fd < 0) {
        unlink(tmp_template);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to create temporary fetch error file");
        }
        api_log_fetch_result(title, url, label, NULL, error && error_size > 0 ? error : "Unable to create temporary fetch error file");
        return NULL;
    }
    close(err_fd);

    quoted_url = shell_quote(url);
    quoted_path = shell_quote(tmp_template);
    quoted_err_path = shell_quote(err_template);
    if (!quoted_url || !quoted_path || !quoted_err_path) {
        free(quoted_url);
        free(quoted_path);
        free(quoted_err_path);
        unlink(tmp_template);
        unlink(err_template);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to prepare fetch command");
        }
        api_log_fetch_result(title, url, label, NULL, error && error_size > 0 ? error : "Unable to prepare fetch command");
        return NULL;
    }

    pid = fork();
    if (pid < 0) {
        free(quoted_url);
        free(quoted_path);
        free(quoted_err_path);
        unlink(tmp_template);
        unlink(err_template);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to start fetch process");
        }
        api_log_fetch_result(title, url, label, NULL, error && error_size > 0 ? error : "Unable to start fetch process");
        return NULL;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);

        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }

        snprintf(command, sizeof(command), "curl -s -L --fail %s -o %s 2>%s",
                 quoted_url, quoted_path, quoted_err_path);
        free(quoted_url);
        free(quoted_path);
        free(quoted_err_path);
        execl("/bin/sh", "sh", "-lc", command, (char *)NULL);
        _exit(1);
    }

    free(quoted_url);
    free(quoted_path);
    free(quoted_err_path);
    if (!run_transfer_progress_ui(title, tmp_template, label ? label : "Fetching data", pid, -1, error, error_size)) {
        api_log_error_file(title, url, label, err_template);
        api_log_fetch_result(title, url, label, NULL, error && error_size > 0 ? error : "Fetch transfer failed");
        unlink(tmp_template);
        unlink(err_template);
        return NULL;
    }

    file = fopen(tmp_template, "rb");
    if (!file) {
        unlink(tmp_template);
        unlink(err_template);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to open fetched data");
        }
        api_log_fetch_result(title, url, label, NULL, error && error_size > 0 ? error : "Unable to open fetched data");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    len = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = (char *)malloc((size_t)len + 1);
    if (!data) {
        fclose(file);
        unlink(tmp_template);
        unlink(err_template);
        if (error && error_size > 0) {
            snprintf(error, error_size, "Unable to allocate fetched data buffer");
        }
        api_log_fetch_result(title, url, label, NULL, error && error_size > 0 ? error : "Unable to allocate fetched data buffer");
        return NULL;
    }

    fread(data, 1, (size_t)len, file);
    data[len] = '\0';
    fclose(file);
    unlink(tmp_template);
    unlink(err_template);

    if (error && error_size > 0) {
        error[0] = '\0';
    }
    api_log_fetch_result(title, url, label, data, NULL);
    return data;
}

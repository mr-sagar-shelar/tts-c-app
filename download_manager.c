#include "download_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error && error_size > 0) {
        snprintf(error, error_size, "%s", message ? message : "Download error");
    }
}

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

void download_task_init(DownloadTask *task) {
    if (!task) {
        return;
    }

    memset(task, 0, sizeof(*task));
    task->stderr_fd = -1;
}

static void close_progress_fd(DownloadTask *task) {
    if (task && task->stderr_fd >= 0) {
        close(task->stderr_fd);
        task->stderr_fd = -1;
    }
}

void download_task_reset(DownloadTask *task) {
    if (!task) {
        return;
    }

    close_progress_fd(task);
    memset(task, 0, sizeof(*task));
    task->stderr_fd = -1;
}

static void update_progress_from_text(DownloadTask *task, const char *chunk) {
    const char *p;

    if (!task || !chunk) {
        return;
    }

    p = chunk;
    while (*p) {
        if ((*p >= '0' && *p <= '9') || *p == '.') {
            const char *start = p;
            char *endptr;
            double value = strtod(start, &endptr);

            if (endptr > start && *endptr == '%' && value >= 0.0 && value <= 100.0) {
                int percent = (int)value;
                if (percent > task->progress_percent) {
                    task->progress_percent = percent;
                    task->progress_changed = 1;
                    snprintf(task->status, sizeof(task->status), "Downloading %s (%d%%)", task->label, percent);
                }
                p = endptr + 1;
                continue;
            }
        }
        p++;
    }
}

static void read_progress_pipe(DownloadTask *task) {
    char buffer[256];
    ssize_t bytes_read;

    if (!task || task->stderr_fd < 0) {
        return;
    }

    while ((bytes_read = read(task->stderr_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        update_progress_from_text(task, buffer);

        if (task->progress_text_len + (size_t)bytes_read >= sizeof(task->progress_text)) {
            size_t keep = sizeof(task->progress_text) / 2;
            memmove(task->progress_text, task->progress_text + task->progress_text_len - keep, keep);
            task->progress_text_len = keep;
        }

        memcpy(task->progress_text + task->progress_text_len, buffer, (size_t)bytes_read + 1);
        task->progress_text_len += (size_t)bytes_read;
    }

    if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        close_progress_fd(task);
    }
}

int download_task_start(DownloadTask *task, const char *url, const char *output_path, const char *label, char *error, size_t error_size) {
    int pipefd[2];
    pid_t pid;

    if (!task || !url || !output_path || !label) {
        set_error(error, error_size, "Invalid download parameters");
        return 0;
    }

    if (task->active) {
        set_error(error, error_size, "A download is already in progress");
        return 0;
    }

    if (pipe(pipefd) != 0) {
        set_error(error, error_size, "Unable to create download progress pipe");
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        set_error(error, error_size, "Unable to start download process");
        return 0;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        char *quoted_url = shell_quote(url);
        char *quoted_path = shell_quote(output_path);
        char command[PATH_MAX * 2];

        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        if (!quoted_url || !quoted_path) {
            _exit(1);
        }
        snprintf(command, sizeof(command), "curl -L --fail --progress-bar %s -o %s", quoted_url, quoted_path);
        free(quoted_url);
        free(quoted_path);
        execl("/bin/sh", "sh", "-lc", command, (char *)NULL);
        _exit(1);
    }

    close(pipefd[1]);
    fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL, 0) | O_NONBLOCK);

    download_task_reset(task);
    task->pid = pid;
    task->stderr_fd = pipefd[0];
    task->active = 1;
    task->progress_percent = 0;
    snprintf(task->url, sizeof(task->url), "%s", url);
    snprintf(task->output_path, sizeof(task->output_path), "%s", output_path);
    snprintf(task->label, sizeof(task->label), "%s", label);
    snprintf(task->status, sizeof(task->status), "Downloading %s (0%%)", label);
    set_error(error, error_size, NULL);
    return 1;
}

void download_task_poll(DownloadTask *task) {
    int status;
    pid_t rc;

    if (!task || !task->active) {
        return;
    }

    task->progress_changed = 0;
    read_progress_pipe(task);

    rc = waitpid(task->pid, &status, WNOHANG);
    if (rc == 0 || rc < 0) {
        return;
    }

    close_progress_fd(task);
    task->active = 0;
    task->completed = 1;
    task->success = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    if (task->success) {
        task->progress_percent = 100;
        snprintf(task->status, sizeof(task->status), "Download completed for %s (100%%)", task->label);
    } else {
        if (task->progress_text[0]) {
            char *tail = task->progress_text + strlen(task->progress_text);
            while (tail > task->progress_text && tail[-1] != '\n' && tail[-1] != '\r') {
                tail--;
            }
            snprintf(task->status, sizeof(task->status), "%s", tail[0] ? tail : task->progress_text);
        } else {
            snprintf(task->status, sizeof(task->status), "Download failed for %s", task->label);
        }
    }
}

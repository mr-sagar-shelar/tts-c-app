#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

#include <limits.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    int stderr_fd;
    int active;
    int completed;
    int success;
    int progress_percent;
    int progress_changed;
    char url[PATH_MAX];
    char output_path[PATH_MAX];
    char label[128];
    char status[256];
    char progress_text[512];
    size_t progress_text_len;
} DownloadTask;

void download_task_init(DownloadTask *task);
int download_task_start(DownloadTask *task, const char *url, const char *output_path, const char *label, char *error, size_t error_size);
void download_task_poll(DownloadTask *task);
void download_task_reset(DownloadTask *task);

#endif /* DOWNLOAD_MANAGER_H */

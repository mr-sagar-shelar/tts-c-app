#include "app_logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char *app_logger_path(void) {
    static char path[256];
    static int initialized = 0;

    if (!initialized) {
        if (access("/tmp", W_OK) == 0) {
            snprintf(path, sizeof(path), "%s", "/tmp/sai-app.log");
        } else {
            mkdir(".sai-dev", 0777);
            snprintf(path, sizeof(path), "%s", ".sai-dev/sai-app.log");
        }
        initialized = 1;
    }

    return path;
}

void app_log_message(const char *component, const char *format, ...) {
    FILE *file;
    time_t now;
    struct tm *tm_info;
    char timestamp[32];
    va_list args;

    file = fopen(app_logger_path(), "a");
    if (!file) {
        return;
    }

    now = time(NULL);
    tm_info = localtime(&now);
    if (tm_info) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(timestamp, sizeof(timestamp), "%s", "unknown-time");
    }

    fprintf(file, "%s [%s] ", timestamp, component ? component : "app");
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

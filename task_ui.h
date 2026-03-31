#ifndef TASK_UI_H
#define TASK_UI_H

#include <stddef.h>

typedef char *(*TaskTextLoader)(const char *path, char *error, size_t error_size);

char *run_text_task_with_progress_ui(const char *title,
                                     const char *label,
                                     const char *path,
                                     TaskTextLoader loader,
                                     char *error,
                                     size_t error_size);

#endif /* TASK_UI_H */

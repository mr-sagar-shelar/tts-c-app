#ifndef DOWNLOAD_UI_H
#define DOWNLOAD_UI_H

#include <stddef.h>

int download_file_with_progress_ui(const char *title,
                                   const char *url,
                                   const char *output_path,
                                   const char *label,
                                   char *error,
                                   size_t error_size);

int upload_file_with_progress_ui(const char *title,
                                 const char *url,
                                 const char *input_path,
                                 const char *label,
                                 char *error,
                                 size_t error_size);

char *fetch_text_with_progress_ui(const char *title,
                                  const char *url,
                                  const char *label,
                                  char *error,
                                  size_t error_size);

#endif /* DOWNLOAD_UI_H */

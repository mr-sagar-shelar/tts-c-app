#ifndef DOCUMENT_READER_H
#define DOCUMENT_READER_H

#include <stddef.h>

int document_is_supported_file(const char *path);
char *document_load_text(const char *path, char *error, size_t error_size);
char *document_load_text_with_progress(const char *path, char *error, size_t error_size);

#endif /* DOCUMENT_READER_H */

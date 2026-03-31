#ifndef VOICE_LIBRARY_H
#define VOICE_LIBRARY_H

#include <stddef.h>

#define VOICE_LIBRARY_HINDI_FILENAME "cmu_indic_hin_ab.flitevox"

void voice_library_show_menu(void);
int voice_library_voice_exists(const char *filename);
void voice_library_get_voice_path(const char *filename, char *buffer, size_t buffer_size);
int voice_library_download_voice(const char *filename, const char *title, char *error, size_t error_size);

#endif /* VOICE_LIBRARY_H */

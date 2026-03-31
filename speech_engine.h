#ifndef SPEECH_ENGINE_H
#define SPEECH_ENGINE_H

#include <stddef.h>

int speech_engine_is_available(void);
int speech_engine_is_enabled(void);
int speech_engine_speak_text(const char *text, char *error, size_t error_size);
int speech_engine_export_text_to_wave(const char *text, const char *output_path, char *error, size_t error_size);
void speech_engine_shutdown(void);

#endif /* SPEECH_ENGINE_H */

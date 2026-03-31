#ifndef SPEECH_ENGINE_H
#define SPEECH_ENGINE_H

#include <stddef.h>

int speech_engine_is_available(void);
int speech_engine_is_enabled(void);
int speech_engine_speak_text(const char *text, char *error, size_t error_size);
void speech_engine_shutdown(void);

#endif /* SPEECH_ENGINE_H */

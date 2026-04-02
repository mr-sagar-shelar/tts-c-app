#ifndef SPEECH_ENGINE_H
#define SPEECH_ENGINE_H

#include <stddef.h>

typedef void (*SpeechEngineProgressCallback)(int token_index, void *userdata);
typedef int (*SpeechEngineInterruptCallback)(void *userdata);

int speech_engine_is_available(void);
int speech_engine_is_enabled(void);
int speech_engine_startup(char *error, size_t error_size);
void speech_engine_reload_settings(void);
void speech_engine_set_progress_callback(SpeechEngineProgressCallback callback, void *userdata);
void speech_engine_set_interrupt_callback(SpeechEngineInterruptCallback callback, void *userdata);
int speech_engine_take_interrupt_key(void);
int speech_engine_speak_text(const char *text, char *error, size_t error_size);
int speech_engine_speak_text_buffered(const char *text, char *error, size_t error_size);
int speech_engine_export_text_to_wave(const char *text, const char *output_path, char *error, size_t error_size);
void speech_engine_shutdown(void);

#endif /* SPEECH_ENGINE_H */

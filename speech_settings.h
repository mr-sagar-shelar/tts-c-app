#ifndef SPEECH_SETTINGS_H
#define SPEECH_SETTINGS_H

#include <stddef.h>

typedef enum {
    TTS_VOICE_KAL = 0,
    TTS_VOICE_SLT,
    TTS_VOICE_RMS,
    TTS_VOICE_AWB
} TtsVoiceOption;

typedef enum {
    TTS_VOLUME_LOW = 0,
    TTS_VOLUME_MEDIUM,
    TTS_VOLUME_HIGH,
    TTS_VOLUME_MAX
} TtsVolumeLevel;

typedef enum {
    SPEECH_MODE_OFF = 0,
    SPEECH_MODE_ON
} SpeechMode;

int handle_speech_setting_selection(const char *menu_key, char *message, size_t message_size);
char *speech_settings_get_selected_label(const char *menu_key);

#endif /* SPEECH_SETTINGS_H */

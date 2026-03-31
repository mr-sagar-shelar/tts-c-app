#include "speech_settings.h"

#include <stdio.h>
#include <string.h>

#include "config.h"

typedef struct {
    const char *menu_key;
    const char *config_key;
    const char *config_value;
    const char *label;
    const char *message_prefix;
} SpeechSettingOption;

static const SpeechSettingOption voice_options[] = {
    {"voice_kal", "tts_voice", "kal", "Kal", "Voice"},
    {"voice_slt", "tts_voice", "slt", "Slt", "Voice"},
    {"voice_rms", "tts_voice", "rms", "Rms", "Voice"},
    {"voice_awb", "tts_voice", "awb", "Awb", "Voice"}
};

static const SpeechSettingOption volume_options[] = {
    {"volume_low", "tts_volume", "low", "Low", "Volume"},
    {"volume_medium", "tts_volume", "medium", "Medium", "Volume"},
    {"volume_high", "tts_volume", "high", "High", "Volume"},
    {"volume_max", "tts_volume", "max", "Max", "Volume"}
};

static const SpeechSettingOption speech_mode_options[] = {
    {"speech_mode_off", "speech_mode", "off", "Off", "Speech mode"},
    {"speech_mode_on", "speech_mode", "on", "On", "Speech mode"}
};

static int apply_option_group(const SpeechSettingOption *options, size_t count, const char *menu_key, char *message, size_t message_size) {
    size_t i;

    for (i = 0; i < count; i++) {
        if (strcmp(options[i].menu_key, menu_key) == 0) {
            save_setting(options[i].config_key, options[i].config_value);
            if (message && message_size > 0) {
                snprintf(message, message_size, "%s set to %s.", options[i].message_prefix, options[i].label);
            }
            return 1;
        }
    }

    return 0;
}

int handle_speech_setting_selection(const char *menu_key, char *message, size_t message_size) {
    if (apply_option_group(voice_options, sizeof(voice_options) / sizeof(voice_options[0]), menu_key, message, message_size)) {
        return 1;
    }
    if (apply_option_group(volume_options, sizeof(volume_options) / sizeof(volume_options[0]), menu_key, message, message_size)) {
        return 1;
    }
    if (apply_option_group(speech_mode_options, sizeof(speech_mode_options) / sizeof(speech_mode_options[0]), menu_key, message, message_size)) {
        return 1;
    }

    return 0;
}

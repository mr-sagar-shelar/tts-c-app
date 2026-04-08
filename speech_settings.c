#include "speech_settings.h"

#include <stdio.h>
#include <stdlib.h>
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

static const SpeechSettingOption speech_mode_options[] = {
    {"speech_mode_off", "speech_mode", "off", "Off", "Speech mode"},
    {"speech_mode_on", "speech_mode", "on", "On", "Speech mode"}
};

static const SpeechSettingOption speech_speed_options[] = {
    {"speech_speed_very_slow", "tts_speed", "1.40", "Very Slow", "Speech speed"},
    {"speech_speed_slow", "tts_speed", "1.20", "Slow", "Speech speed"},
    {"speech_speed_normal", "tts_speed", "1.0", "Normal", "Speech speed"},
    {"speech_speed_fast", "tts_speed", "0.85", "Fast", "Speech speed"},
    {"speech_speed_very_fast", "tts_speed", "0.70", "Very Fast", "Speech speed"}
};

static const SpeechSettingOption audio_playback_options[] = {
    {"audio_playback_off", "audio_playback", "off", "Off", "Audio playback"},
    {"audio_playback_on", "audio_playback", "on", "On", "Audio playback"}
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
    if (apply_option_group(speech_mode_options, sizeof(speech_mode_options) / sizeof(speech_mode_options[0]), menu_key, message, message_size)) {
        return 1;
    }
    if (apply_option_group(speech_speed_options, sizeof(speech_speed_options) / sizeof(speech_speed_options[0]), menu_key, message, message_size)) {
        return 1;
    }
    if (apply_option_group(audio_playback_options, sizeof(audio_playback_options) / sizeof(audio_playback_options[0]), menu_key, message, message_size)) {
        return 1;
    }

    return 0;
}

static char *lookup_selected_label(const SpeechSettingOption *options, size_t count, const char *config_key) {
    char *current_value;
    size_t i;

    current_value = get_setting(config_key);
    if (!current_value) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        if (strcmp(options[i].config_value, current_value) == 0) {
            char *label = strdup(options[i].label);
            free(current_value);
            return label;
        }
    }

    if (strstr(current_value, "cmu_indic_hin_ab.flitevox") != NULL) {
        free(current_value);
        return strdup("Hindi");
    }

    free(current_value);
    return NULL;
}

char *speech_settings_get_selected_label(const char *menu_key) {
    if (strcmp(menu_key, "voice_select") == 0) {
        return lookup_selected_label(voice_options, sizeof(voice_options) / sizeof(voice_options[0]), "tts_voice");
    }
    if (strcmp(menu_key, "toggle_speech_mode") == 0) {
        return lookup_selected_label(speech_mode_options, sizeof(speech_mode_options) / sizeof(speech_mode_options[0]), "speech_mode");
    }
    if (strcmp(menu_key, "speech_speed") == 0) {
        return lookup_selected_label(speech_speed_options, sizeof(speech_speed_options) / sizeof(speech_speed_options[0]), "tts_speed");
    }
    if (strcmp(menu_key, "audio_playback") == 0) {
        return lookup_selected_label(audio_playback_options, sizeof(audio_playback_options) / sizeof(audio_playback_options[0]), "audio_playback");
    }

    return NULL;
}

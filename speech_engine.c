#include "speech_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#ifdef HAVE_FLITE
#include "flite.h"

void usenglish_init(cst_voice *v);
cst_lexicon *cmulex_init(void);
void cmu_indic_lang_init(cst_voice *v);
cst_lexicon *cmu_indic_lex_init(void);
cst_voice *register_cmu_us_kal(const char *voxdir);
cst_voice *register_cmu_us_slt(const char *voxdir);
cst_voice *register_cmu_us_rms(const char *voxdir);
cst_voice *register_cmu_us_awb(const char *voxdir);

typedef struct {
    int initialized;
    cst_voice *voice;
    cst_audio_streaming_info *asi;
    cst_audiodev *audio_device;
    float gain;
} SpeechEngineState;

static SpeechEngineState speech_state = {0};

static void set_error(char *error, size_t error_size, const char *message) {
    if (error && error_size > 0) {
        snprintf(error, error_size, "%s", message ? message : "Unknown speech error");
    }
}

static void speech_engine_close_device(void) {
    if (speech_state.audio_device) {
        audio_close(speech_state.audio_device);
        speech_state.audio_device = NULL;
    }
}

static void speech_engine_apply_voice(cst_voice *voice) {
    if (!voice || !speech_state.asi) {
        return;
    }

    if (speech_state.voice == voice &&
        feat_present(speech_state.voice->features, "streaming_info")) {
        return;
    }

    speech_engine_close_device();
    speech_state.voice = voice;
    feat_set(speech_state.voice->features, "streaming_info", audio_streaming_info_val(speech_state.asi));
}

static float current_gain_from_settings(void) {
    char *volume = get_setting("tts_volume");
    float gain = 1.0f;

    if (volume) {
        if (strcmp(volume, "low") == 0) {
            gain = 0.65f;
        } else if (strcmp(volume, "medium") == 0) {
            gain = 1.0f;
        } else if (strcmp(volume, "high") == 0) {
            gain = 1.45f;
        } else if (strcmp(volume, "max") == 0) {
            gain = 1.9f;
        }
        free(volume);
    }

    return gain;
}

static int speech_stream_chunk(const cst_wave *w, int start, int size, int last, cst_audio_streaming_info *asi) {
    SpeechEngineState *state = (SpeechEngineState *)asi->userdata;
    short *buffer;
    int i;

    if (!state) {
        return CST_AUDIO_STREAM_STOP;
    }

    if (state->audio_device == NULL) {
        state->audio_device = audio_open(w->sample_rate, w->num_channels, CST_AUDIO_LINEAR16);
        if (state->audio_device == NULL) {
            return CST_AUDIO_STREAM_STOP;
        }
    }

    buffer = (short *)malloc((size_t)size * sizeof(short));
    if (!buffer) {
        if (last && state->audio_device) {
            audio_close(state->audio_device);
            state->audio_device = NULL;
        }
        return CST_AUDIO_STREAM_STOP;
    }

    for (i = 0; i < size; i++) {
        float scaled = (float)w->samples[start + i] * state->gain;
        if (scaled > 32767.0f) {
            scaled = 32767.0f;
        } else if (scaled < -32768.0f) {
            scaled = -32768.0f;
        }
        buffer[i] = (short)scaled;
    }

    audio_write(state->audio_device, buffer, size * (int)sizeof(short));
    free(buffer);

    if (last && state->audio_device) {
        speech_engine_close_device();
    }

    return CST_AUDIO_STREAM_CONT;
}

static int speech_engine_init(char *error, size_t error_size) {
    char *voice_name;
    cst_voice *selected_voice;

    if (speech_state.initialized) {
        return 1;
    }

    flite_init();
    flite_add_lang("eng", usenglish_init, cmulex_init);
    flite_add_lang("usenglish", usenglish_init, cmulex_init);
    flite_add_lang("cmu_indic_lang", cmu_indic_lang_init, cmu_indic_lex_init);

    if (flite_voice_list == NULL) {
        flite_voice_list = cons_val(voice_val(register_cmu_us_awb(NULL)), flite_voice_list);
        flite_voice_list = cons_val(voice_val(register_cmu_us_rms(NULL)), flite_voice_list);
        flite_voice_list = cons_val(voice_val(register_cmu_us_slt(NULL)), flite_voice_list);
        flite_voice_list = cons_val(voice_val(register_cmu_us_kal(NULL)), flite_voice_list);
    }

    voice_name = get_setting("tts_voice");
    if (!voice_name) {
        voice_name = strdup("kal");
    }

    selected_voice = flite_voice_select(voice_name);
    if (!selected_voice) {
        selected_voice = flite_voice_select("kal");
    }
    free(voice_name);

    if (!selected_voice) {
        set_error(error, error_size, "Unable to load Flite voice");
        return 0;
    }

    speech_state.asi = new_audio_streaming_info();
    if (!speech_state.asi) {
        set_error(error, error_size, "Unable to create Flite streaming context");
        return 0;
    }

    speech_state.asi->asc = speech_stream_chunk;
    speech_state.asi->userdata = &speech_state;
    speech_state.gain = current_gain_from_settings();
    speech_engine_apply_voice(selected_voice);
    speech_state.initialized = 1;

    return 1;
}

static void speech_engine_refresh_settings(void) {
    char *voice_name;
    cst_voice *selected_voice;

    speech_state.gain = current_gain_from_settings();

    voice_name = get_setting("tts_voice");
    if (!voice_name) {
        voice_name = strdup("kal");
    }

    selected_voice = flite_voice_select(voice_name);
    if (selected_voice) {
        speech_engine_apply_voice(selected_voice);
    }
    free(voice_name);
}

int speech_engine_startup(char *error, size_t error_size) {
    if (!speech_engine_init(error, error_size)) {
        return 0;
    }

    speech_engine_refresh_settings();
    return 1;
}

int speech_engine_is_available(void) {
    char error[64];
    return speech_engine_startup(error, sizeof(error));
}

int speech_engine_is_enabled(void) {
    char *speech_mode = get_setting("speech_mode");
    int enabled = 1;

    if (speech_mode) {
        enabled = strcmp(speech_mode, "off") != 0;
        free(speech_mode);
    }

    return enabled;
}

int speech_engine_speak_text(const char *text, char *error, size_t error_size) {
    if (!text || !text[0]) {
        set_error(error, error_size, "No text available for speech");
        return 0;
    }

    if (!speech_engine_init(error, error_size)) {
        return 0;
    }

    if (!speech_engine_is_enabled()) {
        set_error(error, error_size, "Speech mode is off");
        return 0;
    }

    speech_engine_refresh_settings();

    if (flite_text_to_speech(text, speech_state.voice, "stream") < 0.0f) {
        set_error(error, error_size, "Flite synthesis failed");
        return 0;
    }

    return 1;
}

int speech_engine_export_text_to_wave(const char *text, const char *output_path, char *error, size_t error_size) {
    cst_wave *wave;

    if (!text || !text[0] || !output_path) {
        set_error(error, error_size, "Nothing to export");
        return 0;
    }

    if (!speech_engine_init(error, error_size)) {
        return 0;
    }

    speech_engine_refresh_settings();
    feat_set_float(speech_state.voice->features, "duration_stretch", 1.0f);
    wave = flite_text_to_wave(text, speech_state.voice);
    if (!wave) {
        set_error(error, error_size, "Flite export synthesis failed");
        return 0;
    }

    if (cst_wave_save_riff(wave, output_path) != CST_OK_FORMAT) {
        delete_wave(wave);
        set_error(error, error_size, "Unable to write WAV file");
        return 0;
    }

    delete_wave(wave);
    return 1;
}

void speech_engine_reload_settings(void) {
    if (speech_state.initialized) {
        speech_engine_refresh_settings();
    }
}

void speech_engine_shutdown(void) {
    speech_engine_close_device();
    memset(&speech_state, 0, sizeof(speech_state));
}

#else

int speech_engine_is_available(void) {
    return 0;
}

int speech_engine_is_enabled(void) {
    char *speech_mode = get_setting("speech_mode");
    int enabled = 1;

    if (speech_mode) {
        enabled = strcmp(speech_mode, "off") != 0;
        free(speech_mode);
    }

    return enabled;
}

int speech_engine_startup(char *error, size_t error_size) {
    set_error(error, error_size, "Flite support was not built into this binary");
    return 0;
}

void speech_engine_reload_settings(void) {
}

int speech_engine_speak_text(const char *text, char *error, size_t error_size) {
    (void)text;
    set_error(error, error_size, "Flite support was not built into this binary");
    return 0;
}

int speech_engine_export_text_to_wave(const char *text, const char *output_path, char *error, size_t error_size) {
    (void)text;
    (void)output_path;
    set_error(error, error_size, "Flite support was not built into this binary");
    return 0;
}

void speech_engine_shutdown(void) {
}

#endif

#include "music.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SDL_AUDIO
#include <SDL.h>
#endif

#include "braille_ui.h"
#include "menu.h"
#include "menu_audio.h"
#include "ui_feedback.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    MUSIC_WAVE_SINE = 0,
    MUSIC_WAVE_TRIANGLE,
    MUSIC_WAVE_SQUARE,
    MUSIC_WAVE_SAW,
    MUSIC_WAVE_ORGAN,
    MUSIC_WAVE_PLUCK,
    MUSIC_WAVE_BELL,
    MUSIC_WAVE_REED,
    MUSIC_WAVE_BRASS,
    MUSIC_WAVE_STRINGS,
    MUSIC_WAVE_FLUTE
} MusicWaveform;

typedef struct {
    const char *key;
    const char *title;
    MusicWaveform waveform;
    float attack_s;
    float decay_s;
    float sustain_level;
    float release_s;
    float brightness;
    float vibrato_depth;
    float vibrato_rate;
} MusicInstrument;

typedef struct {
    char key;
    int semitone_offset;
    const char *label;
} MusicKeyBinding;

typedef struct {
#ifdef HAVE_SDL_AUDIO
    SDL_AudioDeviceID device;
    int audio_ready;
#endif
    int initialized;
} MusicAudioState;

static MusicAudioState music_audio_state = {0};

static const MusicInstrument music_instruments[] = {
    {"music_piano", "Acoustic Piano", MUSIC_WAVE_PLUCK, 0.004f, 0.12f, 0.45f, 0.18f, 0.65f, 0.000f, 0.0f},
    {"music_electric_piano", "Electric Piano", MUSIC_WAVE_TRIANGLE, 0.006f, 0.14f, 0.55f, 0.18f, 0.45f, 0.002f, 5.0f},
    {"music_organ", "Organ", MUSIC_WAVE_ORGAN, 0.003f, 0.05f, 0.85f, 0.12f, 0.70f, 0.001f, 5.5f},
    {"music_acoustic_guitar", "Acoustic Guitar", MUSIC_WAVE_PLUCK, 0.003f, 0.10f, 0.35f, 0.15f, 0.85f, 0.000f, 0.0f},
    {"music_bass_guitar", "Bass Guitar", MUSIC_WAVE_PLUCK, 0.004f, 0.11f, 0.42f, 0.18f, 0.35f, 0.000f, 0.0f},
    {"music_violin", "Violin", MUSIC_WAVE_STRINGS, 0.030f, 0.16f, 0.72f, 0.20f, 0.75f, 0.004f, 6.0f},
    {"music_cello", "Cello", MUSIC_WAVE_STRINGS, 0.035f, 0.18f, 0.76f, 0.22f, 0.45f, 0.003f, 5.0f},
    {"music_flute", "Flute", MUSIC_WAVE_FLUTE, 0.012f, 0.08f, 0.68f, 0.14f, 0.25f, 0.002f, 4.5f},
    {"music_clarinet", "Clarinet", MUSIC_WAVE_REED, 0.015f, 0.09f, 0.70f, 0.15f, 0.42f, 0.002f, 5.0f},
    {"music_trumpet", "Trumpet", MUSIC_WAVE_BRASS, 0.010f, 0.08f, 0.72f, 0.12f, 0.88f, 0.002f, 6.5f},
    {"music_saxophone", "Saxophone", MUSIC_WAVE_REED, 0.014f, 0.10f, 0.74f, 0.16f, 0.58f, 0.003f, 5.5f},
    {"music_harp", "Harp", MUSIC_WAVE_PLUCK, 0.002f, 0.09f, 0.28f, 0.14f, 0.78f, 0.000f, 0.0f},
    {"music_vibraphone", "Vibraphone", MUSIC_WAVE_BELL, 0.004f, 0.20f, 0.40f, 0.26f, 0.48f, 0.004f, 7.0f},
    {"music_bell", "Bell", MUSIC_WAVE_BELL, 0.001f, 0.28f, 0.24f, 0.28f, 0.92f, 0.000f, 0.0f},
    {"music_synth_lead", "Synth Lead", MUSIC_WAVE_SAW, 0.003f, 0.07f, 0.70f, 0.12f, 0.95f, 0.003f, 7.5f},
    {"music_harmonica", "Harmonica", MUSIC_WAVE_REED, 0.010f, 0.10f, 0.66f, 0.16f, 0.50f, 0.002f, 5.0f}
};

static const MusicKeyBinding music_key_bindings[] = {
    {'a', 0, "C"},
    {'w', 1, "C sharp"},
    {'s', 2, "D"},
    {'e', 3, "D sharp"},
    {'d', 4, "E"},
    {'f', 5, "F"},
    {'t', 6, "F sharp"},
    {'g', 7, "G"},
    {'y', 8, "G sharp"},
    {'h', 9, "A"},
    {'u', 10, "A sharp"},
    {'j', 11, "B"},
    {'k', 12, "High C"},
    {'o', 13, "High C sharp"},
    {'l', 14, "High D"},
    {'p', 15, "High D sharp"},
    {';', 16, "High E"},
    {'\'', 17, "High F"}
};

static const MusicInstrument *music_find_instrument(const char *instrument_key) {
    size_t i;

    for (i = 0; i < sizeof(music_instruments) / sizeof(music_instruments[0]); i++) {
        if (strcmp(music_instruments[i].key, instrument_key) == 0) {
            return &music_instruments[i];
        }
    }

    return NULL;
}

static float music_frequency_for_note(int octave, int semitone_offset) {
    int midi_note = 12 * (octave + 1) + semitone_offset;
    return 440.0f * powf(2.0f, ((float)midi_note - 69.0f) / 12.0f);
}

#ifdef HAVE_SDL_AUDIO
static int music_audio_ensure_device(void) {
    SDL_AudioSpec desired;

    if (music_audio_state.audio_ready && music_audio_state.device != 0) {
        return 1;
    }

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return 0;
    }

    SDL_zero(desired);
    desired.freq = 22050;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 1024;

    music_audio_state.device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
    if (music_audio_state.device == 0) {
        return 0;
    }

    music_audio_state.audio_ready = 1;
    music_audio_state.initialized = 1;
    return 1;
}

static void music_audio_shutdown_device(void) {
    if (music_audio_state.audio_ready && music_audio_state.device != 0) {
        SDL_ClearQueuedAudio(music_audio_state.device);
        SDL_CloseAudioDevice(music_audio_state.device);
        music_audio_state.device = 0;
        music_audio_state.audio_ready = 0;
    }
    if (SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}
#endif

static float music_wave_sample(const MusicInstrument *instrument, float phase, float time_s, float frequency_hz) {
    float sine = sinf(2.0f * (float)M_PI * phase);
    float value;

    switch (instrument->waveform) {
        case MUSIC_WAVE_SINE:
            value = sine;
            break;
        case MUSIC_WAVE_TRIANGLE:
            value = 2.0f * fabsf(2.0f * (phase - floorf(phase + 0.5f))) - 1.0f;
            break;
        case MUSIC_WAVE_SQUARE:
            value = sine >= 0.0f ? 1.0f : -1.0f;
            break;
        case MUSIC_WAVE_SAW:
            value = 2.0f * (phase - floorf(phase + 0.5f));
            break;
        case MUSIC_WAVE_ORGAN:
            value = 0.70f * sine +
                    0.22f * sinf(2.0f * (float)M_PI * phase * 2.0f) +
                    0.08f * sinf(2.0f * (float)M_PI * phase * 4.0f);
            break;
        case MUSIC_WAVE_PLUCK:
            value = (0.70f * sine +
                     0.20f * sinf(2.0f * (float)M_PI * phase * 2.0f) +
                     0.10f * sinf(2.0f * (float)M_PI * phase * 3.0f)) *
                    expf(-3.8f * time_s);
            break;
        case MUSIC_WAVE_BELL:
            value = 0.60f * sinf(2.0f * (float)M_PI * phase) +
                    0.28f * sinf(2.0f * (float)M_PI * phase * 2.7f) +
                    0.12f * sinf(2.0f * (float)M_PI * phase * 4.1f);
            value *= expf(-2.8f * time_s);
            break;
        case MUSIC_WAVE_REED:
            value = 0.58f * sine +
                    0.26f * (sine >= 0.0f ? 1.0f : -1.0f) +
                    0.16f * sinf(2.0f * (float)M_PI * phase * 3.0f);
            break;
        case MUSIC_WAVE_BRASS:
            value = 0.52f * sine +
                    0.24f * sinf(2.0f * (float)M_PI * phase * 2.0f) +
                    0.14f * sinf(2.0f * (float)M_PI * phase * 3.0f) +
                    0.10f * sinf(2.0f * (float)M_PI * phase * 5.0f);
            break;
        case MUSIC_WAVE_STRINGS:
            value = 0.56f * sine +
                    0.22f * sinf(2.0f * (float)M_PI * phase * 2.0f) +
                    0.14f * sinf(2.0f * (float)M_PI * phase * 3.0f) +
                    0.08f * sinf(2.0f * (float)M_PI * phase * 4.0f);
            break;
        case MUSIC_WAVE_FLUTE:
            value = 0.82f * sine +
                    0.12f * sinf(2.0f * (float)M_PI * phase * 2.0f) +
                    0.06f * sinf(2.0f * (float)M_PI * phase * 3.0f);
            break;
        default:
            value = sine;
            break;
    }

    if (instrument->brightness > 0.0f) {
        value += instrument->brightness * 0.08f * sinf(2.0f * (float)M_PI * phase * 6.0f);
    }

    (void)frequency_hz;
    return value;
}

static float music_adsr_envelope(const MusicInstrument *instrument, float time_s, float total_s) {
    float attack = instrument->attack_s > 0.001f ? instrument->attack_s : 0.001f;
    float decay = instrument->decay_s > 0.001f ? instrument->decay_s : 0.001f;
    float release = instrument->release_s > 0.001f ? instrument->release_s : 0.001f;
    float sustain_time = total_s - attack - decay - release;

    if (sustain_time < 0.0f) {
        sustain_time = 0.0f;
    }

    if (time_s < attack) {
        return time_s / attack;
    }
    time_s -= attack;

    if (time_s < decay) {
        return 1.0f - (1.0f - instrument->sustain_level) * (time_s / decay);
    }
    time_s -= decay;

    if (time_s < sustain_time) {
        return instrument->sustain_level;
    }
    time_s -= sustain_time;

    if (time_s < release) {
        return instrument->sustain_level * (1.0f - time_s / release);
    }

    return 0.0f;
}

static void music_play_note(const MusicInstrument *instrument, float frequency_hz) {
#ifdef HAVE_SDL_AUDIO
    const int sample_rate = 22050;
    const float duration_s = 0.42f;
    const int sample_count = (int)(duration_s * (float)sample_rate);
    Sint16 *samples;
    int i;

    if (!instrument) {
        return;
    }

    if (!music_audio_ensure_device()) {
        ui_feedback_play(UI_FEEDBACK_WARNING);
        return;
    }

    samples = (Sint16 *)malloc((size_t)sample_count * sizeof(Sint16));
    if (!samples) {
        ui_feedback_play(UI_FEEDBACK_WARNING);
        return;
    }

    for (i = 0; i < sample_count; i++) {
        float time_s = (float)i / (float)sample_rate;
        float freq = frequency_hz;
        float phase;
        float vibrato = 0.0f;
        float envelope;
        float value;

        if (instrument->vibrato_depth > 0.0f && instrument->vibrato_rate > 0.0f) {
            vibrato = sinf(2.0f * (float)M_PI * instrument->vibrato_rate * time_s) * instrument->vibrato_depth;
        }

        freq *= (1.0f + vibrato);
        phase = freq * time_s;
        envelope = music_adsr_envelope(instrument, time_s, duration_s);
        value = music_wave_sample(instrument, phase, time_s, frequency_hz) * envelope * 0.78f;

        if (value > 1.0f) {
            value = 1.0f;
        } else if (value < -1.0f) {
            value = -1.0f;
        }

        samples[i] = (Sint16)(value * 32767.0f);
    }

    SDL_ClearQueuedAudio(music_audio_state.device);
    SDL_QueueAudio(music_audio_state.device, samples, (Uint32)((size_t)sample_count * sizeof(Sint16)));
    SDL_PauseAudioDevice(music_audio_state.device, 0);
    free(samples);
#else
    (void)instrument;
    (void)frequency_hz;
    ui_feedback_play(UI_FEEDBACK_WARNING);
#endif
}

static void music_render_screen(const MusicInstrument *instrument, const char *status_line, int octave) {
    int i;

    printf("\033[H\033[J");
    print_memory_widget_line();
    printf("--- %s ---\n\n", instrument ? instrument->title : menu_translate("music", "Music"));
    printf("%s\n", menu_translate("music_screen_intro", "Use a standard computer-keyboard piano layout with white keys on A S D F G H J K L ; ' and black keys on W E T Y U O P."));
    printf(menu_translate("music_screen_octave", "Current octave: %d"), octave);
    printf("\n%s\n\n", menu_translate("music_screen_hint", "Use Z for lower octave, X for higher octave, and C to return to the default octave."));

    printf("%s\n", menu_translate("music_screen_white_keys", "White keys: A S D F G H J K L ; '"));
    printf("%s\n\n", menu_translate("music_screen_black_keys", "Black keys: W E T Y U O P"));

    for (i = 0; i < (int)(sizeof(music_key_bindings) / sizeof(music_key_bindings[0])); i++) {
        printf("%c -> %s", (char)toupper((unsigned char)music_key_bindings[i].key), music_key_bindings[i].label);
        if ((i + 1) % 3 == 0 || i + 1 == (int)(sizeof(music_key_bindings) / sizeof(music_key_bindings[0]))) {
            printf("\n");
        } else {
            printf("   ");
        }
    }

    printf("\n\n%s\n", menu_translate("music_screen_instruments_hint", "Press Esc to return. Press Enter to hear the instrument name again."));
    if (status_line && status_line[0]) {
        printf("%s\n", status_line);
    }
    braille_ui_print_status_line(status_line && status_line[0] ? status_line : (instrument ? instrument->title : ""));
    printf("%s\n", menu_translate("music_footer", "[A,S,D,F,G,H,J,K,L,;,': White | W,E,T,Y,U,O,P: Black | Z/X: Octave | C: Reset | Enter: Repeat | Esc: Back]"));
    fflush(stdout);
}

void music_ui_show_instrument(const char *instrument_key) {
    const MusicInstrument *instrument = music_find_instrument(instrument_key);
    char status_line[128];
    int last_key = -1;
    int octave = 4;

    if (!instrument) {
        printf("\033[H\033[J--- %s ---\n%s\n\n%s",
               menu_translate("music", "Music"),
               menu_translate("music_instrument_unavailable", "That instrument is not available."),
               menu_translate("ui_press_any_key_to_continue", "Press any key to continue..."));
        fflush(stdout);
        read_key();
        return;
    }

    snprintf(status_line, sizeof(status_line),
             menu_translate("music_status_ready", "Ready to play %s."),
             instrument->title);
    menu_audio_speak(instrument->title);

    while (1) {
        int i;
        int key;
        int matched_note = 0;

        music_render_screen(instrument, status_line, octave);
        key = read_key();

        if (key == KEY_ESC) {
            break;
        }

        if (key == KEY_ENTER) {
            menu_audio_speak(instrument->title);
            snprintf(status_line, sizeof(status_line),
                     menu_translate("music_status_repeated", "%s selected."),
                     instrument->title);
            continue;
        }

        if (tolower(key) == 'z') {
            if (octave > 2) {
                octave--;
                snprintf(status_line,
                         sizeof(status_line),
                         menu_translate("music_status_octave", "Octave changed to %d."),
                         octave);
            } else {
                ui_feedback_play(UI_FEEDBACK_WARNING);
            }
            continue;
        }

        if (tolower(key) == 'x') {
            if (octave < 7) {
                octave++;
                snprintf(status_line,
                         sizeof(status_line),
                         menu_translate("music_status_octave", "Octave changed to %d."),
                         octave);
            } else {
                ui_feedback_play(UI_FEEDBACK_WARNING);
            }
            continue;
        }

        if (tolower(key) == 'c') {
            octave = 4;
            snprintf(status_line,
                     sizeof(status_line),
                     menu_translate("music_status_octave_reset", "Octave reset to %d."),
                     octave);
            continue;
        }

        for (i = 0; i < (int)(sizeof(music_key_bindings) / sizeof(music_key_bindings[0])); i++) {
            if (tolower(key) == music_key_bindings[i].key) {
                float frequency = music_frequency_for_note(octave, music_key_bindings[i].semitone_offset);

                matched_note = 1;
                last_key = i;
                music_play_note(instrument, frequency);
                snprintf(status_line,
                         sizeof(status_line),
                         menu_translate("music_status_playing", "Playing %s in octave %d on %s."),
                         music_key_bindings[i].label,
                         octave,
                         instrument->title);
                break;
            }
        }

        if (!matched_note) {
            if (last_key >= 0) {
                snprintf(status_line,
                         sizeof(status_line),
                         menu_translate("music_status_keep_playing", "Use the piano keyboard layout to play %s."),
                         instrument->title);
            }
        }
    }

#ifdef HAVE_SDL_AUDIO
    music_audio_shutdown_device();
#endif
}

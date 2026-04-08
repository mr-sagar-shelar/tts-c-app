#include "ui_feedback.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_SDL_AUDIO
#include <SDL.h>
#endif

typedef struct {
    float frequency_hz;
    float duration_ms;
    float amplitude;
    float pause_ms;
    int repeat_count;
} UIFeedbackToneSpec;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const UIFeedbackToneSpec ui_feedback_specs[] = {
    {330.0f, 90.0f, 0.35f, 35.0f, 2},
    {440.0f, 80.0f, 0.30f, 20.0f, 1},
    {660.0f, 70.0f, 0.28f, 25.0f, 2}
};

#ifdef HAVE_SDL_AUDIO
static int ui_feedback_audio_ready = 0;

static int ui_feedback_ensure_audio(void) {
    if (ui_feedback_audio_ready) {
        return 1;
    }

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return 0;
    }

    ui_feedback_audio_ready = 1;
    return 1;
}

static void ui_feedback_append_silence(float *samples, int start_sample, int sample_count) {
    int i;

    for (i = 0; i < sample_count; i++) {
        samples[start_sample + i] = 0.0f;
    }
}

static void ui_feedback_append_tone(const UIFeedbackToneSpec *spec,
                                    float *samples,
                                    int start_sample,
                                    int sample_rate) {
    int tone_samples = (int)((spec->duration_ms / 1000.0f) * (float)sample_rate);
    int i;

    for (i = 0; i < tone_samples; i++) {
        float t = (float)i / (float)sample_rate;
        float envelope = 1.0f;

        if (i < sample_rate / 200) {
            envelope = (float)i / (float)(sample_rate / 200);
        } else if (tone_samples - i < sample_rate / 200) {
            envelope = (float)(tone_samples - i) / (float)(sample_rate / 200);
        }

        samples[start_sample + i] =
            sinf(2.0f * (float)M_PI * spec->frequency_hz * t) * spec->amplitude * envelope;
    }
}
#endif

void ui_feedback_init(void) {
#ifdef HAVE_SDL_AUDIO
    (void)ui_feedback_ensure_audio();
#endif
}

void ui_feedback_shutdown(void) {
#ifdef HAVE_SDL_AUDIO
    if (ui_feedback_audio_ready && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    ui_feedback_audio_ready = 0;
#endif
}

void ui_feedback_play(UIFeedbackSound sound) {
    const UIFeedbackToneSpec *spec;

    if (sound < 0 || sound >= (int)(sizeof(ui_feedback_specs) / sizeof(ui_feedback_specs[0]))) {
        return;
    }

    spec = &ui_feedback_specs[sound];

#ifdef HAVE_SDL_AUDIO
    {
        const int sample_rate = 22050;
        const int pause_samples = (int)((spec->pause_ms / 1000.0f) * (float)sample_rate);
        const int tone_samples = (int)((spec->duration_ms / 1000.0f) * (float)sample_rate);
        const int total_samples = (tone_samples * spec->repeat_count) +
                                  (pause_samples * (spec->repeat_count > 0 ? spec->repeat_count - 1 : 0));
        SDL_AudioSpec desired;
        SDL_AudioDeviceID device;
        float *float_samples;
        Sint16 *pcm_samples;
        int offset = 0;
        int i;

        if (!ui_feedback_ensure_audio() || total_samples <= 0) {
            fputc('\a', stdout);
            fflush(stdout);
            return;
        }

        float_samples = (float *)malloc((size_t)total_samples * sizeof(float));
        pcm_samples = (Sint16 *)malloc((size_t)total_samples * sizeof(Sint16));
        if (!float_samples || !pcm_samples) {
            free(float_samples);
            free(pcm_samples);
            fputc('\a', stdout);
            fflush(stdout);
            return;
        }

        for (i = 0; i < spec->repeat_count; i++) {
            ui_feedback_append_tone(spec, float_samples, offset, sample_rate);
            offset += tone_samples;
            if (i + 1 < spec->repeat_count) {
                ui_feedback_append_silence(float_samples, offset, pause_samples);
                offset += pause_samples;
            }
        }

        for (i = 0; i < total_samples; i++) {
            float value = float_samples[i];
            if (value > 1.0f) {
                value = 1.0f;
            } else if (value < -1.0f) {
                value = -1.0f;
            }
            pcm_samples[i] = (Sint16)(value * 32767.0f);
        }

        SDL_zero(desired);
        desired.freq = sample_rate;
        desired.format = AUDIO_S16SYS;
        desired.channels = 1;
        desired.samples = 1024;

        device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
        if (device == 0) {
            free(float_samples);
            free(pcm_samples);
            fputc('\a', stdout);
            fflush(stdout);
            return;
        }

        SDL_ClearQueuedAudio(device);
        SDL_QueueAudio(device, pcm_samples, (Uint32)((size_t)total_samples * sizeof(Sint16)));
        SDL_PauseAudioDevice(device, 0);

        while (SDL_GetQueuedAudioSize(device) > 0) {
            SDL_Delay(5);
        }

        SDL_CloseAudioDevice(device);
        free(float_samples);
        free(pcm_samples);
        return;
    }
#else
    fputc('\a', stdout);
    fflush(stdout);
#endif
}

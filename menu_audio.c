#include "menu_audio.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "speech_engine.h"

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int initialized;
    int running;
    int interrupt_requested;
    char *pending_text;
    int pending_force;
} MenuAudioState;

static MenuAudioState menu_audio_state = {0};

static int menu_audio_interrupt(void *userdata) {
    MenuAudioState *state = (MenuAudioState *)userdata;
    return state ? state->interrupt_requested : 0;
}

static int menu_audio_enabled_setting(void) {
    char *value = get_setting("audio_playback");
    int enabled = value && strcmp(value, "on") == 0;
    free(value);
    return enabled;
}

int menu_audio_is_enabled(void) {
    return menu_audio_enabled_setting() && speech_engine_is_enabled();
}

static void *menu_audio_thread_main(void *userdata) {
    MenuAudioState *state = (MenuAudioState *)userdata;

    while (1) {
        char *text = NULL;
        char error[128] = {0};

        pthread_mutex_lock(&state->mutex);
        while (state->running && !state->pending_text) {
            pthread_cond_wait(&state->cond, &state->mutex);
        }

        if (state->running && state->pending_text && !state->pending_force && !menu_audio_is_enabled()) {
                free(state->pending_text);
                state->pending_text = NULL;
                state->pending_force = 0;
                pthread_mutex_unlock(&state->mutex);
                continue;
        }

        if (!state->running) {
            pthread_mutex_unlock(&state->mutex);
            break;
        }

        text = state->pending_text;
        state->pending_text = NULL;
        state->pending_force = 0;
        state->interrupt_requested = 0;
        pthread_mutex_unlock(&state->mutex);

        speech_engine_set_progress_callback(NULL, NULL);
        speech_engine_set_interrupt_callback(menu_audio_interrupt, state);
        speech_engine_speak_text_buffered(text, error, sizeof(error));
        speech_engine_set_interrupt_callback(NULL, NULL);
        free(text);

        pthread_mutex_lock(&state->mutex);
        state->interrupt_requested = 0;
        pthread_mutex_unlock(&state->mutex);
    }

    return NULL;
}

void menu_audio_init(void) {
    MenuAudioState *state = &menu_audio_state;

    if (state->initialized) {
        return;
    }

    pthread_mutex_init(&state->mutex, NULL);
    pthread_cond_init(&state->cond, NULL);
    state->running = 1;
    state->initialized = 1;
    pthread_create(&state->thread, NULL, menu_audio_thread_main, state);
}

void menu_audio_shutdown(void) {
    MenuAudioState *state = &menu_audio_state;

    if (!state->initialized) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    state->running = 0;
    state->interrupt_requested = 1;
    free(state->pending_text);
    state->pending_text = NULL;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mutex);

    pthread_join(state->thread, NULL);
    pthread_cond_destroy(&state->cond);
    pthread_mutex_destroy(&state->mutex);
    memset(state, 0, sizeof(*state));
}

void menu_audio_request(const char *text) {
    MenuAudioState *state = &menu_audio_state;
    char *copy;

    if (!state->initialized || !text || !text[0]) {
        return;
    }

    copy = strdup(text);
    if (!copy) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    free(state->pending_text);
    state->pending_text = copy;
    state->pending_force = 0;
    state->interrupt_requested = 1;
    pthread_cond_signal(&state->cond);
    pthread_mutex_unlock(&state->mutex);
}

void menu_audio_speak(const char *text) {
    MenuAudioState *state = &menu_audio_state;
    char *copy;

    if (!state->initialized || !text || !text[0]) {
        return;
    }

    copy = strdup(text);
    if (!copy) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    free(state->pending_text);
    state->pending_text = copy;
    state->pending_force = 1;
    state->interrupt_requested = 1;
    pthread_cond_signal(&state->cond);
    pthread_mutex_unlock(&state->mutex);
}

void menu_audio_stop(void) {
    MenuAudioState *state = &menu_audio_state;

    if (!state->initialized) {
        return;
    }

    pthread_mutex_lock(&state->mutex);
    free(state->pending_text);
    state->pending_text = NULL;
    state->pending_force = 0;
    state->interrupt_requested = 1;
    pthread_cond_signal(&state->cond);
    pthread_mutex_unlock(&state->mutex);
}

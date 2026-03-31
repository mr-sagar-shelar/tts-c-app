#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "radio.h"
#include "utils.h"
#include "cJSON.h"
#include "download_ui.h"

// For Reference:
// https://jonasrmichel.github.io/radio-garden-openapi/
static RadioStation fallback_stations[] = {
    {"Chillout Lounge", "http://icecast.vrt.be/mnm_chill-high.mp3"},
    {"Global Radio (News)", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service"},
    {"Smooth Jazz", "http://jazz.stream.publicradio.org/jazz.mp3"},
    {"Hindi Hits", "http://ice4.as34763.net/2000_h_mp3_128"},
    {"Classic Rock", "http://stream.rockantenne.de/classic-perlen/stream/mp3"}
};

static int fallback_count = sizeof(fallback_stations) / sizeof(fallback_stations[0]);

static RadioStation *dynamic_stations = NULL;
static int dynamic_count = 0;

static void stop_playback() {
    // Kill any existing mpg123 process
    system("pkill -9 mpg123 > /dev/null 2>&1");
    system("pkill -9 mpv > /dev/null 2>&1");
}

static void play_station(RadioStation *station_list, int index) {
    stop_playback();
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mpg123 -q \"%s\" > /dev/null 2>&1 &", station_list[index].url);
    system(cmd);
}

static void fetch_radio_stations() {
    const char *url = "https://www.dummyradios.com";
    char error[256] = {0};
    char *data;

    if (dynamic_stations) {
        free(dynamic_stations);
        dynamic_stations = NULL;
        dynamic_count = 0;
    }

    data = fetch_text_with_progress_ui("Internet Radio", url, "radio station list", error, sizeof(error));
    if (data && data[0]) {
        cJSON *json = cJSON_Parse(data);
        if (json && cJSON_IsArray(json)) {
            dynamic_count = cJSON_GetArraySize(json);
            if (dynamic_count > 0) {
                dynamic_stations = (RadioStation *)malloc(dynamic_count * sizeof(RadioStation));
                for (int i = 0; i < dynamic_count; i++) {
                    cJSON *item = cJSON_GetArrayItem(json, i);
                    cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
                    cJSON *url_node = cJSON_GetObjectItemCaseSensitive(item, "url");
                    
                    memset(&dynamic_stations[i], 0, sizeof(RadioStation));
                    if (name && cJSON_IsString(name)) strncpy(dynamic_stations[i].name, name->valuestring, 127);
                    if (url_node && cJSON_IsString(url_node)) strncpy(dynamic_stations[i].url, url_node->valuestring, 255);
                }
            }
        }
        if (json) cJSON_Delete(json);
    }
    free(data);
}

void radio_ui_show_menu(void) {
    fetch_radio_stations();

    RadioStation *active_list = (dynamic_stations && dynamic_count > 0) ? dynamic_stations : fallback_stations;
    int active_count = (dynamic_stations && dynamic_count > 0) ? dynamic_count : fallback_count;

    int current_sel = 0;
    int is_playing = 0;
    int playing_idx = -1;

    while (1) {
        printf("\033[H\033[J--- Internet Radio ---\n");
        if (active_list == fallback_stations) {
            printf("(Note: Using fallback station list)\n\n");
        }

        int PAGE_SIZE = 10;
        int start_idx = (current_sel / PAGE_SIZE) * PAGE_SIZE;
        int end_idx = start_idx + PAGE_SIZE;
        if (end_idx > active_count) end_idx = active_count;

        for (int i = start_idx; i < end_idx; i++) {
            if (i == current_sel) {
                printf("> %s %s\n", active_list[i].name, (is_playing && playing_idx == i) ? "(Playing)" : "");
            } else {
                printf("  %s %s\n", active_list[i].name, (is_playing && playing_idx == i) ? "(Playing)" : "");
            }
        }

        printf("\n------------------------------------------\n");
        printf("[Enter: Play/Pause | N: Next | P: Previous | R: Refresh | Esc: Stop & Back]\n");
        if (is_playing) {
            printf("Currently Streaming: %s\n", active_list[playing_idx].name);
        }
        fflush(stdout);

        int key = read_key();
        if (key == KEY_ESC) {
            stop_playback();
            break;
        } else if (key == KEY_ENTER) {
            if (is_playing && playing_idx == current_sel) {
                stop_playback();
                is_playing = 0;
                playing_idx = -1;
            } else {
                play_station(active_list, current_sel);
                is_playing = 1;
                playing_idx = current_sel;
            }
        } else if (key == 'n' || key == 'N' || key == KEY_DOWN) {
            current_sel = (current_sel + 1) % active_count;
            if ((key == 'n' || key == 'N') && is_playing) {
                play_station(active_list, current_sel);
                playing_idx = current_sel;
            }
        } else if (key == 'p' || key == 'P' || key == KEY_UP) {
            current_sel = (current_sel + active_count - 1) % active_count;
            if ((key == 'p' || key == 'P') && is_playing) {
                play_station(active_list, current_sel);
                playing_idx = current_sel;
            }
        } else if (key == 'r' || key == 'R') {
            stop_playback();
            is_playing = 0;
            playing_idx = -1;
            fetch_radio_stations();
            active_list = (dynamic_stations && dynamic_count > 0) ? dynamic_stations : fallback_stations;
            active_count = (dynamic_stations && dynamic_count > 0) ? dynamic_count : fallback_count;
            current_sel = 0;
        }
    }

    if (dynamic_stations) {
        free(dynamic_stations);
        dynamic_stations = NULL;
        dynamic_count = 0;
    }
}
